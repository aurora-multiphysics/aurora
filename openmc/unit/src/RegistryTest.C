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

#include "AppFactory.h"
#include "MooseObject.h"

TEST(RegistryTest, registryTest)
{

  std::string args("");
  char * cstr = new char [args.length()+1];
  std::strcpy (cstr, args.c_str());
  std::vector<char*> arg_list;
  char * next;
  next = strtok(cstr," ");
  while (next != NULL)
  {
    arg_list.push_back(next);
    next = strtok(NULL, " ");
  }
  size_t argc = arg_list.size();
  char ** argv = new char*[argc];
  std::memcpy(argv,arg_list.data(),argc);

  std::shared_ptr<MooseApp> app = AppFactory::createAppShared("OpenMCApp", argc, argv);

  // Deallocate memory for C arrays created with new
  delete argv;
  delete cstr;

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
