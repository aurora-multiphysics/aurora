// Moose includes
#include "OpenMCExecutioner.h"

registerMooseObject("OpenMCApp", OpenMCExecutioner);


InputParameters
OpenMCExecutioner::validParams()
{
  InputParameters params = Transient::validParams();

  // Location for results
  params.addParam<std::vector<std::string>>("variables", std::vector<std::string>(1,"heating-local"),
                                            "List of aux variable names in which to store the means of scores");
  params.addParam<std::vector<std::string>>("err_variables", std::vector<std::string>() ,
                                            "Optional list of aux variable names in which to store the std deviation  of scores");
  params.addParam<bool>("add_variables", true,
                        "Switch to control whether to automatically add variables to FE problem if they don't exist already");

  // OpenMC data params
  params.addParam<std::vector<std::string>>("score_names", std::vector<std::string>(1,"heating-local"),
                                            "List of the OpenMC score names we want to extract");
  params.addParam<std::vector<int32_t>>("tally_ids", std::vector<int32_t>(),
                                            "List of OpenMC tally IDs from which to extract scores. Use -1 to auto-assign.");

  // Normalisation of source strength
  params.addParam<double>("neutron_source", 1.0e20, "Strength of fusion neutron source in neutrons/s");
  params.addParam<bool>("no_scaling", false,
                        "Optionally turn off scaling by neutron source strength and unit conversion factors (useful for direct comparison with openmc results)");

  // Run settings
  params.addParam<bool>("redirect_dagout", false, "Switch to control whether dagmc output is written to file or not");
  params.addParam<std::string>("dagmc_logname", "/dev/null", "File to which to redirect DagMC output");
  params.addParam<bool>("launch_threads", false, "Switch to control whether openmc should launch new child thread. NB Do not set true when MOOSE application is run iwth --n-threads > 0 !");
  params.addParam<unsigned int>("n_threads", 1, "Number of threads to use if launch_threads = true");
  return params;
}

OpenMCExecutioner::OpenMCExecutioner(const InputParameters & parameters) :
  Transient(parameters),
  dag_univ_idx(openmc::C_NONE),
  setProblemLocal(false),
  isInit(false),
  matsUpdated(false),
  updateDensity(false),
  useUWUW(true),
  source_strength(getParam<double>("neutron_source")),
  add_variables(getParam<bool>("add_variables")),
  launch_threads(getParam<bool>("launch_threads")),
  n_threads(getParam<unsigned int>("n_threads")),
  redirect_dagout(getParam<bool>("redirect_dagout")),
  dagmc_logname(getParam<std::string>("dagmc_logname")),
  _execute_timer(registerTimedSection("execute", 1)),
  _init_timer(registerTimedSection("init", 1)),
  _initmoab_timer(registerTimedSection("initmoab", 2)),
  _initopenmc_timer(registerTimedSection("initopenmc", 2)),
  _updateopenmc_timer(registerTimedSection("updateopenmc", 2)),
  _run_timer(registerTimedSection("run", 1)),
  _output_timer(registerTimedSection("output", 1))
{

  if(launch_threads && n_threads > 1){
    if(libMesh::n_threads()>1){
      mooseError("Do not run application with --n-threads and with OpenMCExecutioner setting launch_threads = true");
    }
  }

  initScoreData();
}

OpenMCExecutioner::~OpenMCExecutioner()
{
  // Close any open log files
  // To-do : what happens if exception is thrown?
  // Will the file still close safely?
  if(!dagmclog.is_open()){
    dagmclog.close();
  }
  openmc_finalize();
}

void
OpenMCExecutioner::execute()
{

  TIME_SECTION(_execute_timer);

  // Initialize here so it occurs after MultiApp transfers
  initialize();

  if(!run()) mooseError("Failed to run OpenMC");

  if(!processResults()) mooseError("Failed to process results");

  if(!output())  mooseError("Failed to output results.");

}

