// Moose includes
#include "OpenMCExecutioner.h"

registerMooseObject("OpenMCApp", OpenMCExecutioner);

template <>
InputParameters
validParams<OpenMCExecutioner>()
{
  InputParameters params = validParams<Transient>();
  params.addRequiredParam<std::string>(
      "variable", "Variable name to store the results of tally");
  params.addParam<std::string>("score_name", "heating-local", "Name of the OpenMC score we want to extract");
  params.addParam<double>("neutron_source", 1.0e20, "Strength of fusion neutron source in neutrons/s");
  params.addParam<int32_t>("tally_id", 1, "OpenMC tally ID to extract results from.");
  params.addParam<int32_t>("mesh_id", 1, "OpenMC mesh ID for which we are providing the MOAB interface");
  params.addParam<bool>("redirect_dagout", true, "Switch to control whether dagmc output is written to file or not");
  params.addParam<std::string>("dagmc_logname", "/dev/null", "File to which to redirect DagMC output");
  return params;
}

OpenMCExecutioner::OpenMCExecutioner(const InputParameters & parameters) :
  Transient(parameters),
  setProblemLocal(false),
  isInit(false),
  useUWUW(true),
  var_name(getParam<std::string>("variable")),
  score_name(getParam<std::string>("score_name")),
  source_strength(getParam<double>("neutron_source")),
  tally_id(getParam<int32_t>("tally_id")),
  mesh_id(getParam<int32_t>("mesh_id")),
  redirect_dagout(getParam<bool>("redirect_dagout")),
  dagmc_logname(getParam<std::string>("dagmc_logname"))
{
  // Units for heating are eV / source neutron
  // source_strength has units  neutron / s
  // convert to J / s
  scale_factor = source_strength * eVinJoules;
}

OpenMCExecutioner::~OpenMCExecutioner()
{
  // Close any open log files
  // To-do : what happens if exception is throw? Will the file still close safely?
  if(!dagmclog.is_open()){
    dagmclog.close();
  }
}

void
OpenMCExecutioner::execute()
{

  // Initialize here so it occurs after MultiApp transfers
  initialize();

  if(!run()) mooseError("Failed to run OpenMC");

}

void
OpenMCExecutioner::initialize()
{

  // Don't re-initialize, just update
  if(isInit){
    update();
    return;
  }

  if(!initMOAB()) mooseError("Failed to initialize MOAB");

  if(!initOpenMC()) mooseError("Failed to initialize OpenMC");

  isInit = true;
}

void
OpenMCExecutioner::update()
{
  // Don't need to do anything if this isn't inside a multiapp
  if(setProblemLocal) return;

  // Update MOAB - extract surfaces from temperature binning
  if(!moab().update()) mooseError("Failed to update MOAB");

  // Load new geometry into OpenMC and reinitialise cross sections
  if(!updateOpenMC()) mooseError("Failed to update OpenMC");
}

bool
OpenMCExecutioner::run()
{

  // Run the simulation
  openmc_err = openmc_run();
  if (openmc_err) return false;

  // Fetch the tallied results
  std::vector< std::vector< double > > results_by_mat;
  if(!getResults(results_by_mat)) return false;
  if(results_by_mat.empty()) return false;

  // Pass the results into moab user object
  // ( summed over materials for now )
  // TODO system for every material? or just remove material binning
  if(!moab().setSolution(var_name,results_by_mat.back(),scale_factor,true)){
    std::cerr<<"Failed to pass OpenMC results into MoabUserObject"<<std::endl;
    return false;
  }

  if(setProblemLocal){
    // If the problem belongs to this executioner, we need to set the output here
    try
      {
        _time = 1;
        _problem.outputStep(EXEC_FINAL);
      }
    catch(std::exception &e)
      {
        std::cerr<<"Failed to output results."<<std::endl;
        std::cerr<<e.what()<<std::endl;
        return false;
      }
  }

  return true;
}

MoabUserObject&
OpenMCExecutioner::moab()
{
  // TODO set this name as a required input param for executioner
  return feProblem().getUserObject<MoabUserObject>("moab");
}

