//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "OpenMCAppTest.h"
#include "Executioner.h"
#include "FEProblemBase.h"
#include "VolumePostprocessor.h"
#include "MoabUserObject.h"

// Fixture to test the MOAB user object
class MoabUserObjectTestBase : public OpenMCAppInputTest{
protected:
  MoabUserObjectTestBase(std::string inputfile) :
    OpenMCAppInputTest(inputfile),
    foundMOAB(false)
  {};

  virtual void SetUp() override {

    // Call the base class method
    OpenMCAppInputTest::SetUp();

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
    catch(std::exception& e){
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
    MoabUserObjectTestBase("moabuserobject.i")
  {
    setDefaults();
  };

  MoabUserObjectTest(std::string inputfile) :
    MoabUserObjectTestBase(inputfile)
  {
    setDefaults();
  };

  virtual void setDefaults(){
    var_name="temperature";
    faceting_tol_expect=1.e-4;
    geom_tol_expect=1.e-6;
    lengthscale=100.0;
    dimExpect=3;
    nNodesExpect=2409;
    nElemsExpect=11972;
  }

  void initMoabTest()
  {
    // Set the mesh
    ASSERT_NO_THROW(moabUOPtr->initMOAB());

    // Get the MOAB interface to check the data
    std::shared_ptr<moab::Interface> moabPtr = moabUOPtr->moabPtr;
    ASSERT_NE(moabPtr,nullptr);

    // Check dimension
    int dim;
    moab::ErrorCode rval = moabPtr->get_dimension(dim);
    EXPECT_EQ(rval,moab::MB_SUCCESS);
    EXPECT_EQ(dim,dimExpect);

    // Get root set
    moab::EntityHandle rootset = moabPtr->get_root_set();

    // Get the mesh set
    moab::Range ents;
    rval = moabPtr->get_entities_by_type(rootset,moab::MBENTITYSET,ents);
    EXPECT_EQ(rval,moab::MB_SUCCESS);
    ASSERT_EQ(ents.size(),size_t(1));
    moab::EntityHandle meshset = ents.back();

    // Check nodes
    ents.clear();
    rval = moabPtr->get_entities_by_dimension(rootset,0,ents);
    EXPECT_EQ(rval,moab::MB_SUCCESS);
    EXPECT_EQ(ents.size(),nNodesExpect);

    // Check elems
    ents.clear();
    rval = moabPtr->get_entities_by_dimension(rootset,3,ents);
    EXPECT_EQ(rval,moab::MB_SUCCESS);
    EXPECT_EQ(ents.size(),nElemsExpect);
    size_t nTets = ents.num_of_type(moab::MBTET);
    EXPECT_EQ(nTets,nElemsExpect);

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

  bool checkSystem(){
    try{
      getSys();
    }
    catch(std::runtime_error& e){
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

  void setSolution(const std::vector<moab::EntityHandle>& ents, double rMax,
                   double solMax, double solMin,
                   double scalefactor=1.0, bool normToVol=false) {

    // Create a vector for solutionData
    std::vector<double> solutionData;

    // Populate a vector with some manufactured solution values
    getSolutionData(ents,rMax,solMax,solMin,solutionData);

    // Check we can set the solution
    ASSERT_TRUE(moabUOPtr->setSolution(var_name,
                                       solutionData,
                                       scalefactor,normToVol));

  }

  void setSolution(const std::vector<moab::EntityHandle>& ents, double rMax,
                   double solMax, double solMin,
                   double scalefactor, bool normToVol,
                   std::vector<double>& solExpect) {

    setSolution(ents,rMax,solMax,solMin,scalefactor,normToVol);

    // Save the solution we expect

    // Get the mesh
    MeshBase& mesh = problemPtr->mesh().getMesh();

    // Loop over the elements
    auto itelem = mesh.elements_begin();
    auto endelem = mesh.elements_end();
    for( ; itelem!=endelem; ++itelem){
      Elem& elem = **itelem;

      // Get the midpoint radius
      double radius = elemRadius(elem);

      // Get the expected solution for this element
      double solExpectNow = getSolution(radius,rMax,solMax,solMin);
      solExpectNow *= scalefactor;

      if(normToVol){
        double volume = elem.volume();
        solExpectNow /= volume;
      }

      solExpect.push_back(solExpectNow);
    }

  }

  void setConstSolution(const std::vector<moab::EntityHandle>& ents, double solConst) {

    // Create a vector for constant solution
    std::vector<double> solutionData(ents.size(),solConst);    // Create a vector for solutionData

    // Check we can set the solution
    ASSERT_TRUE(moabUOPtr->setSolution(var_name,solutionData,1.0,false));


  }

  void setConstSolution(const std::vector<moab::EntityHandle>& ents, double solConst,std::vector<double>& solExpect){

    setConstSolution(ents,solConst);
    solExpect.resize(ents.size(),solConst);

  }


  void checkSolution(const std::vector<double>& solExpect){

    // Fetch the system details;
    ASSERT_TRUE(checkSystem());
    System & sys = getSys();
    unsigned int iSys = sys.number();
    unsigned int iVar = sys.variable_number(var_name);

    // Get the size of solution vector
    numeric_index_type 	solsize = sys.solution->size();

    // Get the mesh
    MeshBase& mesh = problemPtr->mesh().getMesh();

    // Loop over the elements
    auto itelem = mesh.elements_begin();
    auto endelem = mesh.elements_end();
    size_t iElem=0;
    for( ; itelem!=endelem; ++itelem){
      Elem& elem = **itelem;

      // Get the degree of freedom number for this element
      dof_id_type soln_index = elem.dof_number(iSys,iVar,0);

      ASSERT_LT(soln_index,solsize);

      // Get the solution value for this element
      double sol = double(sys.solution->el(soln_index));

      // Get the solution we expect
      double solExpectNow=solExpect.at(iElem);

      // Compare
      double solDiff = fabs(sol-solExpectNow);
      EXPECT_LT(solDiff,tol);

      iElem++;
    }

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

    for(int i=0; i<2; i++){

      std::vector<double> solExpect;

      if(i==0){
        // Set a constant solution
        setConstSolution(ents,solConst,solExpect);
      }
      else{
        // Set a non-trival solution
        setSolution(ents,rMax,solMax,solMin,scalefactor,normToVol,solExpect);
      }

      // Check the solution we set is correct
      checkSolution(solExpect);

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

  // Exepcted mesh params
  int dimExpect;
  size_t nNodesExpect;
  size_t nElemsExpect;

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

// Test for second-order mesh
class SecondOrderMoabUserObjectTest : public MoabUserObjectTest {
protected:
  SecondOrderMoabUserObjectTest() :
    MoabUserObjectTest("secondordermesh.i") {

    // Override defaults
    nNodesExpect=65;
    nElemsExpect=192;

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

    setOutputDefaults();
  };

  FindMoabSurfacesTest(std::string inputfile) :
    MoabUserObjectTest(inputfile) {
    setOutputDefaults();
  };

  void setOutputDefaults(){

    // Max number of outputs
    nOutput=10;

    // How often should we skip write?
    nSkip=0; // never skip

    // Expected base name of file
    output_base="moab_surfs";
  }

  // Define a struct to help test properties of entity sets
  struct TagInfo {
    std::string category; // value of category tag
    int dim; // value of dimension tag
    std::string name; // value of name tag
    int id; // value of global id
  };

  // Define a struct to test volume/surface details
  struct VolumeInfo {
    int vol_id;
    std::set<int> surf_ids;
    double temp;
    bool isGraveyard;
  };

  virtual void TearDown() override {
    // Delete any outputs which were created
    std::string filename;

    for(unsigned int iOutput=0; iOutput<nOutput; iOutput++){
      filename = output_base + "_" + std::to_string(iOutput) +".h5m";
      bool check_file_exists = fileExists(filename);
      if(check_file_exists){
        std::cout<<"Deleting file "<< filename<<std::endl;
        deleteFile(filename);
      }
    }
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
    moab::EntityHandle graveyard(0);

    // Save the bounding coords
    double MAX=std::numeric_limits<double>::max();
    double max[3]={-MAX,-MAX,-MAX};
    double min[3]={MAX,MAX,MAX};
    double maxGY[3]={-MAX,-MAX,-MAX};
    double minGY[3]={MAX,MAX,MAX};

    // Check each category in turn
    for( const auto& category : tags_by_cat){

      std::string cat = category.first;

      // Downcast category name
      char namebuf[CATEGORY_TAG_SIZE];
      memset(namebuf,'\0', CATEGORY_TAG_SIZE); // fill C char array with null
      strncpy(namebuf,cat.c_str(),cat.size()); // Copy category data into namebuf
      const void * data = namebuf;

      // Get entity sets in this category
      moab::Range ents;
      rval = moabPtr->get_entities_by_type_and_tag(rootset,moab::MBENTITYSET,&category_tag, &data, 1, ents);

      // Save volumes/surfaces for later comparisons
      if(cat =="Volume") vols = ents;
      else if(cat =="Surface") surfs = ents;

      size_t nCat = category.second.size();
      EXPECT_EQ(ents.size(),nCat) << "Disparity in category size for "<< cat;
      if( ents.size()!= nCat ) continue;

      // Get the values of the other tags
      auto names = new char[nCat][NAME_TAG_SIZE];
      rval = moabPtr->tag_get_data (name_tag,ents,names);
      if(cat =="Group")
        EXPECT_EQ(rval,moab::MB_SUCCESS);
      else
        EXPECT_NE(rval,moab::MB_SUCCESS);

      std::vector<int> dims(nCat);
      rval = moabPtr->tag_get_data (geom_tag,ents,dims.data());
      EXPECT_EQ(rval,moab::MB_SUCCESS);

      std::vector<int> ids(nCat);
      rval = moabPtr->tag_get_data (id_tag,ents,ids.data());
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

          // Only expect 1 graveyard
          if(namecheck=="mat:Graveyard"){
            ASSERT_EQ(group_vols.size(),size_t(1));
            graveyard=group_vols.front();
          }

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
          // Save the tris in the surface
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

            // Get the coords of this tri
            // 9 = 3 nodes * 3 dims
            std::vector<double> nodeCoords(9);
            moabPtr->get_coords	(&tri,1,nodeCoords.data());
            // Loop over nodes of tri
            for(size_t inode=0; inode<3; ++inode){
              // Loop over x,y,z
              for(int i=0; i<3; i++){
                double coord = nodeCoords.at(3*inode +i);
                if(coord>max[i]){
                  max[i]=coord;
                }
                if(coord<min[i]){
                  min[i]=coord;
                }
              }
            }

          }
        }

        EXPECT_EQ(dims[iCat],tags.dim);
        EXPECT_EQ(ids[iCat],tags.id);
      }
      // Deallocate memory
      delete[] names;

    } // End loop over categories

    // Check every volume is assigned to a group
    EXPECT_EQ(vols.size(),volsFromGroups.size());
    for(size_t ivol=0; ivol<vols.size(); ivol++){
      bool found_vol = ( volsFromGroups.find(vols[ivol]) != volsFromGroups.end() );
      EXPECT_TRUE(found_vol);
    }

    // Check we found a graveyard
    EXPECT_NE(graveyard,0);

    // Check surfaces are consistent
    EXPECT_EQ(surfs.size(),surfsFromVols.size());
    for(size_t isurf=0; isurf<surfs.size(); isurf++){
      bool found_surf = ( surfsFromVols.find(surfs[isurf]) != surfsFromVols.end() );
      EXPECT_TRUE(found_surf);

      // Get the parent volumes of this surface
      std::vector< moab::EntityHandle > parents;
      rval = moabPtr->get_parent_meshsets(surfs[isurf],parents);
      EXPECT_EQ(rval,moab::MB_SUCCESS);

      // Expect either one or two volumes
      size_t nVols = parents.size();
      EXPECT_LT(nVols,size_t(3));
      EXPECT_GT(nVols,size_t(0));

      std::vector<int> senses;
      bool isGraveyard=false;
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

        // Check if this volume is the graveyard
        if(parent == graveyard){
          isGraveyard=true;
        }

      }
      // Check parents are different and have opposite sense
      if(nVols == 2){
        EXPECT_NE(parents.at(0),parents.at(1));
        EXPECT_NE(senses.at(0),senses.at(1));
      }

      if(isGraveyard){
        // Set Graveyard BB

        // Get the tris
        std::vector< moab::EntityHandle > tris;
        rval = moabPtr->get_entities_by_handle(surfs[isurf],tris);
        EXPECT_EQ(rval,moab::MB_SUCCESS);
        EXPECT_FALSE(tris.empty());
        for(auto tri: tris){
          // Get the coords of this tri
          // 9 = 3 nodes * 3 dims
          std::vector<double> nodeCoords(9);
          moabPtr->get_coords	(&tri,1,nodeCoords.data());
          // Loop over nodes of tri
          for(size_t inode=0; inode<3; ++inode){
            // Loop over x,y,z
            for(int i=0; i<3; i++){
              double coord = nodeCoords.at(3*inode +i);
              if(coord>maxGY[i]){
                maxGY[i]=coord;
              }
              if(coord<minGY[i]){
                minGY[i]=coord;
              }
            }
          }
        }// End loop over tris

      }
    } // End loop over surfs

    // Check the graveyard fully encompasses everything
    // Loop over x,y,z
    std::string err="Graveyard is not bounding box";
    for(int i=0; i<3; i++){
      EXPECT_LT(minGY[i]-tol,min[i])<<err;
      EXPECT_GT(maxGY[i]+tol,max[i])<<err;
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
    strncpy(namebuf,cat.c_str(),cat.size()); // Copy category data into namebuf
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
    ASSERT_EQ(ents.size(),size_t(1));

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

  void checkSurfsAndTemp(VolumeInfo volinfo){

    // Shorthands
    int& vol_id=volinfo.vol_id;
    std::set<int>& surf_ids=volinfo.surf_ids;

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
      std::string err  = "Unexpected surface "+std::to_string(id)
        +" for volume "+std::to_string(vol_id);
      EXPECT_TRUE(found_surf)<<err;
      // Expect one-to-one mapping, so delete
      if(found_surf){
        surf_ids.erase(it_surf);
      }
    }

    // Found all the surfs
    EXPECT_EQ(surf_ids.size(),size_t(0));

    // Check temperature
    if(volinfo.isGraveyard){
      EXPECT_THROW(moabUOPtr->getTemperature(vol_handle),std::out_of_range);
    }
    else{
      double temp = moabUOPtr->getTemperature(vol_handle);
      double dtemp = fabs(temp-volinfo.temp);
      EXPECT_LT(dtemp,tol);
    }
  }

  void checkOutputAfterUpdate(unsigned int nUpdate){

    // Keep track of how many times we write to file
    unsigned int iWrite=0;

    // Define size of write period
    unsigned int writePeriod=nSkip+1;

    std::string filename;
    for(unsigned int iUpdate=0; iUpdate<nUpdate; iUpdate++){

      // Set filename
      filename = output_base + "_" + std::to_string(iWrite) +".h5m";

      // File shouldn't exist yet.
      std::string err = "File " + filename + " already exists";
      EXPECT_FALSE(fileExists(filename)) << err;

      // Update and find the surfaces
      EXPECT_TRUE(moabUOPtr->update());

      if(iWrite< nOutput && ( iUpdate % writePeriod) == 0 ){
        // File should exist now
        err = "File " + filename + " does not exist";
        EXPECT_TRUE(fileExists(filename)) << err;
        iWrite++;
      }
      else{
        // We exceeded max write or we are skipping this iteration
        // so file should not exist
        err = "File " + filename + " already exists";
        EXPECT_FALSE(fileExists(filename)) << err;
      }
    }
  }

  void checkConstTempSurfs(double solConst,unsigned int nVol,unsigned int nSurf,int nDegen=1){
    EXPECT_FALSE(appIsNull);
    ASSERT_TRUE(foundMOAB);
    ASSERT_TRUE(setProblem());
    ASSERT_NO_THROW(moabUOPtr->initBinningData());

    // Set the mesh
    ASSERT_NO_THROW(moabUOPtr->initMOAB());

    // Get elems
    std::vector<moab::EntityHandle> ents;
    getElems(ents);

    // Set a constant solution
    setConstSolution(ents,solConst);

    // Find the surfaces
    ASSERT_TRUE(moabUOPtr->update());

    // Check groups, volumes and surfaces
    checkAllGeomsets(nVol,nSurf);

    // Check volume->surf relationships
    VolumeInfo volinfo;
    volinfo.temp=solConst*double(nDegen);
    volinfo.isGraveyard=false;
    for(unsigned int iVol=1; iVol<nVol; iVol++){
      volinfo.vol_id=iVol;
      volinfo.surf_ids.clear();
      volinfo.surf_ids.insert(iVol);
      if(iVol>1){
        volinfo.surf_ids.insert(iVol-1);
      }
      checkSurfsAndTemp(volinfo);
    }

    // Graveyard
    volinfo.vol_id=nVol;
    volinfo.surf_ids = {int(nSurf),int(nSurf)-1};
    volinfo.isGraveyard=true;
    checkSurfsAndTemp(volinfo);
  }

  // Vector to hold material names
  std::vector<std::string> mat_names;

  // Variables relating to output params
  unsigned int nOutput;
  unsigned int nSkip;
  std::string output_base;

};

// Repeat surfaces test for second order mesh
class SecondOrderSurfacesTest : public FindMoabSurfacesTest {
protected:

  SecondOrderSurfacesTest() :
    FindMoabSurfacesTest("findsurfstest-second.i") {

    // Define the material names expected for this input file
    mat_names.push_back("mat:copper");
    mat_names.push_back("mat:air");
    mat_names.push_back("mat:Graveyard");

  }
};

class FindSingleMatSurfs: public FindMoabSurfacesTest {
protected:

  FindSingleMatSurfs() :
    FindMoabSurfacesTest("findsurfstest-singlemat.i") {

    // Define the material names expected for this input file
    mat_names.push_back("mat:copper");
    mat_names.push_back("mat:Graveyard");

    // Max number of outputs
    nOutput=4;

    // How often should we skip write?
    nSkip=2;  // write every third

    // Expected base name of file
    output_base="random_name";

  };

};

class FindOffsetSurfs: public FindMoabSurfacesTest {
protected:

  FindOffsetSurfs() :
    FindMoabSurfacesTest("offset-box.i") {

    nNodesExpect=15;
    nElemsExpect=24;

    // Define the material names expected for this input file
    mat_names.push_back("mat:copper");
    mat_names.push_back("mat:Graveyard");

  };

};


class FindLogBinSurfs: public FindMoabSurfacesTest {
protected:

  FindLogBinSurfs() :
    FindMoabSurfacesTest("findsurfstest-logbins.i") {

    // Define the material names expected for this input file
    mat_names.push_back("mat:copper");
    mat_names.push_back("mat:Graveyard");

    // Max number of outputs
    nOutput=10;

    // How often should we skip write?
    nSkip=0; // never skip

    // Expected base name of file
    output_base="moab_surfs";

  };

};

class FindSurfsNodalTemp: public FindMoabSurfacesTest {
protected:

  FindSurfsNodalTemp() :
    FindMoabSurfacesTest("nodal_temperature.i") {

    // Define the material names expected for this input file
    mat_names.push_back("mat:copper");
    mat_names.push_back("mat:air");
    mat_names.push_back("mat:Graveyard");

  };

  virtual void SetUp() override {

    // Create app
    OpenMCAppInputTest::SetUp();

    if(appIsNull) return;

    try{

      ASSERT_NO_THROW(app->run());

      // Get the FE problem
      problemPtr = &(app->getExecutioner()->feProblem());

      // Check for MOAB user object
      if(!(problemPtr->hasUserObject("moab")))
        throw std::logic_error("Could not find MoabUserObject with name 'moab'. Please check your input file.");

      // Get the MOAB user object
      moabUOPtr = &(problemPtr->getUserObject<MoabUserObject>("moab"));

      foundMOAB = true;
     }
    catch(std::exception& e){
      std::cout<<e.what()<<std::endl;
    }
  }

  virtual void TearDown() override {};

};


class DeformedMeshTest : public MoabUserObjectTestBase {
protected:
  DeformedMeshTest() :
    MoabUserObjectTestBase("thermal-expansion.i"),
    foundPP(false)
  {};

  virtual void SetUp() override {

    // Create app
    OpenMCAppInputTest::SetUp();

    if(appIsNull) return;

    try{

      ASSERT_NO_THROW(app->run());

      // Get the FE problem
      problemPtr = &(app->getExecutioner()->feProblem());

      // Check for MOAB user object
      if(!(problemPtr->hasUserObject("moab")))
        throw std::logic_error("Could not find MoabUserObject with name 'moab'. Please check your input file.");

      // Get the MOAB user object
      moabUOPtr = &(problemPtr->getUserObject<MoabUserObject>("moab"));

      foundMOAB = true;

      // Get the volume post processors
      processMeshVol = &(problemPtr->getUserObject<VolumePostprocessor>("volume_calc_orig"));
      processDeformedMeshVol = &(problemPtr->getUserObject<VolumePostprocessor>("volume_calc_deformed"));

      foundPP = (processMeshVol!= nullptr) && (processDeformedMeshVol!= nullptr);
     }
    catch(std::exception& e){
      std::cout<<e.what()<<std::endl;
    }
  }

  double getTotalVolume(std::shared_ptr<moab::Interface> moabPtr,
                        const std::vector<moab::EntityHandle>& ents){

    double sum(0.);
    for(const auto& ent : ents){
      double elemVol  = calcVolumeFromEnt(moabPtr,ent);
      sum += elemVol;
    }

    return sum;
  }


  double calcVolumeFromEnt(std::shared_ptr<moab::Interface> moabPtr,
                           moab::EntityHandle elem){

    moab::ErrorCode rval;

    // Fetch elem connectivity
    std::vector<moab::EntityHandle> conn;
    rval = moabPtr->get_connectivity(&elem, 1, conn);
    if (rval != moab::MB_SUCCESS){
      throw std::logic_error("Failed to get connectivity.");
    }

    // Fetch vertex coords
    size_t nNodes = conn.size();
    if(nNodes==0){
      throw std::logic_error("Failed to get nodes.");
    }

    // Fetch the coordinates of the nodes
    std::vector<double> nodeCoords(nNodes*3);
    rval = moabPtr->get_coords(conn.data(), nNodes, nodeCoords.data());
    if (rval != moab::MB_SUCCESS){
      throw std::logic_error("Failed to get nodes' coords.");
    }

    // Convert nodes into points
    std::vector<Point> points(nNodes);
    for(size_t inode=0; inode<nNodes; ++inode){
      for(int i=0; i<3; i++){
        double coord = nodeCoords.at(3*inode +i);
        (points.at(inode))(i) = coord;
      }
    }

    // Get vectors for the sides of the tet
    Point s1 = points.at(1) - points.at(0);
    Point s2 = points.at(2) - points.at(0);
    Point s3 = points.at(3) - points.at(0);

    double vol = calcTetVolume(s1,s2,s3);
    return vol;

  }

  // If u, v, and w are vectors for the sides of the tet, return volume
  double calcTetVolume(const Point& u, const Point& v, const Point& w){
    return fabs(u.cross(v) * w)/ 6.0;
  }

  VolumePostprocessor* processMeshVol;
  VolumePostprocessor* processDeformedMeshVol;
  bool foundPP;

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

  initMoabTest();

}

// Test for setting FE problem solution
TEST_F(MoabUserObjectTest, setSolution)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Perform the test
  setSolutionTest();
}

// Test for setting FE problem solution
TEST_F(MoabUserObjectTest, zeroSolution)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get number of elems
  MeshBase& mesh = problemPtr->mesh().getMesh();
  unsigned int nElem = mesh.n_elem();

  // Set a solution that is everywhere 0.
  // Normally generates a warning, but in these unit tests
  // mooseWarning throws, which is handled safely and returns false
  std::vector<double> zeroSol(nElem,0.);
  EXPECT_FALSE(moabUOPtr->setSolution(var_name,zeroSol,1.0,false));

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
    EXPECT_EQ(ents.size(),size_t(0));
  }

}

// Test for second-order MOAB mesh initialisation
TEST_F(SecondOrderMoabUserObjectTest, init)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  initMoabTest();
}

// Test for second-order MOAB mesh solution
TEST_F(SecondOrderMoabUserObjectTest, setSolution)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());


  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Create a vector for setting "openmc" solution
  std::vector<double> solutionData;

  // Create a vector for libmesh solution
  std::vector<double> solutionCompareData;

  // Generate solution data (const across sub-tets)
  size_t elemDegen=8;
  size_t nElemsLibMesh = nElemsExpect/elemDegen;
  double solInitial=300.;
  double solDiff=1.;
  for(size_t iElem=0; iElem<nElemsLibMesh; iElem++){
    double solNow = solInitial * solDiff*iElem;
    solutionCompareData.push_back(solNow*elemDegen);
    for(size_t iDegen=0; iDegen<elemDegen; iDegen++){
      solutionData.push_back(solNow);
    }
  }

  EXPECT_TRUE(moabUOPtr->setSolution(var_name,
                                     solutionData,
                                     1.0,
                                     false));


  checkSolution(solutionCompareData);

}

