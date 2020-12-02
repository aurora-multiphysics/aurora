// Moose includes
#include "MoabUserObject.h"

registerMooseObject("OpenMCApp", MoabUserObject);

template <>
InputParameters
validParams<MoabUserObject>()
{
  InputParameters params = validParams<UserObject>();

  // MOAB mesh params
  params.addParam<double>("length_scale", 1.,"Scale factor to convert lengths from MOOSE to MOAB");

  // Params relating to binning
  params.addParam<std::string>("bin_varname", "", "Variable name by whose results elements should be binned.");
  params.addParam<double>("var_min", 297.5,"Minimum value to define range of bins.");
  params.addParam<double>("var_max", 597.5,"Max value to define range of bins.");
  params.addParam<bool>("logscale", false, "Switch to determine if logarithmic binning should be used.");
  params.addParam<unsigned int>("n_bins", 60, "Number of bins");

  // Mesh metadata
  params.addParam<std::vector<std::string> >("material_names", std::vector<std::string>(), "List of material names");

  // Dagmc params
  params.addParam<double>("faceting_tol",1.e-4,"Faceting tolerance for DagMC");
  params.addParam<double>("geom_tol",1.e-6,"Geometry tolerance for DagMC");

  // Output params
  params.addParam<bool>("output_skins", false, "Switch to control whether MOAB should write skins to file.");
  params.addParam<std::string>("output_base", "moab_surfs", "Base filename for file writes (will be appended by integer");
  params.addParam<unsigned int>("n_output", 10, "Number of times to write to file if output_skins is true");
  params.addParam<unsigned int>("n_skip", 0, "Number of iterations to skip between writes if output_skins is true and n_output>1");

  return params;
}

// TO-DO automate the supplying of materials
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
  geom_tol(getParam<double>("geom_tol")),
  output_skins(getParam<bool>("output_skins")),
  output_base(getParam<std::string>("output_base")),
  n_output(getParam<unsigned int>("n_output")),
  n_period(getParam<unsigned int>("n_skip")+1)
{
  // Create MOAB interface
  moabPtr =  std::make_shared<moab::Core>();

  // Create a skinner
  skinner = std::make_unique<moab::Skinner>(moabPtr.get());

  // Create a geom topo tool
  gtt = std::make_unique<moab::GeomTopoTool>(moabPtr.get());

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
  calcMidpoints();

  // Set variables relating to writing to file
  n_write=0;
  n_its=0;
}

FEProblemBase&
MoabUserObject::problem()
{
  if(_problem_ptr == nullptr)
    mooseError("No problem was set");

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
    mooseError("Failed to set MOAB dimension");

  //Create a meshset
  rval = moabPtr->create_meshset(moab::MESHSET_SET,meshset);
  if(rval!=moab::MB_SUCCESS)
    mooseError("Failed to create mesh set");

  rval = createTags();
  if(rval!=moab::MB_SUCCESS)
    mooseError("Could not set up tags");

  std::map<dof_id_type,moab::EntityHandle> node_id_to_handle;
  rval = createNodes(node_id_to_handle);
  if(rval!=moab::MB_SUCCESS)
    mooseError("Could not create nodes");

  rval = createElems(node_id_to_handle);
  if(rval!=moab::MB_SUCCESS)
    mooseError("Could not create elems");

  // Find which elements belong to which materials
  findMaterials();
}

bool
MoabUserObject::update()
{
  // Don't attempt to bin results if we haven't been provided with a variable
  if(!binElems) return false;

  // Clear existing entity sets
  if(!resetMOAB()) return false;

  // Sort libMesh elements into bins of the specified variable
  if(!sortElemsByResults()) return false;

  // Find the surfaces of local temperature regions
  if(!findSurfaces()) return false;

  return true;
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

void
MoabUserObject::findMaterials()
{
  // Clear any prior data.
  mat_blocks.clear();

  std::set<SubdomainID> unique_blocks;

  // Loop over the materials provided by the user
  for(const auto & mat : mat_names){
    // Look for the material
    std::shared_ptr< MaterialBase > mat_ptr = problem().getMaterial(mat, Moose::BLOCK_MATERIAL_DATA);

    if(mat_ptr == nullptr){
      mooseError("Could not find material "+mat );
    }

    // Get the element blocks corresponding to this mat.
    std::set<SubdomainID> blocks = mat_ptr->blockIDs();

    // Check that all the blocks are unique
    unsigned int nblks_before = unique_blocks.size();
    unsigned int nblks_new = blocks.size();
    unique_blocks.insert(blocks.begin(),blocks.end());
    if(unique_blocks.size() != (nblks_before+nblks_new) ){
      mooseError("Some blocks appear in more than one material.");
    }

    // Save list
    mat_blocks.push_back(blocks);
  }

  // Save number of materials
  nMatBins = mat_blocks.size();
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

  // Initialise an array for moab connectivity
  unsigned int nNodes = 4;
  moab::EntityHandle conn[nNodes];
  moab::Range all_elems;

  // Iterate over elements in the mesh
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

    // Save the handle for adding to entity sets
    all_elems.insert(ent);
  }

  // Add the elems to the full meshset
  rval = moabPtr->add_entities(meshset,all_elems);

  return rval;
}

