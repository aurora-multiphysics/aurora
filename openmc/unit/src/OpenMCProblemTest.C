//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "OpenMCAppTest.h"
#include "Executioner.h"
#include "FEProblemBase.h"

class FEProblemTest : public OpenMCAppInputTest{
protected:
  FEProblemTest() : OpenMCAppInputTest("feproblem.i") {};
};

TEST_F(FEProblemTest, readInput)
{
  ASSERT_FALSE(appIsNull);

  ASSERT_NO_THROW(app->setupOptions());
  ASSERT_NO_THROW(app->runInputFile());
}

TEST_F(FEProblemTest, problemIsAlwaysConverged)
{
  ASSERT_FALSE(appIsNull);
  ASSERT_NO_THROW(app->setupOptions());
  ASSERT_NO_THROW(app->runInputFile());

  FEProblemBase& problem = app->getExecutioner()->feProblem();

  // Check the type is correct
  EXPECT_EQ(problem.type(),"OpenMCProblem");

  // Converged should already return true
  EXPECT_TRUE(problem.converged());

  // "Solving" the problem should do absolutely nothing
  ASSERT_NO_THROW(problem.solve());
  EXPECT_TRUE(problem.converged());
}
