// Moose includes
#include "OpenMCExecutioner.h"

registerMooseObject("OpenMCApp", OpenMCExecutioner);

// Declare OpenMC global objects that we need
namespace openmc::model {
  extern std::shared_ptr<moab::Interface> moabPtr;
  extern std::vector<std::unique_ptr<Tally>> tallies;
  extern std::vector<std::unique_ptr<Filter>> tally_filters;
}

template <>
InputParameters
validParams<OpenMCExecutioner>()
{
  InputParameters params = validParams<Transient>();
  return params;
}

OpenMCExecutioner::OpenMCExecutioner(const InputParameters & parameters) : Transient(parameters), isInit(false)
{
  // Create MOAB interface
  moabPtr =  std::make_shared<moab::Core>();
  var_name = "heating-local";
}

void
OpenMCExecutioner::init()
{
  Transient::init();
  
  if(!initMOAB()){
    std::cerr<<"Failed to initialize MOAB"<<std::endl;
    return;
  }
  
  if(!initOpenMC()){
    std::cerr<<"Failed to initialize OpenMC"<<std::endl;
    return;
  }

  if(!initSystems()){
    std::cerr<<"Failed to initalize systems"<<std::endl;
    return;
  }

  // Mark successful initialisation
  isInit = true;
}

void
OpenMCExecutioner::execute()
{
  // Don't continue if something already went wrong
  if (!isInit) return;
    
  // Clear tallies from any previous calls
  openmc_err = openmc_reset();
  if (openmc_err) return;

  // Run the simulation
  openmc_err = openmc_run();
  if (openmc_err) return;

  // Fetch the tallied results
  std::vector< std::vector< double > > results_by_mat;
  if(!getResults(results_by_mat)) return;

  if(results_by_mat.empty()) return;
  
  // Pass the results into libMesh systems
  // ( just one material for now )
  // TODO system for every material?
  try
    {
      setSolution(iSys, iVar, results_by_mat.back());
    }
  catch(std::logic_error &e)
    {
      std::cerr<<"Failed to pass OpenMC results to systems"<<std::endl;
      std::cerr<<e.what()<<std::endl;
      return;
    }

  std::cout<<"Solution set"<<std::endl;

  try
    {
      _time = 1;
      _problem.outputStep(EXEC_FINAL);
    }
  catch(std::exception &e)
    {
      std::cerr<<"Failed to output results."<<std::endl;
      std::cerr<<e.what()<<std::endl;
      return;
     }  

  std::cout<<"output results"<<std::endl;
  
}

MeshBase&
OpenMCExecutioner::mesh()
{
  return feProblem().mesh().getMesh();  
}

EquationSystems&
OpenMCExecutioner::systems()
{
  return feProblem().es();
}

System&
OpenMCExecutioner::system(std::string var_now)
{
  return feProblem().getSystem(var_now);  
}


bool
OpenMCExecutioner::initSystems()
{
  try
    {
      libMesh::System& sys = system(var_name);      
      iSys = sys.number();
      iVar = sys.variable_number(var_name);      
    }
  catch(std::exception &e)
    {
      std::cerr<<e.what()<<std::endl;
      return false;
    }
  
  // // Add a new system and fetch a reference
  // libMesh::System& sys = systems().add_system("Basic","OpenMCsys");
  // sys.set_basic_system_only();

  // // Set ID to retrieve system later
  // iSys = sys.number();
  
  // // Add a new variable and save index
  // iVar = sys.add_variable("heating",libMesh::Order::CONSTANT,libMesh::FEFamily::MONOMIAL);
  
  // Initialise the data structures for the equation systems
  systems().init();

  return true;
}


bool
OpenMCExecutioner::initProblem()
{
 try
    {
      _problem.initialSetup();
    }
  catch(std::exception &e)
    {
      std::cerr<<e.what()<<std::endl;
      return false;
    }
 return true;
}

