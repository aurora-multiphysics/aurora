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
    InputFileTest("AuroraApp",inputfile) {};

  virtual void SetUp() override {
    createApp();
  };
};