// Test for finding surfaces
TEST_F(FindMoabSurfacesTest, constTemp)
{
  checkConstTempSurfs(300,3,4);
}

TEST_F(FindMoabSurfacesTest, singleBin)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());


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

  // Check volume->surf relationships
  VolumeInfo volinfo;

  // Copper block
  volinfo.vol_id=1;
  volinfo.surf_ids = {1};
  volinfo.temp=300.;
  volinfo.isGraveyard=false;
  checkSurfsAndTemp(volinfo);

  // Region of air
  volinfo.vol_id=2;
  volinfo.surf_ids = {1,2};
  checkSurfsAndTemp(volinfo);

  // Graveyard
  volinfo.vol_id=3;
  volinfo.surf_ids = {3,4};
  volinfo.isGraveyard=true;
  checkSurfsAndTemp(volinfo);

}

TEST_F(FindMoabSurfacesTest, extraBin)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());

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
  VolumeInfo volinfo;

  // Copper block
  volinfo.vol_id=1;
  volinfo.surf_ids = {1};
  volinfo.temp=300.;
  volinfo.isGraveyard=false;
  checkSurfsAndTemp(volinfo);

  // Region of air at cooler T
  volinfo.vol_id=2;
  volinfo.surf_ids = {1,2,3};
  checkSurfsAndTemp(volinfo);

  // Region of air at hotter T
  volinfo.vol_id=3;
  volinfo.surf_ids = {3};
  volinfo.temp=305.;
  checkSurfsAndTemp(volinfo);

  // Graveyard
  volinfo.vol_id=4;
  volinfo.surf_ids = {4,5};
  volinfo.isGraveyard=true;
  checkSurfsAndTemp(volinfo);

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
  volinfo.vol_id=1;
  volinfo.surf_ids = {1,3,5};
  volinfo.temp=300.;
  volinfo.isGraveyard=false;
  checkSurfsAndTemp(volinfo);

  // Block of copper at higher T
  volinfo.vol_id=2;
  volinfo.surf_ids={2,3,6};
  volinfo.temp=305.;
  checkSurfsAndTemp(volinfo);

  // Region of air at lower T
  volinfo.vol_id=3;
  volinfo.surf_ids ={4,5,6,7};
  volinfo.temp=300.;
  checkSurfsAndTemp(volinfo);

  // Region of air at higher T
  volinfo.vol_id=4;
  volinfo.surf_ids ={1,2,7};
  volinfo.temp=305.;
  checkSurfsAndTemp(volinfo);

  // Graveyard
  volinfo.vol_id=5;
  volinfo.surf_ids ={8,9};
  volinfo.isGraveyard=true;
  checkSurfsAndTemp(volinfo);

}

