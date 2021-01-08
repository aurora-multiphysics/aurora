//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BasicTest.h"
#include "Executioner.h"
#include "FEProblemBase.h"
#include "MoabUserObject.h"

// Fixture to test the MOAB user object
class MoabUserObjectTestBase : public InputFileTest{
protected:
  MoabUserObjectTestBase(std::string inputfile) :
    InputFileTest(inputfile),
    foundMOAB(false)
  {};

  virtual void SetUp() override {

    // Call the base class method
    InputFileTest::SetUp();

    if(appIsNull) return;

    try{
      app->setupOptions();
      app->runInputFile();

      // Get the FE problem
      problemPtr = &(app->getExecutioner()->feProblem());

      // Check for MOAB user object
      if(!(problemPtr->hasUserObject("moab")))
        throw std::logic_error("Could not find MoabUserObject with name 'moab'. Please check your input file.");

      // Get the MOAB user object
      moabUOPtr = &(problemPtr->getUserObject<MoabUserObject>("moab"));

      foundMOAB = true;
    }
    catch(std::exception e){
      std::cout<<e.what()<<std::endl;
    }

  }

  bool setProblem(){
    // Set the fe problem ptr
    moabUOPtr->setProblem(problemPtr);

    // Return success/failure
    return moabUOPtr->hasProblem();
  }

  // Member data
  MoabUserObject* moabUOPtr;
  FEProblemBase* problemPtr;
  bool foundMOAB;
};

class MoabUserObjectTest : public MoabUserObjectTestBase {
protected:
  MoabUserObjectTest() :
    MoabUserObjectTestBase("moabuserobject.i"),
    var_name("temperature"),
    tol(1.e-9),
    faceting_tol_expect(1.e-4),
    geom_tol_expect(1.e-6),
    lengthscale(100.0) {};

  MoabUserObjectTest(std::string inputfile) :
    MoabUserObjectTestBase(inputfile),
    var_name("temperature"),
    tol(1.e-9),
    faceting_tol_expect(1.e-4),
    geom_tol_expect(1.e-6),
    lengthscale(100.0) {};

  bool checkSystem(){
    try{
      System & sys = getSys();
    }
    catch(std::runtime_error e){
      return false;
    }
    return true;
  }


  System& getSys() {
    return problemPtr->getSystem(var_name);
  }


  bool calcRadius(std::shared_ptr<moab::Interface> moabPtr,
                  moab::EntityHandle elem,
                  double& radius,
                  std::string& errmsg){

    moab::ErrorCode rval;

    // Fetch elem connectivity
    std::vector<moab::EntityHandle> conn;
    rval = moabPtr->get_connectivity(&elem, 1, conn);
    if (rval != moab::MB_SUCCESS){
      errmsg="Failed to get connectivity.";
      return false;
    }

    // Fetch vertex coords
    size_t nNodes = conn.size();
    if(nNodes==0){
      errmsg="Failed to get nodes.";
      return false;
    }

    std::vector<double> nodeCoords(nNodes*3);
    rval = moabPtr->get_coords(conn.data(), nNodes, nodeCoords.data());
    if (rval != moab::MB_SUCCESS){
      errmsg="Failed to get nodes' coords.";
      return false;
    }

    // Compute the centroid of the element vertices
    double centroid[3] = {0.,0.,0.};
    for(size_t inode=0; inode<nNodes; ++inode){
      for(int i=0; i<3; i++){
        double coord = nodeCoords.at(3*inode +i);
        centroid[i]+= coord;
      }
    }

    radius = sqrt( centroid[0]*centroid[0]
                   + centroid[1]*centroid[1]
                   + centroid[2]*centroid[2] ) / double(nNodes);

    return true;
  }

  double getRMax(){
    // Define maximum radius for box with side length 21 m
    return 10.5*lengthscale*sqrt(3);
  }

