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

  double getSolution(double radius, double rMax, double max){
    return max*exp(-radius/rMax);
  }

  void getSolutionData(const std::vector<moab::EntityHandle>& ents, double rMax, double solMax, std::vector<double>& solutionData){
    double solMin = solMax*exp(-1.);

    // Manufacture a solution based on radius of element centroid.
    for(const auto& ent : ents){
      double radius;
      std::string errmsg;
      bool success = calcRadius(moabUOPtr->moabPtr,ent,radius,errmsg);
      ASSERT_TRUE(success) << errmsg;
      EXPECT_GT(radius,0.);
      EXPECT_LT(radius,rMax);
      double solution=getSolution(radius,rMax,solMax);
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
    centroid *= 100.0;
    return centroid.norm();
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
    var_name("heating-local"),
    tol(1.e-9) {};

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


  void setSolution(const std::vector<moab::EntityHandle>& ents, double rMax, double solMax, double scalefactor=1.0, bool normToVol=false) {

    // Create a vector for solutionData
    std::vector<double> solutionData;

    // Populate a vector with some manufactured solution values
    getSolutionData(ents,rMax,solMax,solutionData);

    // Check we can set the solution
    EXPECT_TRUE(moabUOPtr->setSolution(var_name,
                                       solutionData,
                                       scalefactor,normToVol));

  }

  void checkSetSolution(const std::vector<moab::EntityHandle>& ents, double rMax, double solMax, double scalefactor=1.0, bool normToVol=false) {


    // Create a vector for solutionData
    std::vector<double> dummySolutionData;

    // Expect failure due to wrong var name
    EXPECT_THROW(moabUOPtr->setSolution("dummy",dummySolutionData,scalefactor,normToVol),
                 std::runtime_error);

    // Expect failure due to empty solution
    EXPECT_FALSE(moabUOPtr->setSolution(var_name,
                                        dummySolutionData,
                                        scalefactor,normToVol));


    // Set a non-trival solution
    setSolution(ents,rMax,solMax,scalefactor,normToVol);

    // Check the solution we set is correct

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
    for( ; itelem!=endelem; ++itelem){
      Elem& elem = **itelem;

      // Get the degree of freedom number for this element
      dof_id_type soln_index = elem.dof_number(iSys,iVar,0);

      ASSERT_LT(soln_index,solsize);

      // Get the solution value for this element
      double sol = double(sys.solution->el(soln_index));

      // Get the midpoint radius
      double radius = elemRadius(elem);

      // Get the expected solution for this element
      double solExpect = getSolution(radius,rMax,solMax);
      solExpect *= scalefactor;

      if(normToVol){
        double volume = elem.volume();
        solExpect /= volume;
      }

      // Compare
      double solDiff = fabs(sol-solExpect);
      EXPECT_LT(solDiff,tol);

    }

  }

  std::string var_name;

  // Define a tolerance for double comparisons
  double tol;

};

// Test correct failure for ill-defined mesh
class BadMoabUserObjectTest : public MoabUserObjectTestBase {
protected:
  BadMoabUserObjectTest() : MoabUserObjectTestBase("badmoabuserobject.i") {};
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
  double diff = fabs(faceting_tol - 1.e-4);
  EXPECT_LT(diff,tol);

  rval = moabPtr->tag_get_handle("GEOMETRY_RESABS",tag_handle);
  EXPECT_EQ(rval,moab::MB_SUCCESS);

  double geom_tol;
  rval = moabPtr->tag_get_data(tag_handle, &meshset, 1, &geom_tol);
  EXPECT_EQ(rval,moab::MB_SUCCESS);
  diff = fabs(geom_tol - 1.e-6);
  EXPECT_LT(diff,tol);

}


// Test for setting FE problem solution
TEST_F(MoabUserObjectTest, setSolution)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Define maximum radius for box with side length 2100 cm
  double rMax = 1050*sqrt(3);

  // Pick a max solution value
  double solMax = 350.;

  std::vector<double> scalefactors;
  scalefactors.push_back(1.0);
  scalefactors.push_back(5.0);

  for(auto scale: scalefactors){
    checkSetSolution(ents,rMax,solMax,scale,false);
    checkSetSolution(ents,rMax,solMax,scale,true);
  }

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


// // Test for MOAB mesh updating
// TEST_F(MoabUserObjectTest, findSurfs)
// {
//   ASSERT_TRUE(foundMOAB);
//   ASSERT_TRUE(setProblem());

//   // Set the mesh
//   ASSERT_NO_THROW(moabUOPtr->initMOAB());

//   // Get elems
//   std::vector<moab::EntityHandle> ents;
//   getElems(ents);

//   // Set a non-trival solution
//   double rMax = 1050*sqrt(3);
//   double solMax = 350.;
//   setSolution(ents,rMax,solMax);

//   //   // Clear the mesh
//   //   ASSERT_NO_THROW(moabUOPtr->update());

//   //   // Check surfaces and volumes

// }

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