// Test for checking output
TEST_F(FindMoabSurfacesTest, checkOutput)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Set a constant solution
  double solConst = 300.;
  setConstSolution(ents,solConst);

  // How many times to update
  unsigned int nUpdate=15;
  checkOutputAfterUpdate(nUpdate);

}

// Test for finding surfaces for second order mesh
TEST_F(SecondOrderSurfacesTest, constTemp)
{
  checkConstTempSurfs(37.5,3,4,8);
}

// Test for checking output
TEST_F(FindSurfsNodalTemp, nodalTemperature)
{
  ASSERT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());

  // Fix due to a warning which throws
  Moose::_throw_on_error = false;

  // Find the surfaces
  ASSERT_TRUE(moabUOPtr->update());

  Moose::_throw_on_error = true;

  // Check groups, volumes and surfaces
  unsigned int nVol=4;
  unsigned int nSurf=5;
  checkAllGeomsets(nVol,nSurf);

  // Check volume->surf relationships
  VolumeInfo volinfo;

  // Copper block
  volinfo.vol_id=1;
  volinfo.surf_ids = {1};
  volinfo.temp=300.;
  volinfo.isGraveyard=false;
  checkSurfsAndTemp(volinfo);

  // Region of air at cooler T
  volinfo.vol_id=2;
  volinfo.surf_ids = {1,2,3};
  checkSurfsAndTemp(volinfo);

  // Region of air at hotter T
  volinfo.vol_id=3;
  volinfo.surf_ids = {3};
  volinfo.temp=305.;
  checkSurfsAndTemp(volinfo);

  // Graveyard
  volinfo.vol_id=4;
  volinfo.surf_ids = {4,5};
  volinfo.isGraveyard=true;
  checkSurfsAndTemp(volinfo);

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