  void getElems(std::vector<moab::EntityHandle>& ents){

    // Get the MOAB interface to check the data
    std::shared_ptr<moab::Interface> moabPtr = moabUOPtr->moabPtr;
    ASSERT_NE(moabPtr,nullptr);

    // Get root set
    moab::EntityHandle rootset = moabPtr->get_root_set();

    // Get elems
    moab::ErrorCode rval = moabPtr->get_entities_by_type(rootset,moab::MBTET,ents);
    ASSERT_EQ(rval,moab::MB_SUCCESS);
  }

  double getSolution(double radius, double rMax, double max, double min){
    return ((max-min)*exp(-radius/rMax) + min);
  }

  void getSolutionData(const std::vector<moab::EntityHandle>& ents, double rMax, double solMax, double solMin, std::vector<double>& solutionData){

    // Absolute max radius
    double rAbsMax = getRMax();

    // Manufacture a solution based on radius of element centroid.
    for(const auto& ent : ents){
      double radius;
      std::string errmsg;
      bool success = calcRadius(moabUOPtr->moabPtr,ent,radius,errmsg);
      ASSERT_TRUE(success) << errmsg;
      EXPECT_GT(radius,0.);
      EXPECT_LT(radius,rAbsMax);
      double solution=getSolution(radius,rMax,solMax,solMin);
      EXPECT_GT(solution,solMin);
      EXPECT_LT(solution,solMax);
      solutionData.push_back(solution);
    }
  }

  double elemRadius(Elem& el){

    Point centroid(0.,0.,0.);
    unsigned int nNodes = el.n_nodes();
    for(unsigned int iNode=0; iNode<nNodes; ++iNode){
      // Get the point coords for this node
      const Point& point = el.point(iNode);
      centroid += point;
    }
    centroid /= double(nNodes);
    // Scale to same units as Moab
    centroid *= lengthscale;
    return centroid.norm();
  }

  void setSolution(const std::vector<moab::EntityHandle>& ents, double rMax, double solMax, double solMin, double scalefactor=1.0, bool normToVol=false) {

    // Create a vector for solutionData
    std::vector<double> solutionData;

    // Populate a vector with some manufactured solution values
    getSolutionData(ents,rMax,solMax,solMin,solutionData);

    // Check we can set the solution
    ASSERT_TRUE(moabUOPtr->setSolution(var_name,
                                       solutionData,
                                       scalefactor,normToVol));

  }

  void setConstSolution(const std::vector<moab::EntityHandle>& ents, double solConst) {

    // Create a vector for constant solution
    std::vector<double> solutionData(ents.size(),solConst);    // Create a vector for solutionData

    // Check we can set the solution
    ASSERT_TRUE(moabUOPtr->setSolution(var_name,solutionData,1.0,false));

  }

  void checkSetSolution(const std::vector<moab::EntityHandle>& ents, double rMax, double solMax, double solMin, double solConst, double scalefactor=1.0, bool normToVol=false) {


    // Create a vector for solutionData
    std::vector<double> dummySolutionData;

    // Expect failure due to wrong var name
    EXPECT_THROW(moabUOPtr->setSolution("dummy",dummySolutionData,scalefactor,normToVol),
                 std::runtime_error);

    // Expect failure due to empty solution
    EXPECT_FALSE(moabUOPtr->setSolution(var_name,
                                        dummySolutionData,
                                        scalefactor,normToVol));


    // Fetch the system details;
    ASSERT_TRUE(checkSystem());
    System & sys = getSys();
    unsigned int iSys = sys.number();
    unsigned int iVar = sys.variable_number(var_name);

    // Get the size of solution vector
    numeric_index_type 	solsize = sys.solution->size();

    // Get the mesh
    MeshBase& mesh = problemPtr->mesh().getMesh();

    for(int i=0; i<2; i++){

      if(i==0){
        // Set a constant solution
        setConstSolution(ents,solConst);
      }
      else{
        // Set a non-trival solution
        setSolution(ents,rMax,solMax,solMin,scalefactor,normToVol);
      }

      // Check the solution we set is correct

      // Loop over the elements
      auto itelem = mesh.elements_begin();
      auto endelem = mesh.elements_end();
      for( ; itelem!=endelem; ++itelem){
        Elem& elem = **itelem;

        // Get the degree of freedom number for this element
        dof_id_type soln_index = elem.dof_number(iSys,iVar,0);

        ASSERT_LT(soln_index,solsize);

        // Get the solution value for this element
        double sol = double(sys.solution->el(soln_index));

        double solExpect=solConst;
        if(i>0){
          // Get the midpoint radius
          double radius = elemRadius(elem);

          // Get the expected solution for this element
          solExpect = getSolution(radius,rMax,solMax,solMin);
          solExpect *= scalefactor;

          if(normToVol){
            double volume = elem.volume();
            solExpect /= volume;
          }
        }

        // Compare
        double solDiff = fabs(sol-solExpect);
        EXPECT_LT(solDiff,tol);

      }

    }

  }

