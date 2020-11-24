// Moose includes
#include "MoabUserObject.h"

registerMooseObject("OpenMCApp", MoabUserObject);

template <>
InputParameters
validParams<MoabUserObject>()
{
  InputParameters params = validParams<UserObject>();

  params.addParam<double>("length_scale", 1.,"Scale factor to convert lengths from MOOSE to MOAB");
  params.addParam<std::string>("bin_varname", "", "Variable name by whose results elements should be binned.");
  params.addParam<std::vector<std::string> >("material_names", std::vector<std::string>(), "List of material names");
  params.addParam<double>("var_min", 300.,"Minimum value to define range of bins.");
  params.addParam<double>("var_max", 600.,"Max value to define range of bins.");
  params.addParam<bool>("logscale", false, "Switch to determine if logarithmic binning should be used.");
  params.addParam<unsigned int>("n_bins", 60, "Number of bins");

  params.addParam<double>("faceting_tol",1.e-4,"Faceting tolerance for DagMC");
  params.addParam<double>("geom_tol",1.e-6,"Geometry tolerance for DagMC");

  return params;
}

MoabUserObject::MoabUserObject(const InputParameters & parameters) :
  UserObject(parameters),
  _problem_ptr(nullptr),
  lengthscale(getParam<double>("length_scale")),
  var_name(getParam<std::string>("bin_varname")),
  logscale(getParam<bool>("logscale")),
  var_min(getParam<double>("var_min")),
  var_max(getParam<double>("var_max")),
  nVarBins(getParam<unsigned int>("n_bins")),
  mat_names(getParam<std::vector<std::string> >("material_names")),
  faceting_tol(getParam<double>("faceting_tol")),
  geom_tol(getParam<double>("geom_tol"))
{
  // Create MOAB interface
  moabPtr =  std::make_shared<moab::Core>();

  // Create a skinner
  skinner = std::make_unique<moab::Skinner>(moabPtr.get());

  // Set variables relating to binning
  binElems = !( var_name == "" || mat_names.empty());
  if(var_min <= 0.){
    mooseError("var_min out of range! Please pick a value > 0");
  }
  if(var_max <= var_min){
    mooseError("Please pick a value for var_max > var_min");
  }
  bin_width = (var_max-var_min)/double(nVarBins);
  powMin = int(floor(log10(var_min)));
  powMax = int(ceil(log10(var_max)));
  nPow = std::min(powMax-powMin, 1);
  if(nPow > nVarBins){
    mooseError("Please ensure number of powers for variable is less than the number of bins");
  }
  nMinor = nVarBins/nPow;
}

FEProblemBase&
MoabUserObject::problem()
{
  if(_problem_ptr == nullptr)
    throw std::logic_error("No problem was set");

  return *_problem_ptr;
}

MeshBase&
MoabUserObject::mesh()
{
  return problem().mesh().getMesh();
}

EquationSystems&
MoabUserObject::systems()
{
  return problem().es();
}

System&
MoabUserObject::system(std::string var_now)
{
  return problem().getSystem(var_now);
}

void
MoabUserObject::initMOAB()
{
  // Fetch spatial dimension from libMesh
  int dim = mesh().spatial_dimension() ;

  // Set spatial dimension in MOAB
  moab::ErrorCode  rval = moabPtr->set_dimension(dim);
  if(rval!=moab::MB_SUCCESS)
    throw std::logic_error("Failed to set MOAB dimension");

  std::map<dof_id_type,moab::EntityHandle> node_id_to_handle;
  rval = createNodes(node_id_to_handle);
  if(rval!=moab::MB_SUCCESS)
    throw std::logic_error("Could not create nodes");

  rval = createElems(node_id_to_handle);
  if(rval!=moab::MB_SUCCESS)
    throw std::logic_error("Could not create elems");

  rval = setupTags();
  if(rval!=moab::MB_SUCCESS)
    throw std::logic_error("Could not set up tags");

}

bool
MoabUserObject::update()
{
  // Don't attempt to bin results if we haven't been provided with a variable
  if(!binElems) return false;

  // Sort libMesh elements into bins of the specified variable
  if(!sortElemsByResults()) return false;

  // Find local groups of elements
  if(!groupLocalElems()) return false;

  // Deliberately force fail for now.
  return false;
}