void
OpenMCExecutioner::initScoreData()
{
  std::vector<std::string> vars
    = getParam<std::vector<std::string>>("variables");
  std::vector<std::string> err_vars
    = getParam<std::vector<std::string>>("err_variables");
  std::vector<std::string> scores
    = getParam<std::vector<std::string>>("score_names");
  std::vector<int32_t> tally_ids
    = getParam<std::vector<int32_t>>("tally_ids");

  bool noScale = getParam<bool>("no_scaling");

  size_t nVars = vars.size();
  if(scores.size() != nVars){
    mooseError("Please ensure the number of variables and score_names provided match.");
  }
  if(tally_ids.empty()){
    tally_ids.resize(nVars,-1);
  }

  if(tally_ids.size() != nVars){
    mooseError("Please ensure the number of variables and tally_ids provided match.");
  }
  if(!err_vars.empty() && err_vars.size() != nVars){
    mooseError("If provided, please ensure the number of variables and error variables provided match.");
  }

  // Initialise tally / score information
  for(size_t iVar=0; iVar<nVars; iVar++){
    int32_t tally_id = tally_ids.at(iVar);
    std::string score_name = scores.at(iVar);
    std::string var_name = vars.at(iVar);
    std::string err_name = err_vars.empty() ? "" : err_vars.at(iVar);
    bool saveErr = (err_name != "");

    // Set scale factors
    double scale_factor = 1.0;
    if(!noScale){
      // Most scores are per source particle.
      // source_strength has units  neutron / s
      // Scale by source strength to get units of s^-1
      scale_factor *= source_strength;

      // Should we add support for more scores?
      if(score_name == "heating" || score_name== "heating-local"){
        // Units for heating are eV / source neutron
        // convert to J / s
        scale_factor *=  eVinJoules;
      }
    }

    ScoreData score = { score_name, var_name, err_name, saveErr, scale_factor, -1 };
    // First time we see this tally id, create an entry
    if(tally_ids_to_scores.find(tally_id)== tally_ids_to_scores.end()){
      tally_ids_to_scores[tally_id] = std::vector<ScoreData>();
    }
    // Add this score
    tally_ids_to_scores[tally_id].push_back(score);
  }
}

void
OpenMCExecutioner::initialize()
{

  TIME_SECTION(_init_timer);

  // Don't re-initialize, just update
  if(isInit){
    update();
    return;
  }

  if(!initMOAB()) mooseError("Failed to initialize MOAB");

  if(!initOpenMC()) mooseError("Failed to initialize OpenMC");

  if(!initMaterials()) mooseError("Failed to initialize material data");

  if(!initDAGUniverse()) mooseError("Failed to initialize DAGMC universe");

  if(!initMeshTallies()) mooseError("Failed to set up mesh filter tally");

  if(!updateOpenMC()) mooseError("Failed to update OpenMC during initialisation step");

  isInit = true;
}

void
OpenMCExecutioner::update()
{
  // Don't need to do anything if this isn't inside a multiapp
  if(setProblemLocal) return;

  // Update MOAB - extract surfaces from temperature binning
  moab().update();

  // Load new geometry into OpenMC and reinitialise cross sections
  if(!updateOpenMC()) mooseError("Failed to update OpenMC");
}

bool
OpenMCExecutioner::run()
{
  TIME_SECTION(_run_timer);
  // Run the simulation
  openmc_err = openmc_run();
  if (openmc_err) return false;
  else return true;
}

bool
OpenMCExecutioner::processResults()
{
  // Fetch the tallied results from openmc
  std::map<std::string,std::vector< double > > var_results_by_elem;
  if(!getResults(var_results_by_elem)) return false;

  // Pass results into FEProblem
  for(const auto & tally_scores : tally_ids_to_scores){
    for(const auto & score : tally_scores.second){
      if(!setSolution(var_results_by_elem[score.var_name],
                      score.var_name,
                      score.scale_factor,false)){
        return false;
      }
      if(score.saveErr){
        if(!setSolution(var_results_by_elem[score.err_name],
                        score.err_name,
                        score.scale_factor,true)){
          return false;
        }
      }
    }
  }

  return true;
}

bool
OpenMCExecutioner::output()
{
  if(setProblemLocal){
    // If the problem belongs to this executioner, we need to set the output here
    try
      {
        _time = 1;
        TIME_SECTION(_output_timer);
        _problem.execute(EXEC_FINAL);
        _problem.outputStep(EXEC_FINAL);
      }
    catch(std::exception &e)
      {
        return false;
      }
  }
  return true;
}