  void setSolutionTest(){

    // Set the mesh
    ASSERT_NO_THROW(moabUOPtr->initMOAB());

    // Get elems
    std::vector<moab::EntityHandle> ents;
    getElems(ents);

    // Pick a constant solution value for constant test
    double solConst = 300.;

    // Pick a max solution value for non-trivial test
    double solMax = 350.;
    // Pick a min solution value for non-trivial test
    double solMin = 300.;

    std::vector<double> scalefactors;
    scalefactors.push_back(1.0);
    scalefactors.push_back(5.0);

    // Absolute max radius
    double rAbsMax = getRMax();

    for(auto scale: scalefactors){
      checkSetSolution(ents,rAbsMax,solMax,solMin,solConst,scale,false);
      checkSetSolution(ents,rAbsMax,solMax,solMin,solConst,scale,true);
    }
  }

  std::string var_name;

  // Define a tolerance for double comparisons
  double tol;

  // Set the expected values for DagMC tags
  double faceting_tol_expect;
  double geom_tol_expect;

  // Lengthscale to convert between Moose and MOAB units:
  // must match up with parameter in user object
  double lengthscale;
};

// Test correct failure for ill-defined mesh
class BadMoabUserObjectTest : public MoabUserObjectTestBase {
protected:
  BadMoabUserObjectTest() : MoabUserObjectTestBase("badmoabuserobject.i") {};
};


class MoabChangeUnitsTest : public MoabUserObjectTest {
protected:
  MoabChangeUnitsTest() :
    MoabUserObjectTest("moabuserobject-changeunits.i") {
    // override lengthscale
    lengthscale=1000.0;
    // override dagmc paramters
    faceting_tol_expect = 1.e-2;
    geom_tol_expect = 1.e-4;
  };
};

class FindMoabSurfacesTest : public MoabUserObjectTest {
protected:
  FindMoabSurfacesTest() :
    MoabUserObjectTest("findsurfstest.i") {

    // Define the material names expected for this input file
    mat_names.push_back("mat:copper");
    mat_names.push_back("mat:air");
    mat_names.push_back("mat:Graveyard");
  };

  // Define a struct to help test properties of entity sets
  struct TagInfo {
    std::string category; // value of category tag
    int dim; // value of dimension tag
    std::string name; // value of name tag
    int id; // value of global id
  };


  void getTags(std::map< std::string, std::vector<TagInfo> >& tags_by_cat,
               std::string category,
               int dim,
               std::vector<std::string> names){

    // Number in this category
    unsigned int numCat = names.size();
    std::vector<TagInfo> catTags;
    for(unsigned int iCat = 0; iCat<numCat; iCat++){
      int id = iCat+1;
      std::string name = names.at(iCat);
      TagInfo tags = { category, dim , name, id };
      catTags.push_back(tags);
    }
    tags_by_cat[category] = catTags;

  }
  void getTags(std::map< std::string, std::vector<TagInfo> >& tags_by_cat,
               std::vector<std::string> names,
               unsigned int nVol,unsigned  int nSurf){

    // Ensure map empty
    tags_by_cat.clear();

    // Groups
    std::string category = "Group";
    int dim = 4;
    getTags(tags_by_cat,category, dim, names);

    // Volumes
    category = "Volume";
    dim = 3;
    // Clear material names
    names.clear();
    names.resize(nVol,"");
    getTags(tags_by_cat,category, dim, names);

    // Surfaces
    category = "Surface";
    dim = 2;
    names.resize(nSurf,"");
    getTags(tags_by_cat,category, dim, names);

  }

