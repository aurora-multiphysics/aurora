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

#include "MooseObject.h"

TEST(RegistryTest, registryTest)
{

  // Fetch a reference to all objects that been registered (in the global singleton)
  const auto& allRegistered = Registry::allObjects();

  // Check we can find heat conduction
  bool foundHeatConduction = Registry::isRegisteredObj("FunctionUserObject");
  EXPECT_TRUE(foundHeatConduction);

  // Check Aurora exists in the registry
  bool foundAurora = allRegistered.find("AuroraApp") != allRegistered.end();
  ASSERT_TRUE(foundAurora);

  // Get all objects registered to Aurora
  const auto& auroraObjs = allRegistered.at("AuroraApp");

  // Create a list of names of all objects we expect to see registered
  std::map<std::string,bool> knownObjs;
  knownObjs["FunctionUserObject"] = false;
  knownObjs["MoabMeshTransfer"] = false;
  knownObjs["VariableFunction"] = false;

  // Check the objects match up
  for( const auto & obj : auroraObjs) {
    std::string objName = obj._classname;
    bool isKnownObj = knownObjs.find(objName) != knownObjs.end();
    EXPECT_TRUE(isKnownObj) << "Unknown object was registered";
    if(isKnownObj){
      // Shouldn't already have marked found
      EXPECT_FALSE(knownObjs[objName]) << "Duplicate enties of object "<< objName;

      // Mark that we found this object
      knownObjs[objName] = true;
    }
  }

  // Now should have found all known objects
  for( const auto & knownObj : knownObjs){
    EXPECT_TRUE(knownObj.second) << "Object "<< knownObj.first
                                 << " was not registered to AuroraApp.";
  }

}