// Pass the results for named variable into the libMesh systems solution
bool
MoabUserObject::setSolution(std::string var_now,std::vector< double > &results, double scaleFactor, bool normToVol)
{
  try
    {
      libMesh::System& sys = system(var_now);
      unsigned int iSys = sys.number();
      unsigned int iVar = sys.variable_number(var_now);
      setSolution(iSys,iVar,results,scaleFactor,normToVol);

      problem().copySolutionsBackwards();

      for (THREAD_ID tid = 0; tid < libMesh::n_threads(); ++tid){
        problem().getVariable(tid,var_now).computeElemValues();
      }
    }
  catch(std::exception &e)
    {
      std::cerr<<e.what()<<std::endl;
      return false;
    }

  return true;
}

moab::ErrorCode
MoabUserObject::createNodes(std::map<dof_id_type,moab::EntityHandle>& node_id_to_handle)
{

  if(!hasProblem()) return moab::MB_FAILURE;

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

    // Fetch coords (and scale to correct units)
    coords[0]=lengthscale*double(node(0));
    coords[1]=lengthscale*double(node(1));
    coords[2]=lengthscale*double(node(2));

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
MoabUserObject::createElems(std::map<dof_id_type,moab::EntityHandle>& node_id_to_handle)
{

  if(!hasProblem()) return moab::MB_FAILURE;

  moab::ErrorCode rval(moab::MB_SUCCESS);

  // Clear prior results.
  clearElemMaps();

  //Create a meshset
  rval = moabPtr->create_meshset(moab::MESHSET_SET,meshset);
  if(rval!=moab::MB_SUCCESS) return rval;

  // Initialise an array for moab connectivity
  unsigned int nNodes = 4;
  moab::EntityHandle conn[nNodes];
  moab::Range all_elems;

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
      clearElemMaps();
      return rval;
    }

    // Save mapping between libMesh ids and moab handles
    addElem(id,ent);

    // Add to range of all elems
    all_elems.insert(ent);
  }

  // Add the elems to the meshset
  rval = moabPtr->add_entities	(meshset,all_elems);

  return rval;
}

moab::ErrorCode
MoabUserObject::setupTags()
{

  // Create some tags for later use
  moab::ErrorCode rval = moab::MB_SUCCESS;

  // First some built-in MOAB tag types
  rval = moabPtr->tag_get_handle(GEOM_DIMENSION_TAG_NAME, 1, moab::MB_TYPE_INTEGER, geometry_dimension_tag, moab::MB_TAG_DENSE | moab::MB_TAG_CREAT);
  if(rval!=moab::MB_SUCCESS) return rval;

  rval = moabPtr->tag_get_handle(GLOBAL_ID_TAG_NAME, 1, moab::MB_TYPE_INTEGER, id_tag, moab::MB_TAG_DENSE | moab::MB_TAG_CREAT);
  if(rval!=moab::MB_SUCCESS) return rval;

  rval = moabPtr->tag_get_handle(CATEGORY_TAG_NAME, CATEGORY_TAG_SIZE, moab::MB_TYPE_OPAQUE, category_tag, moab::MB_TAG_SPARSE | moab::MB_TAG_CREAT);
  if(rval!=moab::MB_SUCCESS)  return rval;

  rval = moabPtr->tag_get_handle(NAME_TAG_NAME, NAME_TAG_SIZE, moab::MB_TYPE_OPAQUE, name_tag, moab::MB_TAG_SPARSE | moab::MB_TAG_CREAT);
  if(rval!=moab::MB_SUCCESS)  return rval;

  // Some tags needed for DagMC
  rval = moabPtr->tag_get_handle("FACETING_TOL", 1, moab::MB_TYPE_DOUBLE, faceting_tol_tag,moab::MB_TAG_SPARSE | moab::MB_TAG_CREAT);
  if(rval!=moab::MB_SUCCESS)  return rval;

  rval = moabPtr->tag_get_handle("GEOMETRY_RESABS", 1, moab::MB_TYPE_DOUBLE, geometry_resabs_tag, moab::MB_TAG_SPARSE | moab::MB_TAG_CREAT);
  if(rval!=moab::MB_SUCCESS)  return rval;

  rval = moabPtr->tag_get_handle("Material", 1, moab::MB_TYPE_DOUBLE, material_tag, moab::MB_TAG_SPARSE | moab::MB_TAG_CREAT);
  if(rval!=moab::MB_SUCCESS)  return rval;

  // Set the values for DagMC faceting / geometry tolerance tags on the mesh entity set
  rval = moabPtr->tag_set_data(faceting_tol_tag, &meshset, 1, &faceting_tol);
  if(rval!=moab::MB_SUCCESS)  return rval;

  rval = moabPtr->tag_set_data(geometry_resabs_tag, &meshset, 1, &geom_tol);
  if(rval!=moab::MB_SUCCESS)  return rval;

  return moab::MB_SUCCESS;
}

