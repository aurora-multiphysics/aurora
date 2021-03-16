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
#include "MoabUserObject.h"
#include "OpenMCExecutioner.h"

// Fixture to test the OpenMCExecutioner
class OpenMCExecutionerTest : public OpenMCAppRunTest{
protected:

  OpenMCExecutionerTest(std::string inputfile) :
    OpenMCAppRunTest(inputfile)
  {
    init();
  };

  OpenMCExecutionerTest() :
    OpenMCAppRunTest("executioner.i")
  {
    init();
  }

  void init()
  {
    setDefaults();
    setScoreList();
  };

  // Helper struct
  struct VarData{
    std::string var_name;
    std::string err_name;
    double scale;
    int iScore;
  };

  void setDefaults(){
    isSetUp=false;
    scalefactor=16.0218;
    nMeshElemsExpect=11972;
    nDegenBins=1;
    nScores=2;
    nBatches=2;
  }

  // Set all the cases we want to test
  virtual void setScoreList(){
    VarData var = {"heating-local","",scalefactor,0};
    scores.push_back(var);
  }

  virtual void SetUp() override {

    // Call the base class method
    OpenMCAppRunTest::SetUp();

    ASSERT_FALSE(appIsNull);

    try{
      app->setupOptions();
      app->runInputFile();

      // Get the executioner
      executionerPtr = app->getExecutioner();

      ASSERT_NE(executionerPtr,nullptr);

      // Get the FE problem
      problemPtr = &(executionerPtr->feProblem());

      // Check for MOAB user object
      if(!(problemPtr->hasUserObject("moab")))
        throw std::logic_error("Could not find MoabUserObject with name 'moab'. Please check your input file.");

      // Get the MOAB user object
      moabUOPtr = &(problemPtr->getUserObject<MoabUserObject>("moab"));

      isSetUp = true;
    }
    catch(std::exception& e){
      std::cout<<e.what()<<std::endl;
    }

  }


  void getSolExpect(int iScore,double scale,
                    std::vector<double>& solExpect,
                    std::vector<double>& errExpect)
  {
    solExpect.clear();

    // Get OpenMC results and check size
    ASSERT_FALSE(openmc::model::tallies.empty());
    openmc::Tally& tally = *(openmc::model::tallies.at(0));
    xt::xtensor<double, 3> & results = tally.results_;

    size_t nMeshBinsExpect=nMeshElemsExpect*nDegenBins;
    ASSERT_EQ(results.shape()[0],nMeshBinsExpect);
    ASSERT_EQ(results.shape()[1],nScores);
    ASSERT_EQ(results.shape()[2],3);
    ASSERT_LT(iScore,nScores);
    EXPECT_EQ(nBatches,tally.n_realizations_);

    // Get the mesh
    MeshBase& mesh = problemPtr->mesh().getMesh();

    // Loop over the elements
    // NB this only works because we don't have additional filters
    for(size_t iElem=0; iElem<nMeshElemsExpect; iElem++){

      // Because of how the elems are created iElem = elem id
      dof_id_type id = iElem;

      // Get a reference to the element with this ID
      Elem& elem  = mesh.elem_ref(id);

      double solNow = 0.;
      double errNow = 0.;

      // Sum over bins which map to the same element
      for(size_t iDegen=0; iDegen<nDegenBins; iDegen++){

        // Calculate bin index
        size_t iBin = nDegenBins*iElem + iDegen;

        // Get the result for the corresponding bin
        double mean = results(iBin,iScore,1)/double(nBatches);

        // Calculate variance of the mean for this bin
        double var = results(iBin,iScore,2)/double(nBatches);
        var -= mean*mean;
        var /= (nBatches-1);

        // The solution is the sum over degenerate bins
        solNow+= results(iBin,iScore,1)/double(nBatches);

        // Get the sum of variances
        errNow+= var;
      }

      // Error is sqrt of sum of variances
      errNow = sqrt(errNow);

      // Normalise
      solNow *= scale;
      solNow /= elem.volume();
      errNow *= scale;
      errNow /= elem.volume();

      // Save
      solExpect.push_back(solNow);
      errExpect.push_back(errNow);
    }

  }

  void checkSolution(std::string var_name_now, std::vector<double>& solExpect){

    // Get the mesh and number of elems
    MeshBase& mesh = problemPtr->mesh().getMesh();
    size_t nMeshBins= mesh.n_elem();
    EXPECT_EQ(nMeshBins,nMeshElemsExpect);

    ASSERT_TRUE(problemPtr->hasVariable(var_name_now));

    // Fetch the system details
    System & sys = problemPtr->getSystem(var_name_now);
    unsigned int iSys = sys.number();
    unsigned int iVar = sys.variable_number(var_name_now);
    // Get the size of solution vector
    numeric_index_type 	solsize = sys.solution->size();

    // Loop over the elements
    for(size_t iBin=0; iBin<nMeshBins; iBin++){

      // Because of how the elems are created iBin = elem id
      dof_id_type id = iBin;

      // Get a reference to the element with this ID
      Elem& elem  = mesh.elem_ref(id);

      // Get the degree of freedom number for this element
      dof_id_type soln_index = elem.dof_number(iSys,iVar,0);
      ASSERT_LT(soln_index,solsize);

      // Get the solution value for this element
      double sol = double(sys.solution->el(soln_index));

      // Get the solution for the corresponding bin
      double solCompare = solExpect.at(iBin);

      double solDiff = fabs(sol-solCompare);
      if(sol>tol) solDiff/= sol;
      EXPECT_LT(solDiff,tol)<< "solution = "<< sol
                            << " solCompare = "<< solCompare;
    } // End loop over elems

  }