bool
OpenMCExecutioner::initMOAB()
{

  try
    {

      if(!(feProblem().hasUserObject("moab")))
        throw std::logic_error("Could not find MoabUserObject with name 'moab'. Please check your input file.");

      // Fetch a named instance of MOAB user object
      MoabUserObject& moabUO = moab();

      // Check if the user object already has a problem, e.g. through a transfer, in which case don't pass one in
      if(!moabUO.hasProblem()){
        FEProblemBase& problem = feProblem();
        moabUO.setProblem(&feProblem());
        setProblemLocal=true;
      }

      moabUO.initMOAB();

    }
  catch(std::exception &e)
    {
      std::cerr<<e.what()<<std::endl;
      return false;
    }

  return true;

}

bool
OpenMCExecutioner::initOpenMC()
{

  // Check there is not an instance in memory already!
  if(openmc::model::moabPtrs.find(mesh_id)!=openmc::model::moabPtrs.end()){
    if(openmc::model::moabPtrs[mesh_id] != nullptr) return false;
  }

  // Set OpenMC's copy of MOAB
  openmc::model::moabPtrs[mesh_id] = moab().moabPtr;

  char * argv[] = {nullptr, nullptr};
  openmc_err = openmc_init(1, argv, &_communicator.get());
  if (openmc_err) return false;

  return initMaterials();

}

bool
OpenMCExecutioner::initMaterials()
{
  // Find out if we have a material library in dagmc file
  if(uwuwPtr == nullptr){
    uwuwPtr = std::make_unique<UWUW>("dagmc.h5m");
  }
  if (uwuwPtr->material_library.size() == 0) {
    useUWUW = false;
  }

  // Didn't find a material library in dagmc.h5m file
  if(!useUWUW) return initMatNames();
  else return true;
}

bool
OpenMCExecutioner::initMatNames()
{
  for (const auto& mat : openmc::model::materials) {
    std::string mat_name = mat->name_;
    int32_t id = mat->id_;
    if(mat_names_to_id.find(mat_name) !=mat_names_to_id.end()){
      std::cerr<<"More than one material found with name "
               <<mat_name
               <<". Please ensure materials have unique names."
               <<std::endl;
      return false;
    }
    mat_names_to_id[mat->name_] = id;
  }
  return true;
}

bool
OpenMCExecutioner::getMatID(moab::EntityHandle vol_handle, int& mat_id)
{

  // Fetch the string name from dagmc metadata
  std::string mat_name = dmdPtr->get_volume_property("material", vol_handle);
  if (mat_name.empty()) {
    return false;
  }
  // By convention names are lower case in openmc
  openmc::to_lower(mat_name);

  // Find name id for special case - void material
  if (mat_name == "void" || mat_name == "vacuum" || mat_name == "graveyard") {

    // If we found the graveyard, save handle
    if(mat_name == "graveyard"){
      graveyard = vol_handle;
    }

    mat_id = openmc::MATERIAL_VOID;
    return true;
  }


  // Find id for non-void mats
  if(useUWUW){
    // UWUW material library is indexed by a string of the form "mat:xxx/rho=xxx"
    std::string uwuw_mat = dmdPtr->volume_material_property_data_eh[vol_handle];
    if(uwuwPtr->material_library.count(uwuw_mat) != 0){
      mat_id = uwuwPtr->material_library[uwuw_mat].metadata["mat_number"].asInt();
    }
    else{
      std::cerr<<"Couldn't find material "<<uwuw_mat<<" in the UWUW material library."<<std::endl;
      return false;
    }
  }
  else if(mat_names_to_id.find(mat_name) !=mat_names_to_id.end()){
    mat_id = mat_names_to_id[mat_name];
  }
  else{
    std::cerr<<"Couldn't find material "<<mat_name<<std::endl;
    return false;
  }

  return true;

}

bool
OpenMCExecutioner::updateOpenMC()
{

  if(!resetOpenMC()){
    std::cerr<<"Failed to reset OpenMC"<<std::endl;
    return false;
  }

  if(!reloadDAGMC()){
    std::cerr<<"Failed to load data into DagMC"<<std::endl;
    return false;
  }

  if(!setupCells()){
    std::cerr<<"Failed to set up cells in OpenMC"<<std::endl;
    return false;
  }

  if(!setupSurfaces()){
    std::cerr<<"Failed to set up surfaces in OpenMC"<<std::endl;
    return false;
  }

  // Final OpenMC setup after geometry is updated.
  completeSetup();

  return true;
}

