// Moose includes
#include "MoabUserObject.h"

registerMooseObject("OpenMCApp", MoabUserObject);

template <>
InputParameters
validParams<MoabUserObject>()
{
  InputParameters params = validParams<UserObject>();

  params.addParam<std::string>("bin_varname", "", "Variable name by whose results elements should be binned.");
  params.addParam<double>("var_min", 300.,"Minimum value to define range of bins.");
  params.addParam<unsigned int>("n_pow", 6, "Number of powers in which to bin in a log scale.");
  params.addParam<unsigned int>("n_minor", 5, "Number of minor divisions in which to bin in a log scale.");

  return params;
}

MoabUserObject::MoabUserObject(const InputParameters & parameters) :
  UserObject(parameters),
  _problem_ptr(nullptr),
  var_name(getParam<std::string>("bin_varname")),
  var_min(getParam<double>("var_min")),
  nPow(getParam<unsigned int>("n_pow")),
  nMinor(getParam<unsigned int>("n_minor"))
{
  // Create MOAB interface
  moabPtr =  std::make_shared<moab::Core>();

  // Set variables relating to binning
  binElems = ( var_name != "" );
  nBins = nPow*nMinor;
  if(binElems){

    if(var_min <= 0.){
      mooseError("var_min out of range! Please pick a value > 0");
    }
    powMin = int(floor(log10(var_min)));

    // edges.resize(nBins+1);
    // midpoints.resize(nBins);
  }
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

  if(!hasProblem()) return  moab::MB_FAILURE;

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
MoabUserObject::createElems(std::map<dof_id_type,moab::EntityHandle>& node_id_to_handle)
{

  if(!hasProblem()) return moab::MB_FAILURE;

  moab::ErrorCode rval(moab::MB_SUCCESS);

  // Clear prior results.
  clearElemMaps();

  // Initialise an array for moab connectivity
  unsigned int nNodes = 4;
  moab::EntityHandle conn[nNodes];

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
    addElem(id,ent);
  }

  return rval;
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

  // Get the number of bins
  uint32_t nBins = results.size();

  // Loop over mesh filter bins
  for(uint32_t iBin=0; iBin< nBins; iBin++){

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

  // 3) Loop over all elems in mesh
  // NB this won't work for nodal variables.
  // Need to devise alternative method here.
  auto itelem = mesh().elements_begin();
  auto endelem = mesh().elements_end();
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
    double result = sysPtr->solution->el(soln_index);

    // Calculate the bin number for this value
    int iBin = getResultsBin(result);

    // Bin number can be out of range if user specified range doesn't fully contain results
    if(iBin < 0) underflowElems.insert(id);
    else if(iBin >= nBins) overflowElems.insert(id);
    else sortedElems.at(iBin).insert(id);

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
    for(unsigned int iBin=0; iBin<nBins; iBin++){
      groupLocalElems(sortedElems.at(iBin),elemGroups.at(iBin));
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
    localElems.push_back(local);
  }
  // Done, assigned all elems in bin to a local range.
 }

void
MoabUserObject::resetContainers()
{
  sortedElems.clear();
  sortedElems.resize(nBins);
  elemGroups.clear();
  elemGroups.resize(nBins);
}

int
MoabUserObject::getResultsBin(double value)
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
