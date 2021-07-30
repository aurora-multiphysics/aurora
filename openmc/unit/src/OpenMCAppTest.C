//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "OpenMCAppTest.h"

TEST_F(OpenMCAppBasicTest, registryTest)
{
  std::vector<std::string> knownObjNames;
  knownObjNames.push_back("OpenMCProblem");
  knownObjNames.push_back("OpenMCExecutioner");
  knownObjNames.push_back("MoabUserObject");
  knownObjNames.push_back("OpenMCDensity");
  knownObjNames.push_back("ADOpenMCDensity");

  checkKnownObjects(knownObjNames);
}

class OpenMCMinimalInputTest : public OpenMCAppInputTest{
protected:
  OpenMCMinimalInputTest() : OpenMCAppInputTest("minimal.i") {};
};

TEST_F(OpenMCMinimalInputTest, readInput)
{
  ASSERT_FALSE(appIsNull);

  ASSERT_NO_THROW(app->setupOptions());
  ASSERT_NO_THROW(app->runInputFile());

}

class OpenMCFullRunTest : public OpenMCAppRunTest{
protected:
  OpenMCFullRunTest() : OpenMCAppRunTest("executioner.i") {};

  void checkFullRun(std::string dagfile){

    // Get the current dagmc file
    fetchInputFile(dagfile,dagmcFilename);

    ASSERT_NO_THROW(app->run());

  }

};

TEST_F(OpenMCFullRunTest, UWUW)
{
  ASSERT_FALSE(appIsNull);

  std::string dagFile = "dagmc_uwuw.h5m";
  checkFullRun(dagFile);
}

TEST_F(OpenMCFullRunTest, Legacy)
{
  ASSERT_FALSE(appIsNull);

  std::string dagFile = "dagmc_legacy.h5m";
  checkFullRun(dagFile);
}