moab::ErrorCode
MoabUserObject::createTags()
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

  // Set the values for DagMC faceting / geometry tolerance tags on the mesh entity set
  rval = moabPtr->tag_set_data(faceting_tol_tag, &meshset, 1, &faceting_tol);
  if(rval!=moab::MB_SUCCESS)  return rval;

  rval = moabPtr->tag_set_data(geometry_resabs_tag, &meshset, 1, &geom_tol);
  return rval;
}

moab::ErrorCode
MoabUserObject::createGroup(unsigned int id, std::string name,moab::EntityHandle& group_set)
{
  // Create a new mesh set
  moab::ErrorCode rval = moabPtr->create_meshset(moab::MESHSET_SET,group_set);
  if(rval!=moab::MB_SUCCESS) return rval;

  // Set the tags for this material
  return setTags(group_set,name,"Group",id,4);
}


moab::ErrorCode
MoabUserObject::createVol(unsigned int id,moab::EntityHandle& volume_set,moab::EntityHandle group_set)
{
  moab::ErrorCode rval = moabPtr->create_meshset(moab::MESHSET_SET,volume_set);
  if(rval!=moab::MB_SUCCESS) return rval;

  rval =  setTags(volume_set,"","Volume",id,3);
  if(rval != moab::MB_SUCCESS) return rval;

  // Add the volume to group
  rval = moabPtr->add_entities(group_set, &volume_set,1);
  if(rval != moab::MB_SUCCESS) return rval;

  return rval;
}

moab::ErrorCode
MoabUserObject::createSurf(unsigned int id,moab::EntityHandle& surface_set, moab::Range& faces,  std::vector<VolData> & voldata)
{
  // Create meshset
  moab::ErrorCode rval = moabPtr->create_meshset(moab::MESHSET_SET,surface_set);
  if(rval!=moab::MB_SUCCESS) return rval;

  // Set tags
  rval = setTags(surface_set,"","Surface",id,2);
  if(rval!=moab::MB_SUCCESS) return rval;

  // Add tris to the surface
  rval = moabPtr->add_entities(surface_set,faces);
  if(rval != moab::MB_SUCCESS) return rval;

  // Create entry in map
  surfsToVols[surface_set] = std::vector<VolData>();

  // Add volume to list associated with this surface
  for(const auto & data : voldata){
    rval = updateSurfData(surface_set,data);
    if(rval != moab::MB_SUCCESS) return rval;
  }

  return moab::MB_SUCCESS;
}

moab::ErrorCode
MoabUserObject::updateSurfData(moab::EntityHandle surface_set,VolData data)
{

  // Add the surface to the volume set
  moab::ErrorCode rval = moabPtr->add_parent_child(data.vol,surface_set);
  if(rval != moab::MB_SUCCESS) return rval;

  // Set the surfaces sense
  rval = gtt->set_sense(surface_set,data.vol,int(data.sense));
  if(rval != moab::MB_SUCCESS) return rval;

  // Save
  surfsToVols[surface_set].push_back(data);

  return moab::MB_SUCCESS;
}


moab::ErrorCode
MoabUserObject::setTags(moab::EntityHandle ent, std::string name, std::string category, unsigned int id, int dim)
{

  moab::ErrorCode rval;

  // Set the name tag
  if(name!=""){
    rval = setTagData(name_tag,ent,name,NAME_TAG_SIZE);
    if(rval!=moab::MB_SUCCESS) return rval;
  }

  // Set the category tag
  if(category!=""){
    rval = setTagData(category_tag,ent,category,CATEGORY_TAG_SIZE);
    if(rval!=moab::MB_SUCCESS) return rval;
  }

  // Set the dimension tag
  rval = setTagData(geometry_dimension_tag,ent,&dim);
  if(rval!=moab::MB_SUCCESS) return rval;

  // Set the id tag
  rval = setTagData(id_tag,ent,&id);
  return rval;

}