bool
OpenMCExecutioner::setSolution(std::vector< double > & results_by_elem,
                               std::string var_name,
                               double scale_factor,
                               bool isErr)
{
  if(results_by_elem.empty()) return false;

  bool normToVol = !getParam<bool>("no_scaling");

  // Pass the results into moab user object
  if(!moab().setSolution(var_name,results_by_elem,scale_factor,isErr,normToVol)){
    std::cerr<<"Failed to pass OpenMC results into MoabUserObject"<<std::endl;
    return false;
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

  TIME_SECTION(_initmoab_timer);

  try
    {
      if(!(feProblem().hasUserObject("moab")))
        throw std::logic_error("Could not find MoabUserObject with name 'moab'. Please check your input file.");

      // Fetch a named instance of MOAB user object
      MoabUserObject& moabUO = moab();

      // Check if the user object already has a problem, e.g. through a transfer, in which case don't pass one in
      if(!moabUO.hasProblem()){
        moabUO.setProblem(&feProblem());
        setProblemLocal=true;
        moabUO.initBinningData();
      }

      moabUO.update();

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

  TIME_SECTION(_initopenmc_timer);

  // Emulate a command line
  std::string args("dummy");
  if(launch_threads && n_threads > 1){
    args+=" -s "+std::to_string(n_threads);
  }

  // Convert string arguments to char array
  char * cstr = new char [args.length()+1];
  std::strcpy (cstr, args.c_str());

  // Split string by whitespace delimiter
  std::vector<char*> arg_list;
  char * next;
  next = strtok(cstr," ");
  while (next != NULL){
    arg_list.push_back(next);
    next = strtok(NULL, " ");
  }

  // Create array of char*
  size_t argc = arg_list.size();
  char** argv = new char*[argc];
  for (size_t i = 0; i < argc; i++){
    argv[i]=arg_list.at(i);
  }

  // Initialise openmc with the command line args and MOOSE MPI communicator
  openmc_err = openmc_init(argc, argv, &_communicator.get());
  if (openmc_err) return false;

  // Deallocate memory for C array created with new
  delete argv;
  delete cstr;

  return true;

}

bool
OpenMCExecutioner::initDAGUniverse()
{
  // Identify existing DAGUniverses
  std::vector<int32_t> dagmc_univ_ids;

  // Loop over universes and check if type is DAGMC
  for(const auto& universe: openmc::model::universes){
    if (universe->geom_type() == openmc::GeometryType::DAG ){
      dagmc_univ_ids.push_back(universe->id_);
    }
  }

  // We only expect one! How to handle this in future?
  if(dagmc_univ_ids.size() > 1){
    mooseError("Multiple DAGMC universes detected - can support only 1 at present.");
  }
  // Didn't find a DAGMC universe
  else if(dagmc_univ_ids.empty()){
    return false;
  }

  // Get a reference to the universe unique ptr
  dag_univ_idx = openmc::model::universe_map[dagmc_univ_ids.front()];
  auto& univ_ptr_ref = openmc::model::universes.at(dag_univ_idx);

  // Cast as DAGMC Universe pointer
  // (Use of raw pointer conversion because starting pointer is unique)
  openmc::DAGUniverse* dag_univ_ptr =
    dynamic_cast<openmc::DAGUniverse*>(univ_ptr_ref.get());

  // Save if we are using UWUW
  useUWUW = dag_univ_ptr->uses_uwuw();

  // Write out materials for later use
  if(useUWUW)
    dag_univ_ptr->write_uwuw_materials_xml("uwuw_materials.xml");

  return true;
}

bool
OpenMCExecutioner::initMaterials()
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

    if(useUWUW){
      std::string prop="mat:";
      std::string delim="/";
      size_t pos=mat_name.find(prop);
      if(pos!=std::string::npos){
        size_t prop_size =prop.size()+pos;
        size_t new_size = mat_name.size()-prop_size;
        mat_name = mat_name.substr(prop_size,new_size);
      }
      pos =mat_name.find(delim);
      if(pos!=std::string::npos){
        size_t new_size = mat_name.size()-delim.size();
        mat_name = mat_name.substr(0,new_size);
      }
    }
    mat_names_to_id[mat_name] = id;
  }
  return true;
}