void
MoabUserObject::clearElemMaps()
{
  _elem_handle_to_id.clear();
  _id_to_elem_handle.clear();
}

void MoabUserObject::addElem(dof_id_type id,moab::EntityHandle ent)
{
  _elem_handle_to_id[ent]=id;
  _id_to_elem_handle[id]=ent;
}

void
MoabUserObject::setSolution(unsigned int iSysNow,  unsigned int iVarNow, std::vector< double > &results, double scaleFactor, bool normToVol)
{

  if(!hasProblem())
    throw std::logic_error("FE problem was not set");

  // Fetch a reference to our system
  libMesh::System& sys = systems().get_system(iSysNow);

  // Ensure we map our bins onto unique indices
  std::set<dof_id_type> sol_indices;

  // Loop over mesh filter bins
  for(size_t iBin=0; iBin< results.size(); iBin++){

    // Result for this bin
    double result = results.at(iBin);

    // Scale the result
    result*=scaleFactor;

    // Convert the bin index to a libmesh id
    dof_id_type id = bin_index_to_elem_id(iBin);

    if(normToVol){
      // Fetch the volume of element
      double vol = elemVolume(id);
      // Normalise result to the element volume
      result /= vol;
    }

    // Get the solution index for this element
    dof_id_type index = elem_id_to_soln_index(iSysNow,iVarNow,id);

    // Check we haven't used this index already
    if(sol_indices.find(index)!= sol_indices.end()){
      throw std::logic_error("OpenMC indices non-uniquely map to solution indices.");
    }
    sol_indices.insert(index);

    // Set the solution for this index
    sys.solution->set(index,result);
  }

  sys.solution->close();

}

double
MoabUserObject::elemVolume(dof_id_type id)
{
  return mesh().elem_ref(id).volume();
}

