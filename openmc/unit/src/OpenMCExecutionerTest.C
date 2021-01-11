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
class OpenMCExecutioner : public InputFileTest{
protected:
  OpenMCExecutioner() :
    InputFileTest("executioner.i"),
    isSetUp(false)
  {};

  virtual void SetUp() override {

    // Call the base class method
    InputFileTest::SetUp();

    if(appIsNull) return;

    try{
      app->setupOptions();
      app->runInputFile();

      // Get the executioner
      executionerPtr = app->getExecutioner();

      // Get the FE problem
      problemPtr = &(executionerPtr->feProblem());

      // Check for MOAB user object
      if(!(problemPtr->hasUserObject("moab")))
        throw std::logic_error("Could not find MoabUserObject with name 'moab'. Please check your input file.");

      // Get the MOAB user object
      moabUOPtr = &(problemPtr->getUserObject<MoabUserObject>("moab"));

      isSetUp = true;
    }
    catch(std::exception e){
      std::cout<<e.what()<<std::endl;
    }

  }

  FEProblemBase* problemPtr;
  Executioner* executionerPtr;
  MoabUserObject* moabUOPtr;
  bool isSetUp;

};

TEST_F(OpenMCExecutioner,execute){

  ASSERT_TRUE(isSetUp);
  ASSERT_NE(executionerPtr,nullptr);
  EXPECT_NO_THROW(executionerPtr->execute());

}
