// Moose includes
#include "OpenMCExecutioner.h"

registerMooseObject("OpenMCApp", OpenMCExecutioner);

// Declare OpenMC global objects that we need
namespace openmc::model {
  extern std::map<int32_t, std::shared_ptr<moab::Interface> > moabPtrs;
  extern std::vector<std::unique_ptr<Tally>> tallies;
  extern std::vector<std::unique_ptr<Filter>> tally_filters;
}

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
  return params;
}

OpenMCExecutioner::OpenMCExecutioner(const InputParameters & parameters) :
  Transient(parameters),
  setProblemLocal(false),
  isInit(false),
  var_name(getParam<std::string>("variable")),
  score_name(getParam<std::string>("score_name")),
  source_strength(getParam<double>("neutron_source")),
  tally_id(getParam<int32_t>("tally_id")),
  mesh_id(getParam<int32_t>("mesh_id"))
{
}

void
OpenMCExecutioner::execute()
{

  if(!initialize()) return;

  if(!update()) return;

  if(!run()){
     std::cerr<<"Failed to run OpenMC"<<std::endl;
     return;
  }

}

bool
OpenMCExecutioner::initialize()
{

  // Don't re-initialize
  if(isInit) return true;

  // Initialize MOAB here so it occurs after transfers when part of MultiApp
  if(!initMOAB()){
    std::cerr<<"Failed to initialize MOAB"<<std::endl;
    return false;
  }

  if(!initOpenMC()){
    std::cerr<<"Failed to initialize OpenMC"<<std::endl;
    return false;
  }

  isInit = true;
  return isInit;
}

bool
OpenMCExecutioner::update()
{

  try{
    moab().update();
  }
  catch(std::exception &e){
    std::cerr<<e.what()<<std::endl;
    return false;
  }

  return true;
}

bool
OpenMCExecutioner::run()
{
  // Clear tallies from any previous calls
  openmc_err = openmc_reset();
  if (openmc_err) return false;

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
  if(!moab().setSolution(var_name,results_by_mat.back(),source_strength,true)){
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
  else return true;

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
