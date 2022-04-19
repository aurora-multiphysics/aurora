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
class MoabUserObjectTestBase : public OpenMCAppInputTest {
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

  bool getCentroid(std::shared_ptr<moab::Interface> moabPtr,
                   moab::EntityHandle elem,
                   Point & centroid,
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
    centroid = Point(0.,0.,0.);
    for(size_t inode=0; inode<nNodes; ++inode){
      for(int i=0; i<3; i++){
        double coord = nodeCoords.at(3*inode +i);
        centroid(i)+= coord /double(nNodes);
      }
    }

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
      std::string errmsg;
      Point centroid;
      bool success = getCentroid(moabUOPtr->moabPtr,ent,centroid,errmsg);
      ASSERT_TRUE(success) << errmsg;
      double radius = centroid.norm();
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
                                       scalefactor,false,normToVol));

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

  // Helper function to create a constant solution
  void setConstSolution(size_t solSize, double solConst, std::string var_name_in) {
    // Create a vector for constant solution
    std::vector<double> solutionData(solSize,solConst);

    // Check we can set the solution
    ASSERT_TRUE(moabUOPtr->setSolution(var_name_in,solutionData,1.0,false,false));
  }

  // Overloaded version
  void setConstSolution(size_t solSize, double solConst,std::vector<double>& solExpect){
    setConstSolution(solSize,solConst,var_name);
    solExpect.resize(solSize,solConst);
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
    EXPECT_THROW(moabUOPtr->setSolution("dummy",
                                        dummySolutionData,
                                        scalefactor,false,normToVol),
                 std::runtime_error);

    // Expect failure due to empty solution
    EXPECT_FALSE(moabUOPtr->setSolution(var_name,
                                        dummySolutionData,
                                        scalefactor,false,normToVol));

    for(int i=0; i<2; i++){

      std::vector<double> solExpect;

      if(i==0){
        // Set a constant solution
        setConstSolution(ents.size(),solConst,solExpect);
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


  void setErrorsTest(int nDegen=1){
    // Set the mesh
    ASSERT_NO_THROW(moabUOPtr->initMOAB());

    // Create a vector for constant solution
    double value=4.0;
    std::vector<double> errData(nElemsExpect,value);

    // Check we can set the solution
    ASSERT_TRUE(moabUOPtr->setSolution(var_name,errData,1.0,true,false));

    size_t nElemsLibMesh = nElemsExpect/nDegen;
    std::vector<double> solExpect(nElemsLibMesh,sqrt(value*double(nDegen)));
    checkSolution(solExpect);
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
    setBinningDefaults();
    initMats();
    setOutputDefaults();
  };

  FindMoabSurfacesTest(std::string inputfile) :
    MoabUserObjectTest(inputfile) {
    setBinningDefaults();
    setOutputDefaults();
  };

  void initMats() {
    setBaseNames();
    setMatNames();
  }

  void setBinningDefaults(){
    nDenBins=1;
    nTempBins=60;
    tempMin=297.5;
    tempMax=597.5;
    relDenMin=-0.1;
    relDenMax=0.1;
  }

  void setOutputDefaults(){

    // Max number of outputs
    nOutput=10;

    // How often should we skip write?
    nSkip=0; // never skip

    // Expected base name of file
    output_base="moab_surfs";
  }

  virtual void setBaseNames() {
    // Define the material names expected for this input file
    base_names.push_back("mat:copper");
    base_names.push_back("mat:air");
  };

  virtual void setMatNames(){
    for(const auto name : base_names){
      for(unsigned int iDen=0; iDen<nDenBins; iDen++){
        for(unsigned int iTemp=0; iTemp<nTempBins; iTemp++){
          unsigned int iMat = nTempBins*iDen + iTemp;
          std::string new_name = name+"_"+std::to_string(iMat);
          mat_names.push_back(new_name);
        }
      }
    }
    mat_names.push_back("mat:Graveyard");
  };

  virtual void setTemperatures() {
    ASSERT_GT(nTempBins,0);
    double binWidth=(tempMax-tempMin)/double(nTempBins);
    ASSERT_GT(binWidth,0.);
    double tempNow = tempMin - 0.5*binWidth;
    for(unsigned int iTemp=0; iTemp<nTempBins; iTemp++){
      tempNow += binWidth;
      mat_temperatures.push_back(tempNow);
    }
  };

  virtual void setDensities() {
    ASSERT_GT(nDenBins,0);
    double binWidth=(relDenMax-relDenMin)/double(nDenBins);
    ASSERT_GT(binWidth,0.);
    double relDenNow = relDenMin - 0.5*binWidth;
    for(unsigned int iDen=0; iDen<nDenBins; iDen++){
      relDenNow += binWidth;
      mat_densities.push_back(relDenNow);
    }
  };

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

  void init()
  {
    EXPECT_FALSE(appIsNull);
    ASSERT_TRUE(foundMOAB);
    ASSERT_TRUE(setProblem());
    ASSERT_NO_THROW(moabUOPtr->initBinningData());

    // Set the mesh
    ASSERT_NO_THROW(moabUOPtr->initMOAB());
  }

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

      // Get the values of name tags
      auto names = new char[nCat][NAME_TAG_SIZE];
      rval = moabPtr->tag_get_data (name_tag,ents,names);
      if(cat =="Group")
        EXPECT_EQ(rval,moab::MB_SUCCESS);
      else
        EXPECT_NE(rval,moab::MB_SUCCESS);

      // Get the values of dimension (geom) tag
      std::vector<int> dims(nCat);
      rval = moabPtr->tag_get_data (geom_tag,ents,dims.data());
      EXPECT_EQ(rval,moab::MB_SUCCESS);

      // Get the values of ID tag
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

  void checkSurfs(VolumeInfo volinfo){

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
  }

  void matMetadataTest()
  {
    // Retrieve material data
    std::vector<std::string> mat_names_check;
    std::vector<double> initial_densities;
    std::vector<std::string> tails;
    std::vector<MOABMaterialProperties> properties;
    ASSERT_NO_THROW(moabUOPtr->getMaterialProperties(mat_names_check,
                                                     initial_densities,
                                                     tails,
                                                     properties));

    // Materials should be the same
    size_t nMats = base_names.size();
    ASSERT_EQ(mat_names_check.size(),nMats);
    for(auto name: mat_names_check){
      std::string err="Failed to find material "+name;
      name = "mat:"+name;
      auto pos = find(base_names.begin(),base_names.end(),name);
      bool found_name = (pos != base_names.end());
      EXPECT_TRUE(found_name)<<err;
    }

    bool binByDensity = !initial_densities.empty();

    if(binByDensity){
      // Compare initial densities of materials
      ASSERT_EQ(initial_densities.size(),nMats);
      ASSERT_EQ(orig_densities.size(), nMats);
      for(size_t iMat=0; iMat<nMats; iMat++){
        EXPECT_EQ(initial_densities.at(iMat),orig_densities.at(iMat));
      }
    }

    setTemperatures();
    setDensities();

    unsigned int nMatBins = nDenBins*nTempBins;
    ASSERT_EQ(properties.size(),nMatBins);
    ASSERT_EQ(tails.size(),nMatBins);

    for(unsigned int iDen=0; iDen<nDenBins; iDen++){
      // Get relative density change of  bin
      double relDenCheck = mat_densities.at(iDen);

      for(unsigned int iTemp=0; iTemp<nTempBins; iTemp++){
        // Get temperature of bin
        double tempCheck = mat_temperatures.at(iTemp);

        // Get material bin
        unsigned int iMatBin = nTempBins*iDen + iTemp;

        // Check mat name modifier
        std::string tail_cmp = "_"+std::to_string(iMatBin);
        EXPECT_EQ(tails.at(iMatBin),tail_cmp);

        // Fetch properties values for material
        MOABMaterialProperties mat_props = properties.at(iMatBin);
        // Get the relative density
        double relDen = mat_props.rel_density;
        // Get the temperature
        double temp = mat_props.temp;

        // Check relative density
        EXPECT_EQ(relDen,relDenCheck);

        // Check temperature
        EXPECT_EQ(temp,tempCheck);
      }
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

    // Set a constant solution
    setConstSolution(nElemsExpect*nDegen,solConst,var_name);

    // Find the surfaces
    ASSERT_TRUE(moabUOPtr->update());

    // Check groups, volumes and surfaces
    checkAllGeomsets(nVol,nSurf);

    // Check volume->surf relationships
    VolumeInfo volinfo;
    volinfo.isGraveyard=false;
    for(unsigned int iVol=1; iVol<nVol; iVol++){
      volinfo.vol_id=iVol;
      volinfo.surf_ids.clear();
      volinfo.surf_ids.insert(iVol);
      if(iVol>1){
        volinfo.surf_ids.insert(iVol-1);
      }
      checkSurfs(volinfo);
    }

    // Graveyard
    volinfo.vol_id=nVol;
    volinfo.surf_ids = {int(nSurf),int(nSurf)-1};
    volinfo.isGraveyard=true;
    checkSurfs(volinfo);
  }

  // Vector to hold material names
  std::vector<std::string> mat_names;

  // Variables relating to output params
  unsigned int nOutput;
  unsigned int nSkip;
  std::string output_base;

  // Variables relating to material created by binning
  unsigned int nTempBins;
  unsigned int nDenBins;
  std::vector<std::string> base_names;
  double relDenMin;
  double relDenMax;
  double tempMin;
  double tempMax;
  std::vector<double> orig_densities;
  std::vector<double> mat_densities;
  std::vector<double> mat_temperatures;


};

// Repeat surfaces test for second order mesh
class SecondOrderSurfacesTest : public FindMoabSurfacesTest {
protected:

  SecondOrderSurfacesTest() :
    FindMoabSurfacesTest("findsurfstest-second.i") {
    initMats();
  }

};

class FindSingleMatSurfs: public FindMoabSurfacesTest {
protected:

  FindSingleMatSurfs() :
    FindMoabSurfacesTest("findsurfstest-singlemat.i") {

    nTempBins=5;

    initMats();

    // Max number of outputs
    nOutput=4;

    // How often should we skip write?
    nSkip=2;  // write every third

    // Expected base name of file
    output_base="random_name";

  };

  virtual void setBaseNames() override {
    // Define the material names expected for this input file
    base_names.push_back("mat:copper");
  };


};

class FindOffsetSurfs: public FindMoabSurfacesTest {
protected:

  FindOffsetSurfs() :
    FindMoabSurfacesTest("offset-box.i") {
    initMats();
    nNodesExpect=15;
    nElemsExpect=24;
  };

  virtual void setBaseNames() override {
    // Define the material names expected for this input file
    base_names.push_back("mat:copper");
  };

};


class FindLogBinSurfs: public FindMoabSurfacesTest {
protected:

  FindLogBinSurfs() :
    FindMoabSurfacesTest("findsurfstest-logbins.i") {

    nTempBins=4;
    tempMin=150;
    tempMax=6000;

    initMats();

    // Max number of outputs
    nOutput=10;

    // How often should we skip write?
    nSkip=0; // never skip

    // Expected base name of file
    output_base="moab_surfs";

  };

  virtual void setBaseNames() override {
    // Define the material names expected for this input file
    base_names.push_back("mat:copper");
  };


  virtual void setTemperatures() {
    ASSERT_GT(nTempBins,0);
    int powMin = int(floor(log10(tempMin)));
    int powMax = int(ceil(log10(tempMax)));
    int nPow = std::max(powMax-powMin, 1);
    int nMinor = nTempBins/nPow;
    ASSERT_GT(nMinor,0);
    double powDiff = 1./double(nMinor);
    double powStart = double(powMin) - 0.5*powDiff;
    double tempNow = pow(10,powStart);
    double prodDiff = pow(10,powDiff);
    for(unsigned int iTemp=0; iTemp<nTempBins; iTemp++){
      tempNow *= prodDiff;
      mat_temperatures.push_back(tempNow);
    }
  };

};

class FindSurfsNodalTemp: public FindMoabSurfacesTest {
protected:

  FindSurfsNodalTemp() :
    FindMoabSurfacesTest("nodal_temperature.i") {

    initMats();
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

class FindDensitySurfsTestBase: public FindMoabSurfacesTest {
protected:

  FindDensitySurfsTestBase(std::string inputfile):
    FindMoabSurfacesTest(inputfile)
  {
    density_name="density_local";
    nDenBins=5;

    // sides of box
    xMinSav=-10.0*lengthscale;
    xMaxSav=10.0*lengthscale;
  }

  double getLinSol(double x, double xMin, double xMax, double solMin, double solMax)
  {
    double xRel = (x-xMin)/(xMax-xMin);
    double sol = xRel * (solMax-solMin) + solMin;
    return sol;
  }

  void getLinearSolution(const std::vector<moab::EntityHandle>& ents,
                         std::vector<double>& solutionData,
                         double solMin, double solMax,unsigned int iAxis)
  {
    ASSERT_LT(iAxis,3);
    // Manufacture a linearly varying solution based on i-coord of element centroid.
    for(const auto& ent : ents){
      std::string errmsg;
      Point centroid;
      bool success = getCentroid(moabUOPtr->moabPtr,ent,centroid,errmsg);
      ASSERT_TRUE(success) << errmsg;
      double solNow = getLinSol(centroid(iAxis),xMinSav,xMaxSav,solMin,solMax);
      ASSERT_GT(solNow,solMin);
      ASSERT_LT(solNow,solMax);
      solutionData.push_back(solNow);
    }
  }


  void setLinearSolution(std::string var_name_in,
                         double solMin, double solMax,unsigned int iAxis){

    // Get elems
    std::vector<moab::EntityHandle> ents;
    getElems(ents);

    // Populate a vector with some manufactured solution values
    std::vector<double> solutionData;
    getLinearSolution(ents,solutionData,solMin,solMax,iAxis);

    // Set the solution
    ASSERT_TRUE(moabUOPtr->setSolution(var_name_in,
                                       solutionData,
                                       1.0,false,false));
  }

  void linearDensityTest(double denOrig,double relDiff,double temp,
                         unsigned int nVol,unsigned int nSurf)
  {
    double denMin = denOrig*(1.0-relDiff);
    double denMax = denOrig*(1.0+relDiff);

    // Set a linearly varying density along x
    setLinearSolution(density_name,denMin,denMax,0);

    // Set a constant temp
    setConstSolution(nElemsExpect,temp,var_name);

    // Find the surfaces
    ASSERT_TRUE(moabUOPtr->update());

    // Check groups, volumes and surfaces
    checkAllGeomsets(nVol,nSurf);
  }

  void linearDensityTempTest(double denOrig,double relDiff,
                             double tempMin,double tempMax,
                             unsigned int nVol,unsigned int nSurf)
  {
    double denMin = denOrig*(1.0-relDiff);
    double denMax = denOrig*(1.0+relDiff);

    // Set a linearly varying density along x
    setLinearSolution(density_name,denMin,denMax,0);

    // Set a linearly varying temp along x
    setLinearSolution(var_name,tempMin,tempMax,1);

    // Find the surfaces
    ASSERT_TRUE(moabUOPtr->update());

    // Check groups, volumes and surfaces
    checkAllGeomsets(nVol,nSurf);
  }

  std::string density_name;
  double xMinSav;
  double xMaxSav;

};

class FindDensitySurfsTest: public FindDensitySurfsTestBase {
protected:
  FindDensitySurfsTest() :
    FindDensitySurfsTestBase("densitysurfstest.i")
  {
    orig_densities.push_back(8.920);
    initMats();
  };

  FindDensitySurfsTest(std::string inputfile) :
    FindDensitySurfsTestBase(inputfile)
  {
    orig_densities.push_back(8.920);
    initMats();
  };

  virtual void setBaseNames() override {
    // Define the material names expected for this input file
    base_names.push_back("mat:copper");
  };


};

class FindDensitySurfsUnitTest: public FindDensitySurfsTest {
protected:
  FindDensitySurfsUnitTest() :
    FindDensitySurfsTest("densitysurfstest-changeunits.i"){};
};


class MoabDeformedMeshTest : public MoabUserObjectTestBase {
protected:
  MoabDeformedMeshTest() :
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