  void checkAllGeomsets(unsigned int nVol,unsigned int nSurf){
    // Get the MOAB interface to check the data
    std::shared_ptr<moab::Interface> moabPtr = moabUOPtr->moabPtr;
    moab::ErrorCode rval;

    // Create a local geomtopotool
    moab::GeomTopoTool GTT(moabPtr.get(), false);

    // Get root set
    moab::EntityHandle rootset = moabPtr->get_root_set();

    // Get tags to check sorted  by category
    std::map< std::string, std::vector<TagInfo> > tags_by_cat;
    getTags(tags_by_cat,mat_names,nVol,nSurf);

    // Get tag handles
    moab::Tag category_tag;
    rval = moabPtr->tag_get_handle(CATEGORY_TAG_NAME,category_tag);
    ASSERT_EQ(rval,moab::MB_SUCCESS);
    moab::Tag name_tag;
    rval = moabPtr->tag_get_handle(NAME_TAG_NAME,name_tag);
    ASSERT_EQ(rval,moab::MB_SUCCESS);
    moab::Tag geom_tag;
    rval = moabPtr->tag_get_handle(GEOM_DIMENSION_TAG_NAME,geom_tag);
    ASSERT_EQ(rval,moab::MB_SUCCESS);
    moab::Tag id_tag;
    rval = moabPtr->tag_get_handle(GLOBAL_ID_TAG_NAME,id_tag);
    ASSERT_EQ(rval,moab::MB_SUCCESS);

    // Define some containers to perform set comparisons
    moab::Range vols;
    moab::Range surfs;
    std::set<moab::EntityHandle> volsFromGroups;
    std::set<moab::EntityHandle> surfsFromVols;
    std::set<moab::EntityHandle> trisFromSurfs;

    // Check each category in turn
    for( const auto& category : tags_by_cat){

      std::string cat = category.first;

      // Downcast category name
      char namebuf[CATEGORY_TAG_SIZE];
      memset(namebuf,'\0', CATEGORY_TAG_SIZE); // fill C char array with null
      strncpy(namebuf,cat.c_str(), CATEGORY_TAG_SIZE); // Copy category data into namebuf
      const void * data = namebuf;

      // Get entity sets in this category
      moab::Range ents;
      rval = moabPtr->get_entities_by_type_and_tag(rootset,moab::MBENTITYSET,&category_tag, &data, 1, ents);

      // Save volumes/surfaces for later comparisons
      if(cat =="Volume") vols = ents;
      else if(cat =="Surface") surfs = ents;

      unsigned int nCat = category.second.size();
      EXPECT_EQ(ents.size(),nCat);
      if( ents.size()!= nCat ) continue;

      // Get the values of the other tags
      char names[nCat][NAME_TAG_SIZE];
      rval = moabPtr->tag_get_data (name_tag,ents,&names);
      if(cat =="Group")
        EXPECT_EQ(rval,moab::MB_SUCCESS);
      else
        EXPECT_NE(rval,moab::MB_SUCCESS);

      int dims[nCat];
      rval = moabPtr->tag_get_data (geom_tag,ents,&dims);
      EXPECT_EQ(rval,moab::MB_SUCCESS);

      int ids[nCat];
      rval = moabPtr->tag_get_data (id_tag,ents,&ids);
      EXPECT_EQ(rval,moab::MB_SUCCESS);

      // Check tag values
      for(unsigned int iCat=0; iCat<nCat; iCat++){
        TagInfo tags = category.second.at(iCat);

        if(cat =="Group"){
          // Check the name tag
          std::string namecheck(names[iCat]);
          EXPECT_EQ(namecheck,tags.name);

          // Save the volumes in the group
          moab::EntityHandle group = ents[iCat];
          std::vector< moab::EntityHandle > group_vols;
          rval = moabPtr->get_entities_by_handle(group,group_vols);
          EXPECT_EQ(rval,moab::MB_SUCCESS);

          EXPECT_FALSE(group_vols.empty());
          for(auto vol: group_vols){
            // Vols should not be in more than one group
            bool found_vol = ( volsFromGroups.find(vol) != volsFromGroups.end() );
            EXPECT_FALSE(found_vol);
            volsFromGroups.insert(vol);
          }
        }
        else if(cat =="Volume"){
          moab::EntityHandle vol = ents[iCat];
          std::vector< moab::EntityHandle > vol_surfs;
          getChildren(vol,vol_surfs);

          EXPECT_FALSE(vol_surfs.empty());
          surfsFromVols.insert(vol_surfs.begin(),vol_surfs.end());
        }
        else if(cat =="Surface"){
          // Save the volumes in the group
          moab::EntityHandle surf = ents[iCat];
          std::vector< moab::EntityHandle > tris;
          rval = moabPtr->get_entities_by_handle(surf,tris);
          EXPECT_EQ(rval,moab::MB_SUCCESS);

          EXPECT_FALSE(tris.empty());
          for(auto tri: tris){
            // Each tri should only be in one surface
            bool found_tri = ( trisFromSurfs.find(tri) != trisFromSurfs.end() );
            EXPECT_FALSE(found_tri);
            trisFromSurfs.insert(tri);
          }
        }

        EXPECT_EQ(dims[iCat],tags.dim);
        EXPECT_EQ(ids[iCat],tags.id);
      }
    } // End loop over categories

    // Check every volume is assigned to a group
    EXPECT_EQ(vols.size(),volsFromGroups.size());
    for(size_t ivol=0; ivol<vols.size(); ivol++){
      bool found_vol = ( volsFromGroups.find(vols[ivol]) != volsFromGroups.end() );
      EXPECT_TRUE(found_vol);
    }

    // Check surfaces are consistent
    EXPECT_EQ(surfs.size(),surfsFromVols.size());
    for(size_t isurf=0; isurf<surfs.size(); isurf++){
      bool found_surf = ( surfsFromVols.find(surfs[isurf]) != surfsFromVols.end() );
      EXPECT_TRUE(found_surf);

      // Get the parent volumess of this surface
      std::vector< moab::EntityHandle > parents;
      rval = moabPtr->get_parent_meshsets(surfs[isurf],parents);
      EXPECT_EQ(rval,moab::MB_SUCCESS);

      // Expect either one or two volumes
      size_t nVols = parents.size();
      EXPECT_LT(nVols,3);
      EXPECT_GT(nVols,0);

      std::vector<int> senses;
      for(auto parent: parents){
        // Check every parent is a known volume
        bool found_parent = ( volsFromGroups.find(parent) != volsFromGroups.end() );
        EXPECT_TRUE(found_parent);

        // Check senses
        int sense;
        rval = GTT.get_sense(surfs[isurf],parent,sense);
        EXPECT_EQ(rval,moab::MB_SUCCESS);
        EXPECT_EQ(abs(sense),1);
        senses.push_back(sense);
      }
      // Check parents are different and have opposite sense
      if(nVols == 2){
        EXPECT_NE(parents.at(0),parents.at(1));
        EXPECT_NE(senses.at(0),senses.at(1));
      }
    }

  }