bool
OpenMCExecutioner::getResults(std::vector< std::vector< double > > &results_by_mat)
{

  // Get the tally index
  int32_t t_index(0);
  openmc_err = openmc_get_tally_index(tally_id,&t_index);
  if (openmc_err) return false;

  // Fetch a reference to the tally
  openmc::Tally& tally = *(openmc::model::tallies.at(t_index));

  // Fetch the index of the score we are interested in
  int heatScoreIndex=-1;

  // Fetch the number of tally scores
  size_t nScores = (tally.scores_).size();
  // Loop over score indices
  for(unsigned int iScore=0; iScore<nScores; iScore++){
    // Get the score name
    std::string name = tally.score_name(iScore);
    if(name == score_name){
      // Found the score we want
      heatScoreIndex = int(iScore);
      break;
    }
  }
  if(heatScoreIndex == -1){
    openmc::set_errmsg("Failed to find '"+score_name+"' score.");
    return false;
  }

  //Fetch a reference to the indices of the filters;
  std::map<int32_t, FilterInfo> filters_by_id;
  int32_t nFilterBins;
  int32_t meshFilter;
  int32_t matFilter;
  if(!setFilterInfo(tally,filters_by_id,meshFilter,matFilter,nFilterBins)){
    return false;
  }

  size_t nMats = filters_by_id[matFilter].nbins;
  size_t nMeshBins = filters_by_id[meshFilter].nbins;

  // Clear any previous data if there was any
  results_by_mat.clear();

  // Resize results vector
  results_by_mat.resize(nMats+1, std::vector<double>(nMeshBins,0.));

  // Get a reference to the tally results
  xt::xtensor<double, 3> & results = tally.results_;

  // Check shape
  if(nFilterBins != int32_t(results.shape()[0])){
    openmc::set_errmsg("Results shape is inconsistent with number of filter bins.");
    return false;
  }
  else if (nScores != results.shape()[1] ){
    openmc::set_errmsg("Results shape is inconsistent with number of scores.");
    return false;
  }
  else if (3 != results.shape()[2] ){
    openmc::set_errmsg("Results shape is inconsistent with expected tally values.");
    return false;
  }

  // Loop over results
  for(int32_t iresult=0; iresult<nFilterBins; iresult++){

    std::map<int32_t,int32_t> filter_id_to_bin_index;
    if(!decomposeIntoFilterBins(iresult,
                                filters_by_id,
                                filter_id_to_bin_index)){
      openmc::set_errmsg("Failed to decompose results index into filter indices");
      return false;
    }

    int32_t meshIndex = -1;
    int32_t matIndex = -1;
    if(filter_id_to_bin_index.find(meshFilter)!=filter_id_to_bin_index.end()){
      meshIndex = filter_id_to_bin_index[meshFilter];
    }
    if(filter_id_to_bin_index.find(matFilter)!=filter_id_to_bin_index.end()){
      matIndex = filter_id_to_bin_index[matFilter];
    }
    if(meshIndex == -1 || matIndex == -1){
      openmc::set_errmsg("Failed to set filter bin indices");
      return false;
    }

    // Get the heat score result.
    // Last index: 0-> internal placeholder, 1-> mean, 2-> stddev
    double result = results(iresult,heatScoreIndex,1);

    // Add to sum for this material
    (results_by_mat.at(matIndex)).at(meshIndex) += result;

    // Add to sum over all materials
    (results_by_mat.at(nMats)).at(meshIndex) += result;

  }

  return true;
}


