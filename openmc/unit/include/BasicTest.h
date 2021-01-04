#pragma once

#include "gtest/gtest.h"

#include <memory>

#include "AppFactory.h"
#include "MooseObject.h"

class BasicTest : public ::testing::Test {

 protected:

  BasicTest() : args("") {};

  virtual void SetUp() override {

    // Convert string arguments
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

  };

  virtual void TearDown() override {};

  // Arguments for our app
  std::string args;

  // Pointer to our Moose App;
  std::shared_ptr<MooseApp> app;

};