moab::ErrorCode
MoabUserObject::setTagData(moab::Tag tag, moab::EntityHandle ent, std::string data, unsigned int SIZE)
{
  char namebuf[SIZE];
  memset(namebuf,'\0', SIZE); // fill C char array with null
  strncpy(namebuf,data.c_str(),SIZE-1);
  return moabPtr->tag_set_data(tag,&ent,1,namebuf);
}

moab::ErrorCode
MoabUserObject::setTagData(moab::Tag tag, moab::EntityHandle ent, void* data)
{
  return moabPtr->tag_set_data(tag,&ent,1,data);
}

void
MoabUserObject::clearElemMaps()
{
  _elem_handle_to_id.clear();
  _id_to_elem_handle.clear();
}

void
MoabUserObject::addElem(dof_id_type id,moab::EntityHandle ent)
{
  _elem_handle_to_id[ent]=id;
  _id_to_elem_handle[id]=ent;
}

void
MoabUserObject::setSolution(unsigned int iSysNow,  unsigned int iVarNow, std::vector< double > &results, double scaleFactor, bool normToVol)
{

  if(!hasProblem())
    mooseError("FE problem was not set");

  // Fetch a reference to our system
  libMesh::System& sys = systems().get_system(iSysNow);

  // Ensure we map our bins onto unique indices
  std::set<dof_id_type> sol_indices;

  // Loop over mesh filter bins
  for(unsigned int iBin=0; iBin< results.size(); iBin++){

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
      mooseError("OpenMC indices non-uniquely map to solution indices.");
    }
    sol_indices.insert(index);

    // Set the solution for this index
    sys.solution->set(index,result);
  }

  sys.solution->close();

}

double
MoabUserObject::getTemperature(moab::EntityHandle vol)
{
  if(volToTemp.find(vol)==volToTemp.end()){
    throw std::out_of_range("Could not find volume");
  }
  return volToTemp[vol];
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
    mooseError("Unexpected number of expected solution components");
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
    mooseError("Bin index is out of range.");

  // Conversion assumes ent handles were set contiguously in OpenMC
  moab::EntityHandle ent = (_elem_handle_to_id.begin())->first + index;

  if(_elem_handle_to_id.find(ent)==_elem_handle_to_id.end())
    mooseError("Unknown entity handle");

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

  // Outer loop over materials
  for(unsigned int iMat=0; iMat<nMatBins; iMat++){

    // Get the subdomains for this material
    std::set<SubdomainID>& blocks = mat_blocks.at(iMat);

    // Iterate over elements in this materials
    auto itelem = mesh().active_subdomain_set_elements_begin(blocks);
    auto endelem = mesh().active_subdomain_set_elements_end(blocks);
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

      // Sort elems into a bin
      if(iBin < 0) underflowElems.insert(id);
      else if(iBin >= nVarBins) overflowElems.insert(id);
      else{
        unsigned int iSortBin = iMat*nVarBins + iBin;
        sortedElems.at(iSortBin).insert(id);
      }
    }
  }

  // Report how many elems fell outside the range
  // TODO - Should it be an error?
  if(!underflowElems.empty() || !overflowElems.empty() ){
    unsigned int outside = underflowElems.size() + overflowElems.size();
    std::cout<<"Warning: "<< outside
             << " elements fell outside the specified bin range for the variable "
             << var_name
             <<std::endl;
  }

  return true;

}

