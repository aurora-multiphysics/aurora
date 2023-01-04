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
    InputFileTest("OpenMCApp",inputfile,"openmc_app_inputs/") {};

  virtual void SetUp() override {
    createApp();
  };
};

class OpenMCAppRunTest : public OpenMCRunTest {
protected:

  OpenMCAppRunTest(std::string inputfile) :
    OpenMCRunTest("OpenMCApp",inputfile,"openmc_app_inputs/") {};

  virtual void SetUp() override {
    OpenMCRunTest::SetUp();
    createApp();
  };
};