// Test for finding surfaces for single material
TEST_F(FindSingleMatSurfs, constTemp)
{
  checkConstTempSurfs(340,2,3);
}

// Test for finding surfaces for many temperature bins
TEST_F(FindSingleMatSurfs, manyBins)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Manufacture a solution that results in many nested surfaces
  double solMax = 420;
  double solMin = 340;
  double rMax = 9.0/log(8)*lengthscale;
  setSolution(ents,rMax,solMax,solMin,1.0,false);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check groups, volumes and surfaces
  unsigned int nVol=6;
  unsigned int nSurf=7;
  checkAllGeomsets(nVol,nSurf);

  // Check volume->surf relationships
  VolumeInfo volinfo;
  volinfo.isGraveyard=false;

  // Surfaces from outside to inside
  std::vector<int> surfaces = {1,3,2,4,5};
  // Width of temperature bins
  double binWidth=20.0;
  for(unsigned int iVol=1; iVol<nVol; iVol++){
    volinfo.vol_id=iVol;
    volinfo.surf_ids.clear();
    int outer = surfaces.at(iVol-1);
    volinfo.surf_ids.insert(outer);
    if(iVol<nVol-1){
      int inner = surfaces.at(iVol);;
      volinfo.surf_ids.insert(inner);
    }
    volinfo.temp = solMin + double(iVol-1)*binWidth;
    checkSurfsAndTemp(volinfo);
  }

  // Graveyard
  volinfo.vol_id=nVol;
  volinfo.surf_ids = {6,7};
  volinfo.isGraveyard=true;
  checkSurfsAndTemp(volinfo);

}

