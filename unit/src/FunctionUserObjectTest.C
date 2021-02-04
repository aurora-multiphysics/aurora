//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "FunctionUserObjectTest.h"

TEST_F(FunctionUserObjectTest, checkSolution)
{
  // Check setup was successful
  EXPECT_NE(functionUOPtr, nullptr);

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

  // Now loop over the centroids and check function at each point
  size_t nPoints = centroids.size();
  for(size_t iPoint=0; iPoint<nPoints; iPoint++){
    Point p = centroids.at(iPoint);
    double result = results.at(iPoint);
    // Evaluate the function at p
    double function_result(0.);
    EXPECT_NO_THROW(evalFunction(p,function_result));
    // Check
    double diff = fabs(function_result-result);
    EXPECT_LE(diff,tol);

    // Now check the nodes of the elem whose centroid this is
    // to test edge cases
    const Elem & elem=mesh.elem_ref(iPoint);
    unsigned int nNodes = elem.n_nodes();
    for(unsigned int iNode=0; iNode<nNodes; iNode++){
      Point pNode = elem.point(iNode);
      double node_result(0.);
      EXPECT_NO_THROW(evalFunction(pNode,node_result));
      double nodediff = fabs(node_result-result);
      EXPECT_LE(nodediff,tol) << "Result differs for node "<< iNode;
    }
  }

  // Now check a point outside the domain
  Point pOutside(100.,100.,100.);
  double dummy;
  EXPECT_THROW(evalFunction(pOutside,dummy),std::logic_error);

}