dof_id_type
MoabUserObject::elem_id_to_soln_index(unsigned int iSysNow, unsigned int iVarNow, dof_id_type id)
{

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

dof_id_type
MoabUserObject::bin_index_to_elem_id(unsigned int index)
{
  // Convert the bin index to an entity handle
  if(index > _elem_handle_to_id.size() )
    throw std::out_of_range("Bin index is out of range.");

  // Conversion assumes ent handles were set contiguously in OpenMC
  moab::EntityHandle ent = (_elem_handle_to_id.begin())->first + index;

  if(_elem_handle_to_id.find(ent)==_elem_handle_to_id.end())
    throw std::logic_error("Unknown entity handle");

  // Convert the entity handle to a libMesh id
  dof_id_type id = _elem_handle_to_id[ent];

  return id;
}

bool
MoabUserObject::sortElemsByResults()
{

  // 1) Clear any prior data;
  resetContainers();

  // 2) Get the system and variable number
  libMesh::System* sysPtr;
  unsigned int iSysNow;
  unsigned int iVarNow;
  try{
    sysPtr = &system(var_name);
    iSysNow = sysPtr->number();
    iVarNow = sysPtr->variable_number(var_name);
  }
  catch(std::exception &e){
    std::cerr<<e.what()<<std::endl;
    return false;
  }

  // Set representing underflow bin
  std::set<dof_id_type> underflowElems;
  // Set representing overflow bin
  std::set<dof_id_type> overflowElems;


  // 3) Outer loop over subdomains (= mats)
  // Fix me:
  // variable names -> materialbase -> subdomains.
  // only want unique group index for individual materials.
  for(size_t iMat=0; iMat<nMatBins; ++iMat){

    // 4) Loop over all elems in subdomain (starts at 1)
    auto itelem = mesh().active_subdomain_elements_begin(iMat+1);
    auto endelem = mesh().active_subdomain_elements_end(iMat+1);
    for( ; itelem!=endelem; ++itelem){

      Elem& elem = **itelem;
      dof_id_type id = elem.id();

      // Check the number of components for this var
      unsigned int n_components = elem.n_comp(iSysNow,iVarNow);
      if(n_components != 1){
        std::cout<< "Unexpected number of expected solution components: "<<n_components<<std::endl;
        return false;
      }

      // Get the degree of freedom number for this var
      dof_id_type soln_index = elem.dof_number(iSysNow,iVarNow,0);

      // Get the solution value
      // NB this won't work for nodal variables.
      // Need to devise alternative method here, e.g mesh_function
      double result = sysPtr->solution->el(soln_index);

      // Calculate the bin number for this value
      int iBin = getResultsBin(result);

      // Bin number can be out of range if user specified range doesn't fully contain results
      if(iBin < 0) underflowElems.insert(id);
      else if(iBin >= nVarBins) overflowElems.insert(id);
      else{
        // Cast as size_t to avoid any overflow issues if we have a lot of subdomains.
        size_t iSortBin = iMat*nVarBins + iBin;
        sortedElems.at(iSortBin).insert(id);
      }
    }
  }
  // Report how many elems fell outside the range
  // TODO - Should it be an error?
  if(!underflowElems.empty() || !overflowElems.empty() ){
    size_t outside = underflowElems.size() + overflowElems.size();
    std::cout<<"Warning: "<< outside
             << " elements fell outside the specified bin range for the variable "
             << var_name
             <<std::endl;
  }

  return true;

}

bool
MoabUserObject::groupLocalElems()
{

  try{
    // Find all neighbours in mesh
    mesh().find_neighbors();

    // Loop over variable bins
    for(size_t iBin=0; iBin<nSortBins; iBin++){
      groupLocalElems(sortedElems.at(iBin),elemGroups.at(iBin));
      if(!elemGroups.at(iBin).empty())
        std::cout<<"Found "<< elemGroups.at(iBin).size()<< " local regions in T bin. "<< iBin<<std::endl;

    }
  }
  catch(std::exception &e){
    std::cerr<<e.what()<<std::endl;
    return false;
  }

  return true;
}

void
MoabUserObject::groupLocalElems(std::set<dof_id_type> elems, std::vector<moab::Range>& localElems)
{
  while(!elems.empty()){

    // Create a new local range of moab handles
    moab::Range local;

    // Retrieve and remove the fisrt elem
    auto it = elems.begin();
    dof_id_type next = *it;
    elems.erase(it);

    std::set<dof_id_type> neighbors;
    neighbors.insert(next);

    while(!neighbors.empty()){

      std::set<dof_id_type> new_neighbors;

      // Loop over all the new neighbors
      for(auto& next : neighbors){

        // Get the MOAB handle, and add to local set
        moab::EntityHandle ent = _id_to_elem_handle[next];
        local.insert(ent);

        // Get the libMesh element
        Elem& elem = mesh().elem_ref(next);

        // How many nearest neighbors (general element)?
        unsigned int NN = elem.n_neighbors();

        // Loop over neighbors
        for(unsigned int i=0; i<NN; i++){

          const Elem * nnptr = elem.neighbor_ptr(i);
          // If on boundary, some may be null ptrs
          if(nnptr == nullptr) continue;

          dof_id_type idnn = nnptr->id();

          // Select only those that are in the current bin
          if(elems.find(idnn)!= elems.end()){
            new_neighbors.insert(idnn);
            // Remove from those still available
            elems.erase(idnn);
          }

        }// End loop over new neighbors

      }// End loop over previous neighbors

      // Found all the new neighbors, done with current set.
      neighbors = new_neighbors;

    }
    // Done, no more local neighbors in the current bin.

    // Save this moab range of local neighbors
    if(!local.empty())
      std::cout<<"local region contains "<<local.size()<<" elements"<<std::endl;
    localElems.push_back(local);
  }
  // Done, assigned all elems in bin to a local range.
 }

void
MoabUserObject::resetContainers()
{
  // Get number of subdomains
  nMatBins = mesh().n_subdomains();
  nSortBins = nMatBins*nVarBins;
  sortedElems.clear();
  sortedElems.resize(nSortBins);
  elemGroups.clear();
  elemGroups.resize(nSortBins);
}

int
MoabUserObject::getResultsBin(double value)
{
  if(logscale) return getResultsBinLog(value);
  else return getResultsBinLin(value);
}

inline int
MoabUserObject::getResultsBinLin(double value)
{
  return int(floor((value-var_min)/bin_width));
}

int
MoabUserObject::getResultsBinLog(double value)
{

  // Get the power of 10
  double powFloat = log10(value);

  // Round down power to nearest int
  double powMajor = floor(powFloat);

  // Get the major  bin
  int iMajor = int(powMajor)-powMin;

  // Get the minor bin
  int iMinor = int(floor( (powFloat - powMajor)*double(nMinor) ));

  // Get bin - can be out of range if results are not inside the limits specified by user
  int iBin = nMinor*iMajor + iMinor;

  return iBin;

}