// Test for checking output for different output params
TEST_F(FindSingleMatSurfs, checkOutput)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Set a constant solution
  double solConst = 340.;
  setConstSolution(ents,solConst);

  // How many times to update
  unsigned int nUpdate=15;
  checkOutputAfterUpdate(nUpdate);

}

// Test for correct initialisation of offset box
TEST_F(FindOffsetSurfs, init)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());

  initMoabTest();
}

// Test for finding surfaces for an offset box
TEST_F(FindOffsetSurfs, update)
{
  checkConstTempSurfs(300,2,3);
}

// Test for finding surfaces for log binning
TEST_F(FindLogBinSurfs, constTemp)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());

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

  VolumeInfo volinfo;
  volinfo.isGraveyard=false;

  // Check temperature
  volinfo.vol_id=1;
  volinfo. surf_ids = {1};
  volinfo.temp = pow(10,2.25);
  checkSurfsAndTemp(volinfo);

  // Set a constant solution
  solConst = 400.;
  setConstSolution(ents,solConst);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check temperature
  volinfo.temp = pow(10,2.75);
  checkSurfsAndTemp(volinfo);

  // Set a constant solution
  solConst = 2000.;
  setConstSolution(ents,solConst);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check temperature
  volinfo.temp = pow(10,3.25);
  checkSurfsAndTemp(volinfo);

  // Set a constant solution
  solConst = 5000.;
  setConstSolution(ents,solConst);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check temperature
  volinfo.temp = pow(10,3.75);
  checkSurfsAndTemp(volinfo);

}