bool
OpenMCExecutioner::initMOAB()
{
  // Fetch spatial dimension from libMesh
  int dim = mesh().spatial_dimension() ;
  
  // Set spatial dimension in MOAB
  moab::ErrorCode  rval = moabPtr->set_dimension(dim);
  if(rval!=moab::MB_SUCCESS) return false;
  
  std::map<dof_id_type,moab::EntityHandle> node_id_to_handle;
  rval = createNodes(node_id_to_handle);
  if(rval!=moab::MB_SUCCESS) return false;

  rval = createElems(node_id_to_handle,_elem_handle_to_id);
  if(rval!=moab::MB_SUCCESS) return false;

  return true;
}

bool
OpenMCExecutioner::initOpenMC()
{

  // Set OpenMC's copy of MOAB
  openmc::model::moabPtr = moabPtr;
  
  char * argv[] = {nullptr, nullptr};
  openmc_err = openmc_init(1, argv, &_communicator.get());
  if (openmc_err) return false;
  else return true;
  
}

moab::ErrorCode
OpenMCExecutioner::createNodes(std::map<dof_id_type,moab::EntityHandle>& node_id_to_handle)
{
  moab::ErrorCode rval(moab::MB_SUCCESS);

  // Clear prior results.
  node_id_to_handle.clear();
    
  // Init array for MOAB node coords
  double 	coords[3];

    // TODO think about how the mesh is distributed...
  // Iterate over nodes in libmesh
  auto itnode = mesh().nodes_begin();
  auto endnode = mesh().nodes_end();
  for( ; itnode!=endnode; ++itnode){
    // Fetch a const ref to node
    const Node& node = **itnode;

    // Fetch coords
    coords[0]=node(0);
    coords[1]=node(1);
    coords[2]=node(2);

    // Fetch ID
    dof_id_type id = node.id();

    // Add node to MOAB database and get handle
    moab::EntityHandle ent(0);
    rval = moabPtr->create_vertex(coords,ent);
    if(rval!=moab::MB_SUCCESS){
      node_id_to_handle.clear();
      return rval;
    }

    // Save mapping of ids.
    node_id_to_handle[id] = ent;
       
  }

  return rval;
}



moab::ErrorCode
OpenMCExecutioner::createElems(std::map<dof_id_type,moab::EntityHandle>& node_id_to_handle,std::map<dof_id_type,moab::EntityHandle>& elem_handle_to_id)
{
  moab::ErrorCode rval(moab::MB_SUCCESS);

  // Clear prior results.
  elem_handle_to_id.clear();
  
  // Initialise an array for moab connectivity
  unsigned int nNodes = 4;
  moab::EntityHandle conn[nNodes];

  // moab::Range tets;
  
  // TODO think about how the mesh is distributed...
  // Iterate over elements in libmesh
  auto itelem = mesh().elements_begin();
  auto endelem = mesh().elements_end();
  for( ; itelem!=endelem; ++itelem){
    Elem& elem = **itelem;

    // Check all the elements are tets 
    ElemType type = elem.type();
    if(type!=TET4){
      rval = moab::MB_FAILURE;
      return rval;
    }

    // Get the connectivity
    std::vector< dof_id_type > conn_libmesh;
    elem.connectivity	(0,libMesh::IOPackage::VTK,conn_libmesh);
    if(conn_libmesh.size()!=nNodes){
      rval = moab::MB_FAILURE;
      return rval;
    }
    
    // Set MOAB connectivity
    for(unsigned int iNode=0; iNode<nNodes;++iNode){
      if(node_id_to_handle.find(conn_libmesh.at(iNode)) == 
         node_id_to_handle.end()){
        rval = moab::MB_FAILURE;
        return rval;
      }
      conn[iNode]=node_id_to_handle[conn_libmesh.at(iNode)];
    }

    // Fetch ID
    dof_id_type id = elem.id();

    // Create an element in MOAB database
    moab::EntityHandle ent(0);
    rval = moabPtr->create_element(moab::MBTET,conn,nNodes,ent);
    if(rval!=moab::MB_SUCCESS){
      elem_handle_to_id.clear();
      return rval;
    }
    elem_handle_to_id[ent]=id;
    //    tets.insert(ent);
    
  }

  // moab::EntityHandle tetset;
  // rval = moabPtr->create_meshset(moab::MESHSET_SET, tetset);
  // if (rval != moab::MB_SUCCESS) {
  //   return rval;
  // }
  
  // rval = moabPtr->add_entities(tetset, tets);
  // if (rval != moab::MB_SUCCESS) {
  //   return rval;
  // }
  
  return rval;  
}

