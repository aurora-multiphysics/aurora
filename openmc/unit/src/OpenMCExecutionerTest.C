//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BasicTest.h"
#include "Executioner.h"
#include "FEProblemBase.h"
#include "MoabUserObject.h"
#include "OpenMCExecutioner.h"

// Fixture to test the MOAB user object
class OpenMCExecutionerTest : public InputFileTest{
protected:
  OpenMCExecutionerTest() :
    InputFileTest("executioner.i"),
    isSetUp(false),
    var_name("heating-local")
  {
    openmcInputFiles.push_back("settings.xml");
    openmcInputFiles.push_back("materials.xml");
    openmcInputFiles.push_back("tallies.xml");
    openmcInputFiles.push_back("dagmc.h5m");

    scalefactor =16.0218;
  };

  virtual void SetUp() override {

    // Call the base class method
    InputFileTest::SetUp();

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

      // Get the
      fetchInputXML();

      isSetUp = true;
    }
    catch(std::exception e){
      std::cout<<e.what()<<std::endl;
    }

  }

  void fetchInputXML(){

    for(const auto file : openmcInputFiles){
      std::string fileNow = "inputs/"+file;
      ASSERT_TRUE(fileExists(fileNow));
      // This is ugly: system() can result in undefined behaviour. Fix me.
      std::string command = "cp "+fileNow +" .";
      int retval = system(command.c_str());
      EXPECT_EQ(retval,0);
      ASSERT_TRUE(fileExists(file));
    }

  }

  virtual void TearDown() override {
    for(const auto file : openmcInputFiles){
      if(fileExists(file)){
        deleteFile(file);
      }
    }
  }

  FEProblemBase* problemPtr;
  Executioner* executionerPtr;
  MoabUserObject* moabUOPtr;
  bool isSetUp;
  std::vector<std::string> openmcInputFiles;
  std::string var_name;
  double scalefactor;
};

TEST_F(OpenMCExecutionerTest,execute){

  ASSERT_TRUE(isSetUp);

  EXPECT_FALSE(moabUOPtr->hasProblem());

  // Run execute
  EXPECT_NO_THROW(executionerPtr->execute());

  // Check if moabUOPtr now has an FEProblem set
  EXPECT_TRUE(moabUOPtr->hasProblem());

  // Check moose and openmc moabPtrs are the same
  auto moabIt = openmc::model::moabPtrs.find(1);
  ASSERT_NE(moabIt,openmc::model::moabPtrs.end());
  EXPECT_EQ(openmc::model::moabPtrs[1], moabUOPtr->moabPtr);

  // Get the mesh and number of elems
  MeshBase& mesh = problemPtr->mesh().getMesh();
  size_t nMeshBins= mesh.n_elem();
  EXPECT_EQ(nMeshBins,11972);

  // Fetch the system details
  System & sys = problemPtr->getSystem(var_name);
  unsigned int iSys = sys.number();
  unsigned int iVar = sys.variable_number(var_name);
  // Get the size of solution vector
  numeric_index_type 	solsize = sys.solution->size();

  // Get OpenMC results and check size
  size_t nScores=1;
  ASSERT_FALSE(openmc::model::tallies.empty());
  openmc::Tally& tally = *(openmc::model::tallies.at(0));
  xt::xtensor<double, 3> & results = tally.results_;
  ASSERT_EQ(results.shape()[0],nMeshBins);
  ASSERT_EQ(results.shape()[1],nScores);
  ASSERT_EQ(results.shape()[2],3);

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
    double solCompare = results(iBin,0,1);
    // Normalise to volume
    solCompare /= elem.volume();
    solCompare *= scalefactor;

    double solDiff = fabs(sol-solCompare);
    EXPECT_LT(solDiff,tol);

  }

}