// Params:
// In
// tally
// Out
// map of filter ids to FilterInfo
// id of mesh filter
// id of mat filter
// total number of filter bins in results array
bool
OpenMCExecutioner::setFilterInfo(openmc::Tally& tally,
                                 std::map<int32_t, FilterInfo>& filters_by_id,
                                 int32_t& meshFilter,
                                 int32_t& matFilter,
                                 int32_t& nFilterBins)
{

  meshFilter=-1;
  matFilter=-1;
  nFilterBins=-1;
  int32_t firstFilter=-1;

  const std::vector<int32_t> & tally_filter_indices = tally.filters();
  size_t nFilters = tally_filter_indices.size();

  //Iterate over filter indices
  //iFilter is the index in tally.filters
  for(size_t iFilter=0; iFilter<nFilters; iFilter++){

    FilterInfo f_info;

    // Get the global index
    f_info.index = tally_filter_indices.at(iFilter);

    // Fetch a reference to the filter
    openmc::Filter& filter = *openmc::model::tally_filters[f_info.index];

    // Number of filter bins
    f_info.nbins = size_t(filter.n_bins());

    // Unique ID code
    f_info.id = filter.id();

    // Type of filter
    f_info.type = filter.type();

    //Get the stride
    f_info.stride = tally.strides(iFilter);

    filters_by_id[f_info.id] = f_info;

    if(iFilter==0){
      firstFilter=f_info.id;
    }

    if(f_info.type=="mesh"){
      if(meshFilter ==-1) meshFilter = f_info.id;
      else{
        openmc::set_errmsg("Found more than one mesh filter");
        return false;
      }
    }
    else if(f_info.type=="material"){
      if(matFilter ==-1) matFilter = f_info.id;
      else{
        openmc::set_errmsg("Found more than one material filter");
        return false;
      }
    }

  }

  //Check we set everything
  if(firstFilter ==-1){
    openmc::set_errmsg("Failed to find first filter");
    return false;
  }
  else if(filters_by_id.empty()){
    openmc::set_errmsg("Failed to find filters");
    return false;
  }
  else if(meshFilter ==-1){
    openmc::set_errmsg("Failed to find mesh filter");
    return false;
  }
  else if(matFilter ==-1){
    openmc::set_errmsg("Failed to find material filter");
    return false;
  }


  nFilterBins = filters_by_id[firstFilter].stride*filters_by_id[firstFilter].nbins;
  if(nFilterBins != tally.n_filter_bins()){
    openmc::set_errmsg("Inconsistent number of filter bins.");
    return false;
  }

  // Done
  return true;

};

int32_t
OpenMCExecutioner::getFilterBin(int32_t iResultBin, const FilterInfo & filter)
{

  // iResultBin  = stride*nbins*(stuff...) + index*stride + remainder

  // Fetch the remainder of everything with smaller stride
  int32_t remainder = iResultBin % filter.stride;

  // Subtract remainer to get integer multiple of stride
  int32_t round_down = iResultBin - remainder;

  // Fetch the next stride
  int32_t next_stride = filter.stride * filter.nbins;

  // Modulo division to get index*stride
  int32_t next_remainder = round_down % next_stride;

  // Integer division by stride to get index
  int32_t filter_bin_index = next_remainder / filter.stride;

  // Sanity check
  if( filter_bin_index >= int32_t(filter.nbins)){
    return -1;
  }

  return filter_bin_index;
};

bool
OpenMCExecutioner::decomposeIntoFilterBins(int32_t iResultBin,
                                           const std::map<int32_t, FilterInfo>& filters_by_id,
                                           std::map<int32_t,int32_t>& filter_id_to_bin_index)
{

  // Clear any existing results
  filter_id_to_bin_index.clear();

  auto filter = filters_by_id.begin();
  auto filter_end = filters_by_id.end();
  for(; filter!= filter_end; ++filter){
    int32_t id = filter->first;
    int32_t bin_index = getFilterBin(iResultBin, filter->second);
    if(bin_index == -1){
      filter_id_to_bin_index.clear();
      return false;
    }
    filter_id_to_bin_index[id] = bin_index;
  }

  return true;
};


bool
OpenMCExecutioner::resetOpenMC()
{

  // Clear tallies from any previous calls
  openmc_err = openmc_reset();
  if (openmc_err) return false;


  // Clear nuclides, these will get reset in read_ce_cross_sections
  // Horrible circular logic means that clearing nuclides clears nuclide_map, but
  // which is needed before nuclides gets reset
  std::unordered_map<std::string, int> nuclide_map_copy = openmc::data::nuclide_map;
  openmc::data::nuclides.clear();
  openmc::data::nuclide_map = nuclide_map_copy;

  return true;
}


