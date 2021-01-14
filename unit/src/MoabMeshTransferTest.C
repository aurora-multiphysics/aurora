//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "AuroraAppTest.h"
#include "Executioner.h"
#include "FEProblemBase.h"
#include "MoabMeshTransfer.h"

// Fixture to test the MOAB mesh transfer
class MoabMeshTransferTest : public AuroraAppInputTest{
protected:
  MoabMeshTransferTest() : AuroraAppInputTest("transfer.i") {};

  virtual void SetUp() override {

    // Call the base class method
    AuroraAppInputTest::SetUp();

    ASSERT_FALSE(appIsNull);

    ASSERT_NO_THROW(app->setupOptions());
    ASSERT_NO_THROW(app->runInputFile());

    // Get the FE problem
    problemPtr = &(app->getExecutioner()->feProblem());

    ASSERT_NE(problemPtr,nullptr);

    std::vector< std::shared_ptr< Transfer > > transfers_from =
      problemPtr->getTransfers(Transfer::DIRECTION::FROM_MULTIAPP);
    EXPECT_EQ(transfers_from.size(),0);

    std::vector< std::shared_ptr< Transfer > > transfers_to =
      problemPtr->getTransfers(Transfer::DIRECTION::TO_MULTIAPP);
    ASSERT_EQ(transfers_to.size(),1);

    // Upcast
    mbMeshTransferPtr =
      std::dynamic_pointer_cast<MoabMeshTransfer>(transfers_to.back());

  }

  FEProblemBase* problemPtr;
  std::shared_ptr< MoabMeshTransfer > mbMeshTransferPtr;

};

TEST_F(MoabMeshTransferTest, transfer)
{

  EXPECT_NE(mbMeshTransferPtr, nullptr);

}
