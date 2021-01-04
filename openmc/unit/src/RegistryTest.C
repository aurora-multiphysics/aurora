//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BasicTest.h"

TEST_F(BasicTest, registryTest)
{

  ASSERT_FALSE(appIsNull);

  // Fetch a reference to all objects that been registered (in the global singleton)
  const auto& allRegistered = Registry::allObjects();

  // Check OpenMCApp exists in the registry
  bool foundOpenMC = allRegistered.find("OpenMCApp") != allRegistered.end();
  ASSERT_TRUE(foundOpenMC);

  // Get all objects registered to OpenMC
  const auto& auroraObjs = allRegistered.at("OpenMCApp");

  // Create a list of names of all objects we expect to see registered
  std::map<std::string,bool> knownObjs;
  knownObjs["OpenMCProblem"] = false;
  knownObjs["OpenMCExecutioner"] = false;
  knownObjs["MoabUserObject"] = false;

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
                                 << " was not registered to OpenMCApp.";
  }

}

class MinimalInputTest : public InputFileTest{
protected:
  MinimalInputTest() : InputFileTest("minimal.i") {};
};

TEST_F(MinimalInputTest, readInput)
{
  ASSERT_FALSE(appIsNull);

  ASSERT_NO_THROW(app->setupOptions());
  ASSERT_NO_THROW(app->runInputFile());

}