bool
OpenMCExecutioner::initMeshTallies()
{
  // Create a new unstructured mesh in openmc
  openmc::model::meshes.push_back(std::make_unique<openmc::MOABMesh>(moab().moabPtr));

  // Auto-assign mesh ID
  openmc::model::meshes.back()->set_id(openmc::C_NONE);

  // Add a new mesh filter with auto-assigned ID
  openmc::Filter* filter_ptr = openmc::Filter::create("mesh",openmc::C_NONE);

  // Upcast pointer type
  openmc::MeshFilter* mesh_filter = dynamic_cast<openmc::MeshFilter*>(filter_ptr);

  if(mesh_filter == nullptr){
    mooseError("Failed to create mesh filter");
  }

  // Pass in the index of our mesh to the filter
  int32_t mesh_idx = openmc::model::meshes.size() -1;
  mesh_filter->set_mesh(mesh_idx);

  // Set up the tallies we need with this mesh
  setupTallies(filter_ptr);

  return true;
}


void
OpenMCExecutioner::setupTallies(openmc::Filter* filter_ptr)
{

  // Loop over tally ids
  for(auto & tally_scores : tally_ids_to_scores){

    // Get the tally id
    int32_t tally_id = tally_scores.first;

    // Get the list of scores
    auto& scores = tally_scores.second;

    // If auto-assign, wait until end so we don't use reserved ids
    if(tally_id == openmc::C_NONE) continue;

    // Create / update tally
    setupTally(tally_id,filter_ptr,scores);

  }

  // Deal with auto-assign id case and update id our map
  auto tally_it = tally_ids_to_scores.find(openmc::C_NONE);
  if(tally_it != tally_ids_to_scores.end()){

    // Get the list of scores
    auto scores = tally_it->second;

    // Create tally and update the tally id
    int32_t tally_id = tally_it->first;
    setupTally(tally_id,filter_ptr,scores);

    // Update map entry
    tally_ids_to_scores.erase(tally_it);
    tally_ids_to_scores[tally_id] = scores;

  }

}


void
OpenMCExecutioner::setupTally(int32_t& tally_id,
                              openmc::Filter* filter_ptr,
                              std::vector<ScoreData>& scores)
{

  openmc::Tally* tally_ptr;

  // Check whether to create a new tally
  if(openmc::model::tally_map.find(tally_id)
     == openmc::model::tally_map.end())
  {
    // Create a tally with this id
    tally_ptr = openmc::Tally::create(tally_id);

    // If this was an auto-assign ID, get the right one
    if(tally_id == openmc::C_NONE){
      tally_id = tally_ptr->id_;
    }

    // Set name and estimator
    tally_ptr->name_ = "moose_tally_"+std::to_string(tally_id);
    tally_ptr->estimator_ = openmc::TallyEstimator::TRACKLENGTH;
  }
  else{
    // Get pointer for existing tally
    int32_t tally_idx = openmc::model::tally_map.at(tally_id);
    tally_ptr = (openmc::model::tallies.at(tally_idx)).get();
  }

  // Add mesh filter
  tally_ptr->add_filter(filter_ptr);

  // Set the scores to for this tally and update ScoreData indices
  std::vector<std::string> score_names;
  for(auto & score_data : scores){
    std::string name = score_data.score_name;
    score_names.push_back(name);
    score_data.index = score_names.size()-1;
  }
  tally_ptr->set_scores(score_names);

  // Add triggers
  // TODO generalise
  // Trigger metric
  openmc::TriggerMetric metric=openmc::TriggerMetric::relative_error;
  // Trigger threshhold
  double threshold=0.01;
  tally_ptr->triggers_.reserve(tally_ptr->scores_.size());
  for (auto i_score = 0; i_score < tally_ptr->scores_.size(); ++i_score) {
    tally_ptr->triggers_.push_back({metric, threshold, i_score});
  }
}