  void getEntFromID(int id,std::string cat,moab::EntityHandle& ent){

    // Get the MOAB interface to check the data
    std::shared_ptr<moab::Interface> moabPtr = moabUOPtr->moabPtr;
    moab::ErrorCode rval;

    // Get root set
    moab::EntityHandle rootset = moabPtr->get_root_set();

    moab::Tag category_tag;
    rval = moabPtr->tag_get_handle(CATEGORY_TAG_NAME,category_tag);
    ASSERT_EQ(rval,moab::MB_SUCCESS);

    moab::Tag id_tag;
    rval = moabPtr->tag_get_handle(GLOBAL_ID_TAG_NAME,id_tag);
    ASSERT_EQ(rval,moab::MB_SUCCESS);

    moab::Tag tags[2] = {category_tag,id_tag};

    // Downcast category name
    char namebuf[CATEGORY_TAG_SIZE];
    memset(namebuf,'\0', CATEGORY_TAG_SIZE); // fill C char array with null
    strncpy(namebuf,cat.c_str(), CATEGORY_TAG_SIZE); // Copy category data into namebuf
    const void * name_data = namebuf;

    // Downcast id
    const void * id_data = &id;

    // Store list of tag data
    const void * data[2] = {name_data,id_data};

    // Get entity sets with these tag values
    moab::Range ents;
    rval = moabPtr->get_entities_by_type_and_tag(rootset,moab::MBENTITYSET,tags, data, 2, ents);
    ASSERT_EQ(rval,moab::MB_SUCCESS);

    // Only expect one entity
    ASSERT_EQ(ents.size(),1);

    // Return the entity
    ent = ents[0];
  }

