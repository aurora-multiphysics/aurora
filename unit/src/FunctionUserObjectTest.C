//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "FunctionUserObject.h"
#include "MoabMeshTransferTest.h"

// Fixture to test the MOAB mesh transfer
class FunctionUserObjectTest : public MoabMeshTransferTest {
protected:
  FunctionUserObjectTest() :
    MoabMeshTransferTest("function_user_object.i"),
    var_name("heating-local")
  {};

  virtual void SetUp() override {

    // Call the base class method
    MoabMeshTransferTest::SetUp();

    // Check setup was successful
    ASSERT_NE(mbMeshTransferPtr, nullptr);
    ASSERT_NE(moabUOPtr, nullptr);

    // Perform the transfer
    ASSERT_NO_THROW(mbMeshTransferPtr->execute());

    // moabUOPtr should now have a problem set
    ASSERT_TRUE(moabUOPtr->hasProblem());

    // Get the function user object
    functionUOPtr = &(problemPtr->getUserObject<FunctionUserObject>("uo-heating-function"));

  }

  FunctionUserObject* functionUOPtr;
  std::string var_name;

};

TEST_F(FunctionUserObjectTest, initMOAB)
{
  // Check setup was successful
  EXPECT_NE(functionUOPtr, nullptr);

  ASSERT_NO_THROW(moabUOPtr->initMOAB());
}
