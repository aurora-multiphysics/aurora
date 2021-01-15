//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "FunctionUserObjectTest.h"
#include "Function.h"

class VariableFunctionTest : public FunctionUserObjectTest {

protected:
  VariableFunctionTest() :
    FunctionUserObjectTest("variable_function.i"),
    function_name("heating-function")
  {};

  virtual void SetUp() override {

    // Call the base class method
    ASSERT_NO_THROW(FunctionUserObjectTest::SetUp());
    ASSERT_NE(functionUOPtr,nullptr);

    // Get function object
    ASSERT_NE(problemPtr,nullptr);
    ASSERT_TRUE(problemPtr->hasFunction(function_name));
    functionPtr = &(problemPtr->getFunction(function_name));

  }

  void evalFunction(Real t, Point& p, double& function_result){
    function_result = functionPtr->value(t,p);
  }

  Function* functionPtr;
  std::string function_name;

};

TEST_F(VariableFunctionTest, checkSolution)
{
  // Check setup was successful
  EXPECT_NE(functionPtr, nullptr);

  // Initialise mesh data in MOAB
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Manufacture a solution
  std::vector<double> results;
  std::vector<Point> centroids;
  EXPECT_NO_THROW(getSolutionData(results,centroids));
  ASSERT_EQ(results.size(),centroids.size());

  // Set the solution
  ASSERT_TRUE(moabUOPtr->setSolution(var_name,results,1.0,false));

  // Get the mesh
  MeshBase& mesh = problemPtr->mesh().getMesh();

  // Choose some arbitrary timesteps (our function is time-independent)
  std::vector<Real> timesteps;
  timesteps.push_back(0.);
  timesteps.push_back(10.);
  timesteps.push_back(100.);

  // Perform test for each time
  for(const auto t : timesteps){
    // Now loop over the centroids and check function at each point
    size_t nPoints = centroids.size();
    for(size_t iPoint=0; iPoint<nPoints; iPoint++){
      Point p = centroids.at(iPoint);
      double result = results.at(iPoint);
      // Evaluate the function at p
      double function_result;
      EXPECT_NO_THROW(evalFunction(t,p,function_result));
      // Check
      double diff = fabs(function_result-result);
      EXPECT_LE(diff,tol);

      // Now check the nodes of the elem whose centroid this is
      // to test edge cases
      const Elem & elem=mesh.elem_ref(iPoint);
      unsigned int nNodes = elem.n_nodes();
      for(unsigned int iNode=0; iNode<nNodes; iNode++){
        Point pNode = elem.point(iNode);
        double node_result;
        EXPECT_NO_THROW(evalFunction(t,pNode,node_result));
        double nodediff = fabs(node_result-result);
        EXPECT_LE(diff,tol) << "Result differs for node "<< iNode;
      }
    }

    // Now check a point outside the domain
    Point pOutside(100.,100.,100.);
    double dummy;
    EXPECT_THROW(evalFunction(t,pOutside,dummy),std::logic_error);
  }

}
