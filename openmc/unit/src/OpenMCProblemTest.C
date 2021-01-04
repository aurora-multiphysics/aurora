//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BasicTest.h"

class FEProblemTest : public InputFileTest{
protected:
  FEProblemTest() : InputFileTest("feproblem.i") {};
};

TEST_F(FEProblemTest, readInput)
{
  ASSERT_FALSE(appIsNull);

  ASSERT_NO_THROW(app->setupOptions());
  ASSERT_NO_THROW(app->runInputFile());
}