// Test for finding surfaces for many temperature bins on log scale
TEST_F(FindLogBinSurfs, manyBins)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());


  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Manufacture a solution that results in many nested surfaces
  double solMax = 6000;
  double solMin = 150;
  double rMax = 2.5*lengthscale;
  setSolution(ents,rMax,solMax,solMin,1.0,false);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check groups, volumes and surfaces
  unsigned int nVol=5;
  unsigned int nSurf=6;
  checkAllGeomsets(nVol,nSurf);

  // Check volume->surf relationships
  VolumeInfo volinfo;
  volinfo.isGraveyard=false;

  // Surfaces from outside to inside
  std::vector<int> surfaces = {1,3,2,4};
  // Parameters to compare the temperature
  double tnow = pow(10,2.25);
  double proddiff = pow(10,0.5);
  for(unsigned int iVol=1; iVol<nVol; iVol++){
    volinfo.vol_id=iVol;
    volinfo.temp=tnow;
    volinfo.surf_ids.clear();
    int outer = surfaces.at(iVol-1);
    volinfo.surf_ids.insert(outer);
    if(iVol<nVol-1){
      int inner = surfaces.at(iVol);;
      volinfo.surf_ids.insert(inner);
    }
    checkSurfsAndTemp(volinfo);
    // Update temperature for next iteration;
    tnow *= proddiff;
  }

  // Graveyard
  volinfo.vol_id=nVol;
  volinfo.surf_ids = {5,6};
  volinfo.isGraveyard=true;
  checkSurfsAndTemp(volinfo);

}

