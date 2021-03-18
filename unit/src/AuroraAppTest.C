//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "gtest/gtest.h"
#include <memory>

#include "AuroraAppTest.h"
#include "Executioner.h"

TEST_F(AuroraAppBasicTest, registryTest)
{

  // Check we can find heat conduction
  bool foundHeatConduction = Registry::isRegisteredObj("ADHeatConduction");
  EXPECT_TRUE(foundHeatConduction);


  // Check we can find the objects we need
  std::vector<std::string> knownObjNames;
  knownObjNames.push_back("FunctionUserObject");
  knownObjNames.push_back("MoabMeshTransfer");
  knownObjNames.push_back("VariableFunction");
  checkKnownObjects(knownObjNames);

  knownObjNames.clear();
  knownObjNames.push_back("OpenMCProblem");
  knownObjNames.push_back("OpenMCExecutioner");
  knownObjNames.push_back("MoabUserObject");
  knownObjNames.push_back("OpenMCDensity");
  knownObjNames.push_back("ADOpenMCDensity");
  checkKnownObjects(knownObjNames,"OpenMCApp");

}

class MinimalInputTest : public AuroraAppInputTest{
protected:
  MinimalInputTest() : AuroraAppInputTest("minimal.i") {};
};

TEST_F(MinimalInputTest, readInput)
{
  ASSERT_FALSE(appIsNull);

  ASSERT_NO_THROW(app->setupOptions());
  ASSERT_NO_THROW(app->runInputFile());

}

class FullRunTest : public AuroraAppRunTest{
protected:
  FullRunTest() : AuroraAppRunTest("aurora.i") {};

  void checkFullRun(std::string dagfile){

    // Get the current dagmc file
    fetchInputFile(dagfile,dagmcFilename);

    ASSERT_NO_THROW(app->run());

    // Get the executioner
    Executioner* executionerPtr = app->getExecutioner();
    ASSERT_NE(executionerPtr,nullptr);

    EXPECT_TRUE(executionerPtr->lastSolveConverged());
  }

};

TEST_F(FullRunTest, UWUW)
{
  ASSERT_FALSE(appIsNull);

  std::string dagFile = "dagmc_uwuw.h5m";
  checkFullRun(dagFile);
}

TEST_F(FullRunTest, Legacy)
{
  ASSERT_FALSE(appIsNull);

  std::string dagFile = "dagmc_legacy.h5m";
  checkFullRun(dagFile);
}