bool
OpenMCExecutioner::updateOpenMC()
{

  TIME_SECTION(_updateopenmc_timer);

  // Update materials and mesh tallies
  if(!resetOpenMC()){
    std::cerr<<"Failed to reset OpenMC"<<std::endl;
    return false;
  }

  if(!reloadDAGMC()){
    std::cerr<<"Failed to load data into DagMC"<<std::endl;
    return false;
  }

  // Final OpenMC setup after geometry is updated.
  completeSetup();

  // Overwrite summary file
  if (this->comm().rank() == 0 && openmc::settings::output_summary){
    openmc::write_summary();
  }

  return true;
}

bool
OpenMCExecutioner::getResults(std::map<std::string,std::vector< double > > & var_results_by_elem)
{

  // Clear any previous data if there was any
  var_results_by_elem.clear();

  // Loop over tally id
  for(const auto & tally_scores : tally_ids_to_scores){
    // Get the tally id
    int tally_id = tally_scores.first;

    // Get the tally index
    int32_t t_index(0);
    openmc_err = openmc_get_tally_index(tally_id,&t_index);
    if (openmc_err) return false;

    // Fetch a reference to the tally
    openmc::Tally& tally = *(openmc::model::tallies.at(t_index));

    // Get sample size (number of batches)
    int nSample =  tally.n_realizations_;

    // Fetch the number of tally scores
    size_t nScores = (tally.scores_).size();

    //Fetch a reference to the indices of the filters;
    std::map<int32_t, FilterInfo> filters_by_id;
    int32_t nFilterBins;
    int32_t meshFilter;
    if(!setFilterInfo(tally,filters_by_id,meshFilter,nFilterBins)){
      return false;
    }

    // Get a reference to the tally results and check shape
    xt::xtensor<double, 3> & results = tally.results_;
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

    // Get the number of mesh bins
    size_t nMeshBins = filters_by_id[meshFilter].nbins;

    // Initialise storage for results by variable name
    for(const auto & score : tally_scores.second){
      std::string var_name = score.var_name;
      std::string err_name = score.err_name;
      var_results_by_elem[var_name]=std::vector<double>(nMeshBins,0.);
      if(score.saveErr){
        var_results_by_elem[err_name]=std::vector<double>(nMeshBins,0.);
      }
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

      // Get the mesh bin
      int32_t meshIndex = -1;
      if(filter_id_to_bin_index.find(meshFilter)!=filter_id_to_bin_index.end()){
        meshIndex = filter_id_to_bin_index[meshFilter];
      }
      if(meshIndex == -1){
        openmc::set_errmsg("Failed to set filter bin indices");
        return false;
      }

      // Get results for each score for this mesh bin
      for(auto & score : tally_scores.second){

        // Last index: 0-> internal placeholder, 1-> sum, 2-> sum_sq
        double sum = results(iresult,score.index,1);
        double sumsq = results(iresult,score.index,2);

        // Add to mean for this mesh element
        // (may be multiple filter bin contributions)
        var_results_by_elem[score.var_name].at(meshIndex) += sum/double(nSample);

        if(score.saveErr){
          var_results_by_elem[score.err_name].at(meshIndex) += sumsq/double(nSample);
        }
      }

    } // End loop over results vector

    // Post-processing to get variance
    for(auto & score : tally_scores.second){
      if(score.saveErr){
        for(size_t iMeshBin=0; iMeshBin<nMeshBins; iMeshBin++){
          // Get the mean for this bin
          double mean = var_results_by_elem[score.var_name].at(iMeshBin);
          // Subtract mean squared to get variance of sample
          var_results_by_elem[score.err_name].at(iMeshBin) -= mean*mean;
          // Divide by N-1 to get variance of the mean
          var_results_by_elem[score.err_name].at(iMeshBin) /= (nSample-1);
        }
      }
    }

  } // End loop over tallies

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
                                 int32_t& nFilterBins)
{

  meshFilter=-1;
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

  nFilterBins = filters_by_id[firstFilter].stride*filters_by_id[firstFilter].nbins;
  if(nFilterBins != tally.n_filter_bins()){
    openmc::set_errmsg("Inconsistent number of filter bins.");
    return false;
  }

  // Done
  return true;

}

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
}

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
}


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

  // Clear existing cell data
  openmc::model::cells.clear();
  openmc::model::cell_map.clear();

  // Clear existing surface data
  openmc::model::surfaces.clear();
  openmc::model::surface_map.clear();

  updateMaterials();

  updateMeshTallies();

  return true;
}