  void checkSolution(VarData var){

    // Retrieve the expected solution
    std::vector<double> solExpect;
    std::vector<double> errExpect;
    getSolExpect(var.iScore,var.scale,solExpect,errExpect);
    EXPECT_EQ(solExpect.size(),nMeshElemsExpect);
    EXPECT_EQ(errExpect.size(),nMeshElemsExpect);

    // Check solution vector matches OpenMC results
    checkSolution(var.var_name,solExpect);

    // Check errors if requested
    if(var.err_name !=""){
      checkSolution(var.err_name,errExpect);
    }

  }


  void checkSolutions(){
    for(const auto& score : scores){
      checkSolution(score);
    }
  }

  void checkExecute(std::string dagfile){

    // Get the current dagmc file
    fetchInputFile(dagfile,dagmcFilename);

    // Run the test three times
    for(unsigned int i=0; i<3; i++){
      // Delete them for next time
      deleteAll(openmcOutputFiles);

      // Run execute
      ASSERT_NO_THROW(executionerPtr->execute());

      // Check if moabUOPtr now has an FEProblem set
      EXPECT_TRUE(moabUOPtr->hasProblem());

      // Check moose and openmc moabPtrs are the same
      auto moabIt = openmc::model::moabPtrs.find(1);
      ASSERT_NE(moabIt,openmc::model::moabPtrs.end());
      EXPECT_EQ(openmc::model::moabPtrs[1], moabUOPtr->moabPtr);

      // Check we produced some output
      std::vector<std::string> openmcOutputFilesCopy = openmcOutputFiles;
      // summary file only gets produced in first iteration,
      // so remove it from the list to check.
      if(i>0){
        auto it =find(openmcOutputFilesCopy.begin(),
                      openmcOutputFilesCopy.end(),
                      "summary.h5");
        ASSERT_NE(it,openmcOutputFilesCopy.end());
        openmcOutputFilesCopy.erase(it);
      }
      checkFilesExist(openmcOutputFilesCopy);

      // Check OpenMC results match up with MOOSE solutions
      checkSolutions();

    } // End iterations over test

  }

  size_t nMeshElemsExpect;
  size_t nDegenBins;
  size_t nScores;
  int nBatches;

  FEProblemBase* problemPtr;
  Executioner* executionerPtr;
  MoabUserObject* moabUOPtr;
  bool isSetUp;

  // List of scores to check
  std::vector<VarData> scores;

  double scalefactor;
};

// Fixture to test multiple scores
class ManyScoresExecutionerTest: public OpenMCExecutionerTest {
protected:

  ManyScoresExecutionerTest() :
    OpenMCExecutionerTest("executioner-many-scores.i")
  {};

  virtual void setScoreList() override{
    VarData var = {"heating-local","heating-local-errs",scalefactor,0};
    scores.push_back(var);
    var = {"flux","flux-errs",1.0,1};
    scores.push_back(var);
  }

};

// Fixture to test the OpenMCExecutioner with a second order mesh
class SecondOrderExecutionerTest: public OpenMCExecutionerTest {
protected:

  SecondOrderExecutionerTest() :
    OpenMCExecutionerTest("executioner-second.i")
  {
    // Second-order mesh has 8 sub-tets for a single tet10
    nDegenBins=8;
  }

  virtual void setScoreList() override{
    VarData var = {"heating-local","heating-local-errs",scalefactor,0};
    scores.push_back(var);
  }

};


TEST_F(OpenMCExecutionerTest,executeUWUW){

  ASSERT_TRUE(isSetUp);

  EXPECT_FALSE(moabUOPtr->hasProblem());

  std::string dagFile = "dagmc_uwuw.h5m";
  checkExecute(dagFile);

}

TEST_F(OpenMCExecutionerTest,executeLegacy){

  ASSERT_TRUE(isSetUp);

  EXPECT_FALSE(moabUOPtr->hasProblem());

  std::string dagFile = "dagmc_legacy.h5m";
  checkExecute(dagFile);

}

TEST_F(ManyScoresExecutionerTest,execute){

  ASSERT_TRUE(isSetUp);

  EXPECT_FALSE(moabUOPtr->hasProblem());

  std::string dagFile = "dagmc_legacy.h5m";
  checkExecute(dagFile);

}

TEST_F(SecondOrderExecutionerTest,executeUWUW){

  ASSERT_TRUE(isSetUp);

  EXPECT_FALSE(moabUOPtr->hasProblem());

  std::string dagFile = "dagmc_uwuw.h5m";
  checkExecute(dagFile);

}

TEST_F(SecondOrderExecutionerTest,executeLegacy){

  ASSERT_TRUE(isSetUp);

  EXPECT_FALSE(moabUOPtr->hasProblem());

  std::string dagFile = "dagmc_legacy.h5m";
  checkExecute(dagFile);

}
