#pragma once

#include "BasicTest.h"

class AuroraAppBasicTest : public BasicTest {
protected:

  AuroraAppBasicTest(): BasicTest("AuroraApp") {};

  virtual void SetUp() override {
    createApp();
  };
};

class AuroraAppInputTest : public InputFileTest {
protected:

  AuroraAppInputTest(std::string inputfile) :
    InputFileTest("AuroraApp",inputfile,"aurora_app_inputs/") {};

  virtual void SetUp() override {
    createApp();
  };
};

class AuroraAppRunTest : public OpenMCRunTest {
protected:

  AuroraAppRunTest(std::string inputfile) :
    OpenMCRunTest("AuroraApp",inputfile,"aurora_app_inputs/") {
  };

  virtual void SetUp() override {
    // Change whether to throw on error for duration of this test
    // This is because currently warnings are viewed as errors
    // and we have some unavoidable ones in this test.
    throw_on_error_save = Moose::_throw_on_error;
    Moose::_throw_on_error = false;

    OpenMCRunTest::SetUp();
    createApp();
  };

  virtual void TearDown() override {
    OpenMCRunTest::TearDown();
    Moose::_throw_on_error = throw_on_error_save;
  }

  bool throw_on_error_save;

};
