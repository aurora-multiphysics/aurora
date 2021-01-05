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
  MoabUserObjectTest() : MoabUserObjectTestBase("moabuserobject.i") {};
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

  // Check nodes

  // Check elems

  // Check tags

}

// Test for MOAB mesh reseting
// TEST_F(MoabUserObjectTest, reset)
// {
//   ASSERT_TRUE(foundMOAB);

//   // Set the mesh
//   ASSERT_NO_THROW(moabUOPtr->initMOAB());

//   // Clear the mesh
//   ASSERT_NO_THROW(moabUOPtr->reset());

//   // Get the MOAB interface
//   std::shared_ptr<moab::Interface> moabPtr = moabUOPtr->moabPtr;

//   // Check the (lack of) data

// }


// Test for MOAB mesh updating
// {
//   ASSERT_TRUE(foundMOAB);

//   // Set the mesh
//   ASSERT_NO_THROW(moabUOPtr->initMOAB());

//   // Clear the mesh
//   ASSERT_NO_THROW(moabUOPtr->update());

//   // Check surfaces and volumes

//}

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