bool
OpenMCExecutioner::getResults(std::vector< std::vector< double > > &results_by_mat)
{

  // Assume we know what tally we want
  int32_t tallyID = 1;

  // Get the tally index
  int32_t t_index(0);
  openmc_err = openmc_get_tally_index(tallyID,&t_index);
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
    if(name == "heating-local"){
      // Found the score we want
      heatScoreIndex = int(iScore);
      break;
    }
  }
  if(heatScoreIndex == -1){
    openmc::set_errmsg("Failed to find 'heating-local' score.");
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
    
    // if(result != 0.){
    //   std::cout<<"setting result"
    //            << " matIndex = "<< matIndex
    //            << " meshIndex = "<< meshIndex
    //            << " " << result<<std::endl;
    // }
        
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

moab::EntityHandle
OpenMCExecutioner::bin_index_to_handle(unsigned int index)
{
  if(index > _elem_handle_to_id.size() ) return 0;

  moab::EntityHandle ent = (_elem_handle_to_id.begin())->first + index;
  return ent;
}


void
OpenMCExecutioner::setSolution(unsigned int iSysNow,  unsigned int iVarNow, std::vector< double > &results)
{

  // Fetch a reference to our system
  // TODO debug this line!  
  libMesh::System& sys = systems().get_system(iSysNow);
    
  // Check the sizes match up
  uint32_t nBins = results.size();
  // if(sys.solution->size() != nBins){
  //   std::cout<<" Solution size = "<< sys.solution->size()<<std::endl;
  //   std::cout<<" current local Solution size = "<< sys.current_local_solution->size()<<std::endl;
  //   std::cout<<"nvars = "<< sys.n_vars()<< " n comps" <<sys.n_components()<<std::endl;
  //   throw std::logic_error("Solution has a different number of bins to OpenMC results vector.");
  // }

  // Check we map our bins onto unique indices
  std::set<dof_id_type> sol_indices;
  
  std::cout<<"Number of bins = "<<nBins<<std::endl;
  
  // Loop over mesh filter bins
  for(uint32_t iBin=0; iBin< nBins; iBin++){

    // Result for this bin
    double result = results.at(iBin);

    // Get the solution index for this bin
    dof_id_type index = bin_index_to_soln_index(iSysNow,iVarNow,iBin);

    // Check we haven't used this index already
    if(sol_indices.find(index)!= sol_indices.end()){
      throw std::logic_error("OpenMC indices non-uniquely map to solution indices.");
    }
    sol_indices.insert(index);

    // if(result != 0.){
    //   std::cout<<"setting result"<< index << " " << result<<std::endl;
    // }

    //Number complex_result = (result,0.);
    
    // Set the solution for this index
    sys.solution->set(index,result);
  }

  sys.solution->close();

  _problem.copySolutionsBackwards();

  for (THREAD_ID tid = 0; tid < libMesh::n_threads(); ++tid){
    _problem.getVariable(tid,var_name).computeElemValues(); 
  }
    
  
}

dof_id_type
OpenMCExecutioner::bin_index_to_soln_index(unsigned int iSysNow, unsigned int iVarNow, unsigned int index)
{

  // Convert the bin index to an entity handle
  moab::EntityHandle ent = bin_index_to_handle(index);
  
  // Convert the entity handle to a libMesh id
  dof_id_type id = _elem_handle_to_id[ent];
  
  // Get a reference to the element with this ID
  Elem& elem  = mesh().elem_ref(id);

  // Expect only one component, but check anyay
  unsigned int n_components = elem.n_comp(iSysNow,iVarNow);
  if(n_components != 1){
    throw std::logic_error("Unexpected number of expected solution components");
  }
  
  // Get the degree of freedom number
  dof_id_type soln_index = elem.dof_number(iSysNow,iVarNow,0);

  return soln_index;
  
}