  void getVol(int id,moab::EntityHandle& ent){
    getEntFromID(id,"Volume",ent);
  }


  void getEntityID(moab::EntityHandle ent, int& id){

    // Get the MOAB interface to check the data
    std::shared_ptr<moab::Interface> moabPtr = moabUOPtr->moabPtr;
    moab::ErrorCode rval;

    moab::Tag id_tag;
    rval = moabPtr->tag_get_handle(GLOBAL_ID_TAG_NAME,id_tag);
    ASSERT_EQ(rval,moab::MB_SUCCESS);

    rval = moabPtr->tag_get_data(id_tag,&ent,1,&id);
    ASSERT_EQ(rval,moab::MB_SUCCESS);
  }

  void getChildren(moab::EntityHandle parent,std::vector< moab::EntityHandle > &children){
    moab::ErrorCode rval = moabUOPtr->moabPtr->get_child_meshsets(parent,children);
    ASSERT_EQ(rval,moab::MB_SUCCESS);
  }

  void checkSurfs(int vol_id, std::set<int> surf_ids){

    // First fetch the handle for this vol id
    moab::EntityHandle vol_handle;
    getVol(vol_id,vol_handle);

    // Now get the child surfaces
    std::vector< moab::EntityHandle > surf_handles;
    getChildren(vol_handle, surf_handles);

    EXPECT_EQ(surf_handles.size(),surf_ids.size());

    for( const auto& surf : surf_handles){
      int id;
      getEntityID(surf,id);
      auto it_surf = surf_ids.find(id);
      bool found_surf = ( it_surf!= surf_ids.end());
      EXPECT_TRUE(found_surf);
      // Expect one-to-one mapping, so delete
      surf_ids.erase(it_surf);
    }

    // Found all the surfs
    EXPECT_EQ(surf_ids.size(),0);

  }


  std::vector<std::string> mat_names;
};

// Test the fixture set up
TEST_F(MoabUserObjectTest, setup)
{
  EXPECT_FALSE(appIsNull);
  EXPECT_TRUE(foundMOAB);
  EXPECT_FALSE(moabUOPtr->hasProblem());
}

// Init should fail if we don't set the FE problem
TEST_F(MoabUserObjectTest, nullproblem)
{
  ASSERT_TRUE(foundMOAB);

  // Init should throw because we didn't set the feproblem
  EXPECT_THROW(moabUOPtr->initMOAB(),std::runtime_error);

}

