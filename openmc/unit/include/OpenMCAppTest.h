#pragma once

#include "BasicTest.h"

class OpenMCAppBasicTest : public BasicTest {
protected:

  OpenMCAppBasicTest(): BasicTest("OpenMCApp") {};

  virtual void SetUp() override {
    createApp();
  };
};

class OpenMCAppInputTest : public InputFileTest {
protected:

  OpenMCAppInputTest(std::string inputfile) :
    InputFileTest("OpenMCApp",inputfile) {};

  virtual void SetUp() override {
    createApp();
  };
};