bool
MoabUserObject::findSurfaces()
{

  moab::ErrorCode rval = moab::MB_SUCCESS;
  try{
    // Find all neighbours in mesh
    mesh().find_neighbors();

    // Counter for volumes
    unsigned int vol_id=0;

    // Counter for surfaces
    unsigned int surf_id=0;

    // Loop over material bins
    for(unsigned int iMat=0; iMat<nMatBins; iMat++){

      // Get the material name:
      std::string mat_name = "mat:"+mat_names.at(iMat);

      // Create a material group
      moab::EntityHandle group_set;
      rval = createGroup(iMat+1,mat_name,group_set);
      if(rval != moab::MB_SUCCESS) return false;

      // Loop over variable bins
      for(unsigned int iVar=0; iVar<nVarBins; iVar++){

        unsigned int iBin = iMat*nVarBins + iVar;

        std::vector<moab::Range> regions;
        groupLocalElems(sortedElems.at(iBin),regions);

        // Retrieve the average bin temperature
        double temp = midpoints.at(iVar);

        // Loop over all regions and find surfaces
        for(const auto & region : regions){
          moab::EntityHandle volume_set;
          if(!findSurface(region,group_set,vol_id,surf_id,volume_set)){
            return false;
          }
          // Save the volume temperature
          volToTemp[volume_set] = temp;
        }

      }
    } // End loop over materials

    // Finally, build a graveyard
    rval = buildGraveyard(vol_id,surf_id);
    if(rval != moab::MB_SUCCESS) return false;

  }
  catch(std::exception &e){
    std::cerr<<e.what()<<std::endl;
    return false;
  }

  // Optionally write to file
  if(output_skins){
    if(!writeSurfaces()) return false;
  }

  return true;
}

bool
MoabUserObject::writeSurfaces()
{

  if(n_write >= n_output){
    output_skins; // Don't write any more times
    return true;
  }

  if( (n_its % n_period) == 0 ){

    // Generate list of surfaces to write.
    std::vector<moab::EntityHandle> surfs;
    for( auto itsurf : surfsToVols){
      surfs.push_back(itsurf.first);
    }
    std::string filename = output_base + "_" + std::to_string(n_write) +".h5m";
    std::cout << "Writing MOAB mesh to "<< filename << std::endl;
    moab::ErrorCode rval = moabPtr->write_mesh(filename.c_str(),surfs.data(),surfs.size());
    if(rval != moab::MB_SUCCESS) return false;
    n_write++;
  }

  n_its++;
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
  nSortBins = nMatBins*nVarBins;
  sortedElems.clear();
  sortedElems.resize(nSortBins);
  volToTemp.clear();
}

