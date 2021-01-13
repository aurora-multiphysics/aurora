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

}
