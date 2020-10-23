// Moose includes
#include "MoabUserObject.h"

registerMooseObject("OpenMCApp", MoabUserObject);

template <>
InputParameters
validParams<MoabUserObject>()
{
  InputParameters params = validParams<UserObject>();
  return params;
}

MoabUserObject::MoabUserObject(const InputParameters & parameters) :
  UserObject(parameters),
  _problem_ptr(nullptr)
{
  // Create MOAB interface
  moabPtr =  std::make_shared<moab::Core>();
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

  rval = createElems(node_id_to_handle,_elem_handle_to_id);
  if(rval!=moab::MB_SUCCESS)
    throw std::logic_error("Could not create elems");
}

// // bool
// MoabUserObject::initSystem(std::string var_now)
// {

//   try
//     {
//       libMesh::System& sys = system(var_name);      
//       unsigned int iSys = sys.number();
//       iVar = sys.variable_number(var_name);      
//     }
//   catch(std::exception &e)
//     {
//       std::cerr<<e.what()<<std::endl;
//       return false;
//     }  
//   // // Add a new system and fetch a reference
//   // libMesh::System& sys = systems().add_system("Basic","OpenMCsys");
//   // sys.set_basic_system_only();

//   // // Set ID to retrieve system later
//   // iSys = sys.number();
  
//   // // Add a new variable and save index
//   // iVar = sys.add_variable("heating",libMesh::Order::CONSTANT,libMesh::FEFamily::MONOMIAL);
  
//   // Initialise the data structures for the equation systems
//   systems().init();

//   return true;
// }

// Pass the results for named variable into the libMesh systems solution
bool
MoabUserObject::setSolution(std::string var_now,std::vector< double > &results)
{

  try
    {
      libMesh::System& sys = system(var_now);      
      unsigned int iSys = sys.number();
      unsigned int iVar = sys.variable_number(var_now);
      setSolution(iSys,iVar,results);

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
MoabUserObject::createElems(std::map<dof_id_type,moab::EntityHandle>& node_id_to_handle,std::map<dof_id_type,moab::EntityHandle>& elem_handle_to_id)
{

  if(!hasProblem()) return moab::MB_FAILURE;

  moab::ErrorCode rval(moab::MB_SUCCESS);

  // Clear prior results.
  elem_handle_to_id.clear();
  
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
      elem_handle_to_id.clear();
      return rval;
    }
    elem_handle_to_id[ent]=id;
    
  }
  
  return rval;  
}

void
MoabUserObject::setSolution(unsigned int iSysNow,  unsigned int iVarNow, std::vector< double > &results)
{

  if(!hasProblem())
    throw std::logic_error("FE problem was not set");
  
  // Fetch a reference to our system
  // TODO debug this line!  
  libMesh::System& sys = systems().get_system(iSysNow);

  // Ensure we map our bins onto unique indices
  std::set<dof_id_type> sol_indices;

  // Get the number of bins
  uint32_t nBins = results.size();
  
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
    
    // Set the solution for this index
    sys.solution->set(index,result);
  }

  sys.solution->close();
  
}
                      
dof_id_type
MoabUserObject::bin_index_to_soln_index(unsigned int iSysNow, unsigned int iVarNow, unsigned int index)
{

  // Convert the bin index to a libmesh id
  dof_id_type id = bin_index_to_elem_id(index);

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