// Test for MOAB mesh initialisation
TEST_F(MoabUserObjectTest, init)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get the MOAB interface to check the data
  std::shared_ptr<moab::Interface> moabPtr = moabUOPtr->moabPtr;
  ASSERT_NE(moabPtr,nullptr);

  // Check dimension
  int dim;
  moab::ErrorCode rval = moabPtr->get_dimension(dim);
  EXPECT_EQ(rval,moab::MB_SUCCESS);
  EXPECT_EQ(dim,3);

  // Get root set
  moab::EntityHandle rootset = moabPtr->get_root_set();

  // Get the mesh set
  moab::Range ents;
  rval = moabPtr->get_entities_by_type(rootset,moab::MBENTITYSET,ents);
  EXPECT_EQ(rval,moab::MB_SUCCESS);
  ASSERT_EQ(ents.size(),1);
  moab::EntityHandle meshset = ents.back();

  // Check nodes
  ents.clear();
  rval = moabPtr->get_entities_by_dimension(rootset,0,ents);
  EXPECT_EQ(rval,moab::MB_SUCCESS);
  EXPECT_EQ(ents.size(),2409);

  // Check elems
  ents.clear();
  rval = moabPtr->get_entities_by_dimension(rootset,3,ents);
  EXPECT_EQ(rval,moab::MB_SUCCESS);
  EXPECT_EQ(ents.size(),11972);
  size_t nTets = ents.num_of_type(moab::MBTET);
  EXPECT_EQ(nTets,11972);

  // Check built-in tags
  moab::Tag tag_handle;
  rval = moabPtr->tag_get_handle(GEOM_DIMENSION_TAG_NAME,tag_handle);
  EXPECT_EQ(rval,moab::MB_SUCCESS);

  rval = moabPtr->tag_get_handle(GLOBAL_ID_TAG_NAME,tag_handle);
  EXPECT_EQ(rval,moab::MB_SUCCESS);

  rval = moabPtr->tag_get_handle(CATEGORY_TAG_NAME,tag_handle);
  EXPECT_EQ(rval,moab::MB_SUCCESS);

  rval = moabPtr->tag_get_handle(NAME_TAG_NAME,tag_handle);
  EXPECT_EQ(rval,moab::MB_SUCCESS);

  // Check DagMC tags
  rval = moabPtr->tag_get_handle("FACETING_TOL",tag_handle);
  EXPECT_EQ(rval,moab::MB_SUCCESS);

  double faceting_tol;
  rval = moabPtr->tag_get_data(tag_handle, &meshset, 1, &faceting_tol);
  EXPECT_EQ(rval,moab::MB_SUCCESS);
  double diff = fabs(faceting_tol - faceting_tol_expect);
  EXPECT_LT(diff,tol);

  rval = moabPtr->tag_get_handle("GEOMETRY_RESABS",tag_handle);
  EXPECT_EQ(rval,moab::MB_SUCCESS);

  double geom_tol;
  rval = moabPtr->tag_get_data(tag_handle, &meshset, 1, &geom_tol);
  EXPECT_EQ(rval,moab::MB_SUCCESS);
  diff = fabs(geom_tol - geom_tol_expect);
  EXPECT_LT(diff,tol);

}


// Test for setting FE problem solution
TEST_F(MoabUserObjectTest, setSolution)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Perform the test
  setSolutionTest();
}

// Repeat for different lengthscale
TEST_F(MoabChangeUnitsTest, setSolution)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Perform the test
  setSolutionTest();
}

// Test for MOAB mesh reseting
TEST_F(MoabUserObjectTest, reset)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Clear the mesh
  ASSERT_NO_THROW(moabUOPtr->reset());

  // Get the MOAB interface
  std::shared_ptr<moab::Interface> moabPtr = moabUOPtr->moabPtr;

  // Check the (lack of) data

  // Get root set
  moab::EntityHandle rootset = moabPtr->get_root_set();

  // Check no entity handles of each dimensionality in turn
  // dim=4 -> entitiy set
  std::vector<moab::EntityHandle> ents;
  moab::ErrorCode rval;
  for(int dim=0; dim<5; dim++){
    ents.clear();
    rval = moabPtr->get_entities_by_dimension(rootset,dim,ents);
    EXPECT_EQ(rval,moab::MB_SUCCESS);
    EXPECT_EQ(ents.size(),0);
  }

}


