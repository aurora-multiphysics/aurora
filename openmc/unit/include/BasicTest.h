#pragma once

#include "gtest/gtest.h"

#include <memory>

#include "AppFactory.h"
#include "MooseObject.h"

class BasicTest : public ::testing::Test {

 protected:

 BasicTest() : args(""), appIsNull(true) {};

  virtual void SetUp() override {

    // Convert string arguments to char array
    char * cstr = new char [args.length()+1];
    std::strcpy (cstr, args.c_str());

    // Split string by whitespace delimiter
    std::vector<char*> arg_list;
    char * next;
    next = strtok(cstr," ");
    while (next != NULL){
      arg_list.push_back(next);
      next = strtok(NULL, " ");
    }

    // Create array of char*
    size_t argc = arg_list.size();
    char ** argv = new char*[argc];
    for (size_t i = 0; i < argc; i++){
      argv[i]=arg_list.at(i);
    }

    try {
      app = AppFactory::createAppShared("OpenMCApp", argc, argv);
      appIsNull = ( app == nullptr );
    }
    catch(std::exception e){
      std::cout<<e.what()<<std::endl;
      appIsNull = true;
    }

    // Deallocate memory for C arrays created with new
    delete argv;
    delete cstr;

  };

  virtual void TearDown() override {};

  // Arguments for our app
  std::string args;

  // Pointer to our Moose App;
  std::shared_ptr<MooseApp> app;

  bool appIsNull;

};


class InputFileTest : public BasicTest {

 protected:

  InputFileTest(std::string inputfile){
    setInputFile(inputfile);
  };

  InputFileTest(){};

  void setInputFile(std::string inputfile){
    if(inputfile==""){
      throw std::logic_error("Input file string is empty");
    }
    args+=" -i inputs/"+inputfile;
  };

  // Some file utility methods
  bool fileExists(std::string filename){
    std::ifstream f(filename.c_str());
    return f.good();
  }

  void deleteFile(std::string filename){
    std::string err = "Failed to remove " + filename;
    EXPECT_EQ(remove(filename.c_str()),0) << err;
  }


};
