//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "gtest/gtest.h"
#include <memory>

#include "AuroraAppTest.h"
#include "Executioner.h"
#include "DisplacedProblem.h"
#include "FunctionUserObject.h"

#include "openmc/position.h"
#include "openmc/mesh.h"
#include "openmc/tallies/tally.h"


TEST_F(AuroraAppBasicTest, registryTest)
{

  // Check we can find heat conduction
  bool foundHeatConduction = Registry::isRegisteredObj("ADHeatConduction");
  EXPECT_TRUE(foundHeatConduction);


  // Check we can find the objects we need
  std::vector<std::string> knownObjNames;
  knownObjNames.push_back("FunctionUserObject");
  knownObjNames.push_back("MoabMeshTransfer");
  knownObjNames.push_back("VariableFunction");
  checkKnownObjects(knownObjNames);

  knownObjNames.clear();
  knownObjNames.push_back("OpenMCProblem");
  knownObjNames.push_back("OpenMCExecutioner");
  knownObjNames.push_back("MoabUserObject");
  knownObjNames.push_back("OpenMCDensity");
  knownObjNames.push_back("ADOpenMCDensity");
  checkKnownObjects(knownObjNames,"OpenMCApp");

}

class MinimalInputTest : public AuroraAppInputTest{
protected:
  MinimalInputTest() : AuroraAppInputTest("minimal.i") {};
};

TEST_F(MinimalInputTest, readInput)
{
  ASSERT_FALSE(appIsNull);

  ASSERT_NO_THROW(app->setupOptions());
  ASSERT_NO_THROW(app->runInputFile());

}

class FullRunTest : public AuroraAppRunTest{
protected:
  FullRunTest() : AuroraAppRunTest("aurora.i") {};

  void checkFullRun(std::string dagfile){

    // Get the current dagmc file
    fetchInputFile(dagfile,dagmcFilename);

    ASSERT_NO_THROW(app->run());

    // Get the executioner
    Executioner* executionerPtr = app->getExecutioner();
    ASSERT_NE(executionerPtr,nullptr);

    EXPECT_TRUE(executionerPtr->lastSolveConverged());
  }

};

class CubeMeshTest : public AuroraAppRunTest{
protected:
  CubeMeshTest(std::string inputfile) : AuroraAppRunTest(inputfile) {

    // Override source openmc input names
    openmcInputXMLFilesSrc.at(0) = "settings-deformed.xml";

  };

  virtual void SetUp() override {

    // Base class setup.
    AuroraAppRunTest::SetUp();
    ASSERT_FALSE(appIsNull);

    // Get the current dagmc file
    fetchInputFile("dagmc_cube.h5m",dagmcFilename);

    // Should run without error
    ASSERT_NO_THROW(app->run());

    // Get the FE problem and mesh
    FEProblemBase& problem = app->getExecutioner()->feProblem();
    if(problem.haveDisplaced()){
      meshPtr = &(problem.getDisplacedProblem()->mesh().getMesh());
    }
    ASSERT_NE(meshPtr,nullptr);

    // Get the function user object
    functionUOPtr = &(problem.getUserObject<FunctionUserObject>("uo-heating-function"));

  }

  MeshBase* meshPtr;
  FunctionUserObject* functionUOPtr;

};

class DeformedMeshTest : public CubeMeshTest{
protected:
  DeformedMeshTest() : CubeMeshTest("aurora-thermal-expansion.i") {};
};

class RefinedMeshTest : public CubeMeshTest{
protected:
  RefinedMeshTest() : CubeMeshTest("aurora-refine.i") {};
};



TEST_F(FullRunTest, UWUW)
{
  ASSERT_FALSE(appIsNull);

  std::string dagFile = "dagmc_uwuw.h5m";
  checkFullRun(dagFile);
}

TEST_F(FullRunTest, Legacy)
{
  ASSERT_FALSE(appIsNull);

  std::string dagFile = "dagmc_legacy.h5m";
  checkFullRun(dagFile);
}

// This test locates a point on the deformed mesh that is outside the bounds of
// the original mesh and checks that openmc can locate it, and get tally value
TEST_F(DeformedMeshTest, CheckExtremal)
{

  // Create a point to having maximal x,y,z
  Point extr(0.,0.,0.);

  // Loop over elems: find max point
  auto itelem = meshPtr->elements_begin();
  auto endelem = meshPtr->elements_end();
  for( ; itelem!=endelem; ++itelem){
    // Get a reference to current elem
    Elem& elem = **itelem;
    // Loop over elem's nodes
    size_t nNodes = elem.n_nodes();
    for(size_t iNode=0; iNode<nNodes; iNode++){
      const Point pointNow = elem.point(iNode);
      // Save point if this is the max in x and y,,z > 0
      double xcoord = pointNow(0);
      double ycoord = pointNow(1);
      double zcoord = pointNow(2);
      if(xcoord > extr(0) && ycoord > 1.0 && zcoord > 1.0){
        extr = pointNow;
      }
    }
  }

  // Having found extr point subtract epsilon to ensure fully inside element
  Point epsilon (5e-4,5e-4,5e-4);
  extr -= epsilon;

  // Convert to Openmc position
  openmc::Position pt(extr(0),extr(1),extr(2));

  // Get OpenMC Mesh
  openmc::Mesh* omcMeshPtr = openmc::model::meshes.back().get();

  // Find bin of extr pt
  int iBin = omcMeshPtr->get_bin(pt);

  // Upcast mesh ptr as unstructed
  openmc::UnstructuredMesh* unstrMeshPtr = dynamic_cast<openmc::UnstructuredMesh*>(omcMeshPtr);
  // Get the volume of the bin
  double volume= unstrMeshPtr->volume(iBin);

  // Get the openmc tally result for this bin
  ASSERT_FALSE(openmc::model::tallies.empty());
  openmc::Tally& tally = *(openmc::model::tallies.at(0));
  xt::xtensor<double, 3> & results = tally.results_;
  int iScore = 0;
  int nBatches = tally.n_realizations_;
  double omc_val = results(iBin,iScore,1)/double(nBatches)/volume;

  // Now compare this result to that obtained from a function
  double function_val = functionUOPtr->value(extr);
  EXPECT_LT(fabs(function_val-omc_val),tol);

}


TEST_F(RefinedMeshTest, CheckBins)
{
}
