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
    var_name("heating-local"),
    sidelength(20.),
    sideoffset(10.),
    T0(300.),
    Tdiff(20.)
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

  void getSolutionData(std::vector<double>& results,
                       std::vector<Point>& centroids){

    // Get the mesh
    MeshBase& mesh = problemPtr->mesh().getMesh();

    // Loop over the elements
    auto itelem = mesh.elements_begin();
    auto endelem = mesh.elements_end();
    for( ; itelem!=endelem; ++itelem){
      Elem& elem = **itelem;
      // Get the centroid for elem
      Point centroid = getCentroid(elem);
      // Manufacture a position-dependent solution
      double result = calcSolution(centroid);
      // Store
      centroids.push_back(centroid);
      results.push_back(result);
    }
  }

  Point getCentroid(Elem &elem){
    Point centroid(0.,0.,0.);
    unsigned int nNodes = elem.n_nodes();
    for(unsigned int iNode=0; iNode<nNodes; ++iNode){
      // Get the point coords for this node
      const Point& point = elem.point(iNode);
      centroid += point;
    }
    centroid /= double(nNodes);
    return centroid;
  }

  double calcSolution(Point p){

    double sol=T0;
    for(unsigned int i=0; i<3; i++){
      double relpos = (p(i) + sideoffset)/sidelength;
      EXPECT_GT(relpos,0.0);
      EXPECT_LT(relpos,1.0);
      sol += relpos*Tdiff;
    }
    EXPECT_GT(sol,T0-tol);
    EXPECT_LT(sol,T0+3.0*Tdiff+tol);

    return sol;
  }

  void evalFunction(Point& p, double& function_result){
    function_result = functionUOPtr->value(p);
  }

  FunctionUserObject* functionUOPtr;
  std::string var_name;
  double sidelength;
  double sideoffset;
  double T0;
  double Tdiff;

};

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
    double function_result;
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
      double node_result;
      EXPECT_NO_THROW(evalFunction(pNode,node_result));
      double nodediff = fabs(node_result-result);
      EXPECT_LE(diff,tol) << "Result differs for node "<< iNode;
    }
  }

  // Now check a point outside the domain
  Point pOutside(100.,100.,100.);
  double dummy;
  EXPECT_THROW(evalFunction(pOutside,dummy),std::logic_error);

}