bool
OpenMCExecutioner::reloadDAGMC()
{
  moab::ErrorCode rval;

  // Backup streambuffer of std out if redirecting
  std::streambuf* stream_buffer_stdout(nullptr);

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

  // Create a new DagMC, but pass in our MOAB interface
  dagPtr.reset(new moab::DagMC(moab().moabPtr));

  // Set up geometry in DagMC from already-loaded mesh
  rval = dagPtr->load_existing_contents();
  if(rval!= moab::MB_SUCCESS) return false;

  // Initialize acceleration data structures
  rval = dagPtr->init_OBBTree();
  if(rval!= moab::MB_SUCCESS) return false;

  if(redirect_dagout && stream_buffer_stdout!= nullptr){
    // Reset cout streambuffer
    std::cout.rdbuf(stream_buffer_stdout);
  }

  updateDAGUniverse();

  return true;
}

void
OpenMCExecutioner::updateDAGUniverse()
{

  // Get an iterator to the DAGMC universe unique ptr
  auto univ_it = openmc::model::universes.begin()+dag_univ_idx;

  // Remove the old universe
  openmc::model::universes.erase(univ_it);

  // Create new DAGMC universe
  openmc::DAGUniverse* dag_univ_ptr = new openmc::DAGUniverse(dagPtr);
  openmc::model::universes.emplace(univ_it,std::unique_ptr<openmc::DAGUniverse>(dag_univ_ptr));

  // Add cells to universes
  openmc::populate_universes();
}

void
OpenMCExecutioner::updateMaterials()
{
  // Only update materials once
  if(matsUpdated) return;

  // Retrieve material data
  std::vector<std::string> mat_names;
  std::vector<double> initial_densities;
  std::vector<std::string> tails;
  std::vector<MOABMaterialProperties> properties;
  moab().getMaterialProperties(mat_names,initial_densities,tails,properties);
  updateDensity = !initial_densities.empty();

  // First check if we can find the original material names in openmc
  std::map<int32_t,size_t> orig_ID_to_moose_index;
  int32_t maxOrigID=0;
  int32_t minOrigID=std::numeric_limits<int32_t>::max();
  for(size_t iMat=0; iMat<mat_names.size(); iMat++){
    std::string mat_name = mat_names.at(iMat);
    if(mat_names_to_id.find(mat_name)==mat_names_to_id.end()){
      std::string err="Could not find material "+mat_name;
      mooseError(err);
    }
    int32_t mat_id = mat_names_to_id[mat_name];
    if(openmc::model::material_map.find(mat_id)==
       openmc::model::material_map.end()){
      std::string err="Could not find openmc material with id "+mat_id;
      mooseError(err);
    }
    if(mat_id>maxOrigID) maxOrigID=mat_id;
    if(mat_id<minOrigID) minOrigID=mat_id;
    // Save
    orig_ID_to_moose_index[mat_id]=iMat;
  }

  // Check consistency of sizes
  if(properties.size() != tails.size()){
    mooseError("Error setting updated material metadata.");
  }

  // Clear existing material data
  openmc::free_memory_material();
  mat_names_to_id.clear();
  mat_id_to_density.clear();

  // Get the original xml string
  pugi::xml_document doc;
  std::string filename;
  if(useUWUW){
    // We need to keep this line because of the way that nuclides are
    // re-initialised - otherwise there ends up being a mismatch
    openmc::read_materials_xml();
    filename = openmc::settings::path_input + "uwuw_materials.xml";
  }
  else{
    filename = openmc::settings::path_input + "materials.xml";
  }

  // Parse materials.xml file and get root element
  doc.load_file(filename.c_str());

  // Loop over child nodes in xml string
  pugi::xml_node root = doc.document_element();
  for (pugi::xml_node material_node : root.children("material")) {

    int32_t origMatID = std::stoi(openmc::get_node_value(material_node, "id"));

    if(orig_ID_to_moose_index.find(origMatID) ==
       orig_ID_to_moose_index.end()){
      std::string err = "Unknown material ID from material.xml: "
        +std::to_string(origMatID);
      mooseWarning(err);
      openmc::model::materials.push_back(std::make_unique<openmc::Material>(material_node));
      continue;
    }

    // Get moose's index for this material
    size_t iMat = orig_ID_to_moose_index.at(origMatID);

    // Get the original material name
    std::string origName = mat_names.at(iMat);

    // Get the original density
    double origDen = (updateDensity) ?  initial_densities.at(iMat) : 0.;

    // Loop over new materials in decreasing order so we never
    // simultaneously try to create any Material with the same ID
    // (Needed because we reuse the same xml node)
    for(int iNewMat=int(properties.size())-1; iNewMat>=0; iNewMat--){

      // Get the material properties for this material
      MOABMaterialProperties mat_props = properties.at(iNewMat);

      // Get the relative density
      double relDiff = mat_props.rel_density;

      // Get the temperature
      double temp = mat_props.temp;

      // Create new material in place
      openmc::model::materials.push_back(std::make_unique<openmc::Material>(material_node));

      // Get a reference to our mat
      openmc::Material& mat = *openmc::model::materials.back();

      // Update ID
      int32_t newID = iNewMat*(maxOrigID) + origMatID;
      mat.set_id(newID);

      // Update_name
      std::string new_name = origName + tails.at(iNewMat);
      mat.set_name(new_name);

      // Update temperature
      mat.set_temperature(temp);

      // Update density
      if(updateDensity){
        double newDen = (1.0+relDiff)*origDen;

        // Save updated density (we will update later)
        mat_id_to_density[newID]=newDen;
      }

      // Update mat lib index
      mat_names_to_id[new_name]=newID;
    }
  }

  // Success!
  matsUpdated = true;
}