// Test for finding surfaces
TEST_F(FindMoabSurfacesTest, constTemp)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Set a constant solution
  double solConst = 300.;
  setConstSolution(ents,solConst);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check groups, volumes and surfaces
  unsigned int nVol=3;
  unsigned int nSurf=4;
  checkAllGeomsets(nVol,nSurf);

}

TEST_F(FindMoabSurfacesTest, singleBin)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Manufacture a solution that will map to a single temp bin
  // Bin edges are 297.5, 302.5
  double solMax = 302.;
  double solMin = 298.;
  // Absolute max radius
  double rAbsMax = getRMax();
  setSolution(ents,rAbsMax,solMax,solMin,1.0,false);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check groups, volumes and surfaces
  unsigned int nVol=3;
  unsigned int nSurf=4;
  checkAllGeomsets(nVol,nSurf);

}

TEST_F(FindMoabSurfacesTest, extraBin)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Manufacture a solution that results in one extra surface
  // Bin edges are 297.5, 302.5
  double solMax = 307.;
  double solMin = 298.;
  double rMax = 2.0*lengthscale/log(2.0);
  setSolution(ents,rMax,solMax,solMin,1.0,false);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check groups, volumes and surfaces
  unsigned int nVol=4;
  unsigned int nSurf=5;
  checkAllGeomsets(nVol,nSurf);


  // Check volume->surf relationships

  // Copper block
  int vol_id=1;
  std::set<int> surf_ids = {1};
  checkSurfs(vol_id,surf_ids);

  // Region of air at cooler T
  vol_id=2;
  surf_ids = {1,2,3};
  checkSurfs(vol_id,surf_ids);

  // Region of air at hotter T
  vol_id=3;
  surf_ids = {3};
  checkSurfs(vol_id,surf_ids);

  // Graveyard
  vol_id=4;
  surf_ids = {4,5};
  checkSurfs(vol_id,surf_ids);

  // Change boundary of temperature contour to intersect material boundary
  rMax = 4.0*lengthscale/log(2.0);
  setSolution(ents,rMax,solMax,solMin,1.0,false);

  // Find the surfaces (and also test reset)
  EXPECT_TRUE(moabUOPtr->update());

  // Check groups, volumes and surfaces
  nVol=5;
  nSurf=9;
  checkAllGeomsets(nVol,nSurf);

  // Check volume->surf relationships

  // Main block of copper at lower T
  vol_id=1;
  surf_ids = {1,3,5};
  checkSurfs(vol_id,surf_ids);

  // Block of copper at higher T
  vol_id=2;
  surf_ids={2,3,6};
  checkSurfs(vol_id,surf_ids);

  // Region of air at lower T
  vol_id=3;
  surf_ids ={4,5,6,7};
  checkSurfs(vol_id,surf_ids);

  // Region of air at higher T
  vol_id=4;
  surf_ids ={1,2,7};
  checkSurfs(vol_id,surf_ids);

  // Graveyard
  vol_id=5;
  surf_ids ={8,9};
  checkSurfs(vol_id,surf_ids);

}


// Check that a non-tet mesh fails
TEST_F(BadMoabUserObjectTest, failinit)
{

  // Check setup
  ASSERT_TRUE(foundMOAB);
  ASSERT_FALSE(moabUOPtr->hasProblem());

  // Set the problem
  ASSERT_TRUE(setProblem());

  // Init should throw because we didn't pass in a tet mesh
  EXPECT_THROW(moabUOPtr->initMOAB(),std::runtime_error);

}