bool
OpenMCExecutioner::reloadDAGMC()
{
  moab::ErrorCode rval;

  // Backup streambuffer of std out if redirecting
  std::streambuf* stream_buffer_stdout;

  if(redirect_dagout){
    // Don't reopen log file
    if(!dagmclog.is_open()){
      if(dagmc_logname!=""){
        // Open our log file
        dagmclog.open(dagmc_logname, std::ios::out);
      }
      // Did we successfully open a file?
      if(!dagmclog.is_open()){
        std::cerr<<"Failed to open dagmc log file: "<< dagmc_logname<< std::endl;
        return false;
      }
      else{
        std::cout<<"Redirecting DagMC output to file: "<< dagmc_logname<< std::endl;
      }
    }
    // Check if any previous io operations failed
    if(dagmclog.bad()){
      std::cerr<<"Bad bit detected in fstream to dag log  file"<< dagmc_logname<< std::endl;
      return false;
    }

    // Backup streambuffer of std out
    stream_buffer_stdout = std::cout.rdbuf();

    // Get the streambuffer of the file
    std::streambuf* stream_buffer_file = dagmclog.rdbuf();

    // Redirect cout to file
    std::cout.rdbuf(stream_buffer_file);
  }

  // Delete old data
  openmc::free_memory_dagmc();

  // Create a new DagMC, but pass in our MOAB interface
  dagPtr = new moab::DagMC(moab().moabPtr);
  openmc::model::DAG = dagPtr;

  // Set up geometry in DagMC from already-loaded mesh
  rval = dagPtr->load_existing_contents();
  if(rval!= moab::MB_SUCCESS) return false;

  // Initialize acceleration data structures
  rval = dagPtr->init_OBBTree();
  if(rval!= moab::MB_SUCCESS) return false;

  // Parse model metadata
  dmdPtr = std::make_unique<dagmcMetaData>(dagPtr, false, false);
  dmdPtr->load_property_data();

  if(redirect_dagout){
    // Reset cout streambuffer
    std::cout.rdbuf(stream_buffer_stdout);
  }

  return true;
}

bool
OpenMCExecutioner::setupCells()
{

  // Clear existing cell data
  openmc::model::cells.clear();
  openmc::model::cell_map.clear();

  // Universe ID for DAGMC (always zero)
  int32_t dagmc_univ_id = 0;

  // Create universe if required
  auto it = openmc::model::universe_map.find(dagmc_univ_id);
  if (it == openmc::model::universe_map.end()) {
    openmc::model::universes.push_back(std::make_unique<openmc::Universe>());
    openmc::model::universes.back()->id_ = dagmc_univ_id;
    openmc::model::universe_map[dagmc_univ_id] = openmc::model::universes.size() - 1;
  }
  // Get reference to dagmc universe
  int32_t uID = openmc::model::universe_map[dagmc_univ_id];
  openmc::Universe& universe = *(openmc::model::universes.at(uID));

  // Clear prior universe cell data
  universe.cells_.clear();

  // Get number of volumes from DAGMC
  unsigned int n_cells = dagPtr->num_entities(DIM_VOL);

  // Loop over the cells
  for (unsigned int icell = 0; icell < n_cells; icell++) {

    // DagMC indices are offset by one (convention stemming from MCNP)
    unsigned int index = icell+1;

    // Create new cell
    openmc::DAGCell* cell = new openmc::DAGCell();
    if(!setCellAttrib(*cell,index,dagmc_univ_id)){
      delete cell;
      return false;
    }

    // Save cell
    openmc::model::cell_map[cell->id_] = icell;
    openmc::model::cells.emplace_back(cell);
    universe.cells_.push_back(icell);

  }

  // Allocate the cell overlap count if necessary
  if (openmc::settings::check_overlaps) {
    openmc::model::overlap_check_count.resize(openmc::model::cells.size(), 0);
  }

  if (!graveyard) {
    std::cerr<<"No graveyard volume found in the DagMC model."<<std::endl;
    return false;

  }

  return true;
}

bool
OpenMCExecutioner::setupSurfaces()
{

  // Clear existing surface data
  openmc::model::surfaces.clear();
  openmc::model::surface_map.clear();

  // Get number of surfaces from DAGMC
  unsigned int n_surfaces = dagPtr->num_entities(DIM_SURF);

  // Loop over the surfaces
  for (unsigned int iSurf = 0; iSurf < n_surfaces; iSurf++) {

    // DagMC indices are offset by one (convention stemming from MCNP)
    unsigned int index = iSurf+1;

    // Create new surface
    openmc::DAGSurface* surf = new openmc::DAGSurface();
    if(!setSurfAttrib(*surf,index)){
      delete surf;
      return false;
    }

    // Add to global array and map
    openmc::model::surface_map[surf->id_] = iSurf;
    openmc::model::surfaces.emplace_back(surf);
  }

  return true;
}