// Test to check we are using the deformed mesh if there is one
TEST_F(DeformedMeshTest, checkDeformedMesh)
{
  ASSERT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(foundPP);
  ASSERT_TRUE(setProblem());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get the MOAB interface to check the data
  std::shared_ptr<moab::Interface> moabPtr = moabUOPtr->moabPtr;
  ASSERT_NE(moabPtr,nullptr);

  // Get root set
  moab::EntityHandle rootset = moabPtr->get_root_set();

  // Fetch the elements
  std::vector<moab::EntityHandle> ents;
  moab::ErrorCode rval = moabPtr->get_entities_by_type(rootset,moab::MBTET,ents);
  ASSERT_EQ(rval,moab::MB_SUCCESS);

  // Sanity check
  EXPECT_EQ(ents.size(),size_t(3000));

  // Calculate the volume of all the elements
  double meshVolTest = getTotalVolume(moabPtr,ents);

  // Compare manually calculated volume to the postprocessor values.
  double meshVolOrig = processMeshVol->getValue();
  double meshVolDef = processDeformedMeshVol->getValue();

  // Check the original volume is 5x5x5 cube
  EXPECT_LT(fabs(meshVolOrig-125.0),tol);

  // Check the deformed mesh volume is bigger from thermal expansion
  EXPECT_GT(meshVolDef,meshVolOrig);

  // Check MOAB volume equals deformed mesh volume
  EXPECT_LT(fabs(meshVolDef-meshVolTest),tol);

  // Check the difference between deformed and original is above our tolerance
  EXPECT_GT(meshVolTest-meshVolOrig,tol);

}