void
OpenMCExecutioner::updateMaterialDensities()
{
  if (!updateDensity) return;

  for(auto& mat: openmc::model::materials){
    // Look up densities saved by id
    int32_t m_id= mat->id_;
    if(mat_id_to_density.find(m_id)!=mat_id_to_density.end()){
      double newDen = mat_id_to_density[m_id];
      // Set new density
      mat->set_density(newDen,"g/cm3");
    }
  }
}

void
OpenMCExecutioner::updateMeshTallies()
{
  // Retrieve the current mesh id
  int32_t mesh_id = openmc::model::meshes.back()->id_;

  // Update in place the mesh pointer
  openmc::model::meshes.back().reset(new openmc::MOABMesh(moab().moabPtr));

  // Set mesh ID to what is was before
  openmc::model::meshes.back()->id_ = mesh_id;

  // Get a pointer to current mesh filter
  openmc::Filter* filter_ptr = openmc::model::tally_filters.back().get();

  // Upcast pointer type
  openmc::MeshFilter* mesh_filter = dynamic_cast<openmc::MeshFilter*>(filter_ptr);

  int nBinsBefore = mesh_filter->n_bins();

  // Update the mesh in the mesh_filter
  int32_t mesh_idx = openmc::model::meshes.size() -1;
  mesh_filter->set_mesh(mesh_idx);

  int nBinsAfter = mesh_filter->n_bins();

  // Update strides in tallies if number of bins has changed
  if(nBinsAfter != nBinsBefore){
    for(auto tally_pair : tally_ids_to_scores){
      int32_t tally_id = tally_pair.first;

      // Look up tally index
      auto tally_it = openmc::model::tally_map.find(tally_id);
      if(tally_it == openmc::model::tally_map.end()){
        mooseError("Could not find tally ID in map.");
      }
      int32_t tally_index = tally_it->second;

      // Re-calcuate strides
      openmc::model::tallies.at(tally_index)->set_strides();
    }
  }
}

void
OpenMCExecutioner::completeSetup()
{
  // Set the root universe
  openmc::model::root_universe = openmc::find_root_universe();
  openmc::check_dagmc_root_univ();

  // Final geometry setup and assign temperatures
  openmc::finalize_geometry();

  // Finalize cross sections having assigned temperatures
  openmc::finalize_cross_sections();

  // This occurs here because some material attributes are not properly
  // initialised until after finalize_cross_sections is called.
  updateMaterialDensities();
}