void
OpenMCExecutioner::completeSetup()
{

  openmc::model::root_universe = openmc::find_root_universe();

  // Copied code segment from openmc::read_input_xml()

  // Convert user IDs -> indices, assign temperatures
  openmc::double_2dvec nuc_temps(openmc::data::nuclide_map.size());
  openmc::double_2dvec thermal_temps(openmc::data::thermal_scatt_map.size());
  openmc::finalize_geometry(nuc_temps, thermal_temps);

  if (openmc::settings::run_mode != openmc::RunMode::PLOTTING) {
    openmc::simulation::time_read_xs.start();
    if (openmc::settings::run_CE) {
      // Read continuous-energy cross sections
      openmc::read_ce_cross_sections(nuc_temps, thermal_temps);
    } else {
      // Create material macroscopic data for MGXS
      openmc::set_mg_interface_nuclides_and_temps();
      openmc::data::mg.init();
      openmc::mark_fissionable_mgxs_materials();
    }
    openmc::simulation::time_read_xs.stop();
  }

}

bool
OpenMCExecutioner::setCellAttrib(openmc::DAGCell& cell,unsigned int index,int32_t universe_id)
{

  cell.dag_index_ = index;
  cell.id_ = dagPtr->id_by_index(DIM_VOL, index);
  cell.dagmc_ptr_ = dagPtr;
  cell.universe_ = universe_id;
  cell.fill_ = openmc::C_NONE;

  // Get the MOAB handle
  moab::EntityHandle vol_handle= dagPtr->entity_by_index(DIM_VOL, index);

  // Set the material ID
  int mat_id;
  if(getMatID(vol_handle, mat_id)){
    cell.material_.push_back(mat_id);
  }
  else{
    std::cerr<<"Could not set material for cell "<< cell.id_<<std::endl;
    return false;
  }

  // Set the cell temperature
  if (cell.material_[0] != openmc::MATERIAL_VOID){
    //Retrieve the binned temperature data
    double temp = moab().getTemperature(vol_handle);
    cell.sqrtkT_.push_back(std::sqrt(openmc::K_BOLTZMANN * temp));
  }

  return true;
}

bool
OpenMCExecutioner::setSurfAttrib(openmc::DAGSurface& surf,unsigned int index)
{
  moab::EntityHandle surf_handle = dagPtr->entity_by_index(DIM_SURF,index);

  surf.dag_index_ = index;
  surf.id_ = dagPtr->id_by_index(DIM_SURF, surf.dag_index_);
  surf.dagmc_ptr_ = dagPtr;

  // set BCs
  std::string bc_value = dmdPtr->get_surface_property("boundary", surf_handle);
  openmc::to_lower(bc_value);
  if (bc_value.empty() || bc_value == "transmit" || bc_value == "transmission") {
    // set to transmission by default
    surf.bc_ = openmc::Surface::BoundaryType::TRANSMIT;
  } else if (bc_value == "vacuum") {
    surf.bc_ = openmc::Surface::BoundaryType::VACUUM;
  } else if (bc_value == "reflective" || bc_value == "reflect" || bc_value == "reflecting") {
    surf.bc_ = openmc::Surface::BoundaryType::REFLECT;
  } else if (bc_value == "periodic") {
    std::cerr<<"Periodic boundary condition not supported in DAGMC."<<std::endl;
    return false;
  } else {
    std::cerr<<"Unknown boundary condition "<<bc_value
             <<" specified on surface "
             << surf.id_<<std::endl;
    return false;
  }

  // graveyard check
  moab::Range parent_vols;
  moab::ErrorCode rval = dagPtr->moab_instance()->get_parent_meshsets(surf_handle, parent_vols);
  if(rval!=moab::MB_SUCCESS) return false;

  // Check if this surface belongs to the graveyard
  if (graveyard && parent_vols.find(graveyard) != parent_vols.end()) {
    // Set graveyard surface's bcs to vacuum
    surf.bc_ = openmc::Surface::BoundaryType::VACUUM;
  }

  return true;
}