bool
MoabUserObject::resetMOAB()
{

  moab::ErrorCode rval = moab::MB_SUCCESS;

  std::vector<std::string> categories;
  categories.push_back("Surface");
  categories.push_back("Volume");
  categories.push_back("Group");

  // Retrieve and delete the entities in each category
  for(const auto & data : categories){

    char category[CATEGORY_TAG_SIZE];
    memset(category,'\0', CATEGORY_TAG_SIZE); // fill C char array with null
    strncpy(category,data.c_str(),CATEGORY_TAG_SIZE-1);
    const void* const val[] = {&category};

    moab::Range ents;
    rval = moabPtr->get_entities_by_type_and_tag(0, moab::MBENTITYSET, &category_tag,
                                                 val, 1, ents);
    if(rval != moab::MB_SUCCESS) return false;

    rval = moabPtr->delete_entities(ents);
    if(rval != moab::MB_SUCCESS) return false;
  }

  // Clear data structures in this class
  surfsToVols.clear();

  // Done
  return true;
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

void
MoabUserObject::calcMidpoints()
{
  if(logscale) calcMidpointsLog();
  else calcMidpointsLin();
}


void
MoabUserObject::calcMidpointsLin()
{
  double var_now = var_min - bin_width/2.0;
  for(unsigned int iVar=0; iVar<nVarBins; iVar++){
    var_now += bin_width;
    midpoints.push_back(var_now);
  }
}

void
MoabUserObject::calcMidpointsLog()
{
  double powDiff = 1./double(nMinor);
  double powStart = double(powMin) - 0.5*powStart;
  double var_now = pow(10,powStart);
  double prodDiff = pow(10,powDiff);
  for(unsigned int iVar=0; iVar<nVarBins; iVar++){
    var_now *= prodDiff;
    midpoints.push_back(var_now);
  }
}


bool
MoabUserObject::findSurface(const moab::Range& region,moab::EntityHandle group, unsigned int & vol_id, unsigned int & surf_id,moab::EntityHandle& volume_set)
{

  moab::ErrorCode rval;

  // Create a volume set
  vol_id++;
  rval = createVol(vol_id,volume_set,group);
  if(rval != moab::MB_SUCCESS) return false;

  // Find surfaces from these regions
  moab::Range tris; // The tris of the surfaces
  moab::Range rtris;  // The tris which are reversed with respect to their surfaces
  rval = skinner->find_skin(0,region,false,tris,&rtris);
  if(rval != moab::MB_SUCCESS) return false;

  // Create surface sets for the forwards tris
  VolData vdata = {volume_set,Sense::FORWARDS};
  rval = createSurfaces(tris,vdata,surf_id);
  if(rval != moab::MB_SUCCESS) return false;

  // Create surface sets for the reversed tris
  vdata.sense =Sense::BACKWARDS;
  rval = createSurfaces(rtris,vdata,surf_id);
  if(rval != moab::MB_SUCCESS) return false;

  return true;
}


moab::ErrorCode
MoabUserObject::createSurfaces(moab::Range& faces, VolData& voldata, unsigned int& surf_id){

  moab::ErrorCode rval = moab::MB_SUCCESS;

  if(faces.empty()) return rval;

  // Loop over the surfaces we have already created
  for ( const auto & surfpair : surfsToVols ) {

    // Local copies of surf/vols
    moab::EntityHandle surf = surfpair.first;
    std::vector<VolData> vols = surfpair.second;

    // First get the entities in this surface
    moab::Range tris;
    rval = moabPtr->get_entities_by_handle(surf,tris);
    if(rval!=moab::MB_SUCCESS) return rval;

    // Find any tris that live in both surfs
    moab::Range overlap = moab::intersect(tris,faces);
    if(!overlap.empty()) {

      // Check if the tris are a subset or the entire surf
      if(tris.size()==overlap.size()){
        // Whole surface -> Just need to update the volume relationships
        rval = updateSurfData(surf,voldata);
      }
      else{
        // If overlap is subset, subtract shared tris from this surface and create a new shared surface
        rval = moabPtr->remove_entities(surf,overlap);
        if(rval!=moab::MB_SUCCESS) return rval;

        // Append our new volume to the list that share this surf
        vols.push_back(voldata);

        // Create a new shared surface
        moab::EntityHandle shared_surf;
        surf_id++;
        rval = createSurf(surf_id,shared_surf,overlap,vols);
        if(rval!=moab::MB_SUCCESS) return rval;
      }

      // Subtract from the input list
      for( auto& shared : overlap ){
        faces.erase(shared);
      }
      if(faces.empty()) break;
    }
  }

  if(!faces.empty()){
    moab::EntityHandle surface_set;
    std::vector<VolData> voldatavec(1,voldata);
    surf_id++;
    rval = createSurf(surf_id,surface_set,faces,voldatavec);
    if(rval != moab::MB_SUCCESS) return rval;
  }

  return rval;
}

moab::ErrorCode MoabUserObject::buildGraveyard( unsigned int & vol_id, unsigned int & surf_id)
{
  moab::ErrorCode rval(moab::MB_SUCCESS);

  // Create the graveyard set
  moab::EntityHandle graveyard;
  unsigned int id = nMatBins+1;
  std::string mat_name = "mat:Graveyard";
  rval = createGroup(id,mat_name,graveyard);
  if(rval != moab::MB_SUCCESS) return rval;

  // Create a volume set
  moab::EntityHandle volume_set;
  vol_id++;
  rval = createVol(vol_id,volume_set,graveyard);
  if(rval != moab::MB_SUCCESS) return rval;

  // Set up for the volume data to pass to surfs
  VolData vdata = {volume_set,Sense::FORWARDS};

  // Find a bounding box
  BoundingBox bbox =  MeshTools::create_bounding_box(mesh());

  // Create inner surface from the box with normals pointing out of box
  // Rescale by 1% to avoid having to imprint
  rval = createSurfaceFromBox(bbox,vdata,surf_id,true,1.01);
  if(rval != moab::MB_SUCCESS) return rval;

  // Create outer surface with face normals pointing into the box
  // Rescale by 10% to create a volume
  rval = createSurfaceFromBox(bbox,vdata,surf_id,false,1.1);
  return rval;
}

moab::ErrorCode
MoabUserObject::createSurfaceFromBox(const BoundingBox& box, VolData& voldata, unsigned int& surf_id, bool normalout, double factor)
{
  // Create the vertices of the box
  std::vector<moab::EntityHandle> vert_handles;
  moab::ErrorCode rval = createNodesFromBox(box,factor,vert_handles);
  if(rval!=moab::MB_SUCCESS) return rval;
  else if(vert_handles.size() != 8) mooseError("Failed to get box coords");

  // Create the tris in 4 groups of 3 (4 open tetrahedra)
  moab::Range tris;
  rval = createCornerTris(vert_handles,0,1,2,4,normalout,tris);
  if(rval!=moab::MB_SUCCESS) return rval;

  rval = createCornerTris(vert_handles,3,2,1,7,normalout,tris);
  if(rval!=moab::MB_SUCCESS) return rval;

  rval = createCornerTris(vert_handles,6,4,2,7,normalout,tris);
  if(rval!=moab::MB_SUCCESS) return rval;

  rval = createCornerTris(vert_handles,5,1,4,7,normalout,tris);
  if(rval!=moab::MB_SUCCESS) return rval;

  moab::EntityHandle surface_set;
  std::vector<VolData> voldatavec(1,voldata);
  surf_id++;
  return createSurf(surf_id,surface_set,tris,voldatavec);
}

moab::ErrorCode
MoabUserObject::createNodesFromBox(const BoundingBox& box,double factor,std::vector<moab::EntityHandle>& vert_handles)
{
  moab::ErrorCode rval(moab::MB_SUCCESS);

  // Fetch the vertices of the box
  std::vector<Point> verts = boxCoords(box,factor);
  if(verts.size() != 8) mooseError("Failed to get box coords");

  // Array to represent a coord in moab
  double coord[3];
  // Create the vertices in moab and get the handles
  for(const auto & vert : verts){
    coord[0]=vert(0);
    coord[1]=vert(1);
    coord[2]=vert(2);

    moab::EntityHandle ent;
    rval = moabPtr->create_vertex(coord,ent);
    if(rval!=moab::MB_SUCCESS){
      vert_handles.clear();
      return rval;
    }
    vert_handles.push_back(ent);
  }
  return rval;
}

std::vector<Point>
MoabUserObject::boxCoords(const BoundingBox& box, double factor)
{
  Point minpoint = box.min();
  Point maxpoint = box.max();
  Point diff = (maxpoint - minpoint)/2.0;
  Point origin = minpoint + diff;

  // Rescale (half)sides of box
  diff *= factor;

  // modify minpoint
  minpoint = origin - diff;

  // Vectors for sides of box
  Point dx(2.0*diff(0),0.,0.);
  Point dy(0.,2.0*diff(1),0.);
  Point dz(0.,0.,2.0*diff(2));

  // Start at (-,-,-) and add side vectors
  std::vector<Point> verts(8,minpoint);
  for(unsigned int idz=0; idz<2; idz++){
    for(unsigned int idy=0; idy<2; idy++){
      for(unsigned int idx=0; idx<2; idx++){
        unsigned int ibin = 4*idz + 2*idy+ idx;
        verts.at(ibin) += double(idx)*dx + double(idy)*dy + double(idz)*dz;
      }
    }
  }

  return verts;
}

moab::ErrorCode
MoabUserObject::createCornerTris(const std::vector<moab::EntityHandle> & verts,
                                 unsigned int corner,
                                 unsigned int v1, unsigned int v2 ,unsigned int v3,
                                 bool normalout, moab::Range &surface_tris)
{
  // Create 3 tris stemming from one corner (i.e. an open tetrahedron)
  // Assume first is the central corner, and the others are labelled clockwise looking down on the corner
  moab::ErrorCode rval = moab::MB_SUCCESS;
  unsigned int indices[3] = {v1,v2,v3};

  //Create each tri by a cyclic permutation of indices
  for(unsigned int i=0; i<3; i++){
    // v1,v2 = 0,1; 1,2; 2;0
    int v1 = indices[i%3];
    int v2 = indices[(i+1)%3];
    if(normalout){
      // anti-clockwise: normal points outwards
      rval = createTri(verts,corner,v2,v1,surface_tris);
    }
    else{
      // clockwise: normal points inwards
      rval = createTri(verts,corner,v1,v2,surface_tris);
    }
    if(rval!=moab::MB_SUCCESS) return rval;
  }
  return rval;
}

moab::ErrorCode
MoabUserObject::createTri(const std::vector<moab::EntityHandle> & vertices,unsigned int v1, unsigned int v2 ,unsigned int v3, moab::Range &surface_tris) {
  moab::ErrorCode rval = moab::MB_SUCCESS;
  moab::EntityHandle triangle;
  moab::EntityHandle connectivity[3] = { vertices[v1],vertices[v2],vertices[v3] };
  rval = moabPtr->create_element(moab::MBTRI,connectivity,3,triangle);
  surface_tris.insert(triangle);
  return rval;
}
