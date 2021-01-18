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

  checkKnownObjects(knownObjNames);
}

class MinimalInputTest : public OpenMCAppInputTest{
protected:
  MinimalInputTest() : OpenMCAppInputTest("minimal.i") {};
};

TEST_F(MinimalInputTest, readInput)
{
  ASSERT_FALSE(appIsNull);

  ASSERT_NO_THROW(app->setupOptions());
  ASSERT_NO_THROW(app->runInputFile());

}
