//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MoabMeshTransferTest.h"

TEST_F(MoabMeshTransferTest, transfer)
{

  // Check setup was successful
  EXPECT_NE(mbMeshTransferPtr, nullptr);
  EXPECT_NE(moabUOPtr, nullptr);

  // moabUOPtr should not have a problem set
  EXPECT_FALSE(moabUOPtr->hasProblem());

  // Perform the transfer
  EXPECT_NO_THROW(mbMeshTransferPtr->execute());

  // moabUOPtr should now have a problem set
  EXPECT_TRUE(moabUOPtr->hasProblem());

}
