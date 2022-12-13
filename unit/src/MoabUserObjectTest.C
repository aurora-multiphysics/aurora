#include "MoabUserObjectTest.h"

// Test the fixture set up
TEST_F(MoabUserObjectTest, setup)
{
  EXPECT_FALSE(appIsNull);
  EXPECT_TRUE(foundMOAB);
  EXPECT_FALSE(moabUOPtr->hasProblem());
}

// Init should fail if we don't set the FE problem
TEST_F(MoabUserObjectTest, nullproblem)
{
  ASSERT_TRUE(foundMOAB);

  // Init should throw because we didn't set the feproblem
  EXPECT_THROW(moabUOPtr->initMOAB(),std::runtime_error);

}

// Test for MOAB mesh initialisation
TEST_F(MoabUserObjectTest, init)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  initMoabTest();

}

// Test for setting FE problem solution
TEST_F(MoabUserObjectTest, setSolution)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Perform the test
  setSolutionTest();
}

// Test for setting FE problem solution
TEST_F(MoabUserObjectTest, setErrors)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Perform the test
  setErrorsTest();
}

// Test for setting FE problem solution
TEST_F(MoabUserObjectTest, zeroSolution)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get number of elems
  MeshBase& mesh = problemPtr->mesh().getMesh();
  unsigned int nElem = mesh.n_elem();

  // Set a solution that is everywhere 0.
  // Normally generates a warning, but in these unit tests
  // mooseWarning throws, which is handled safely and returns false
  std::vector<double> zeroSol(nElem,0.);
  EXPECT_FALSE(moabUOPtr->setSolution(var_name,zeroSol,1.0,false,false));

}

// Repeat for different lengthscale
TEST_F(MoabChangeUnitsTest, setSolution)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Perform the test
  setSolutionTest();
}

// Test for MOAB mesh reseting
TEST_F(MoabUserObjectTest, reset)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Clear the mesh
  ASSERT_NO_THROW(moabUOPtr->reset());

  // Get the MOAB interface
  std::shared_ptr<moab::Interface> moabPtr = moabUOPtr->moabPtr;

  // Check the (lack of) data

  // Get root set
  moab::EntityHandle rootset = moabPtr->get_root_set();

  // Check no entity handles of each dimensionality in turn
  // dim=4 -> entitiy set
  std::vector<moab::EntityHandle> ents;
  moab::ErrorCode rval;
  for(int dim=0; dim<5; dim++){
    ents.clear();
    rval = moabPtr->get_entities_by_dimension(rootset,dim,ents);
    EXPECT_EQ(rval,moab::MB_SUCCESS);
    EXPECT_EQ(ents.size(),size_t(0));
  }

}

// Test for second-order MOAB mesh initialisation
TEST_F(SecondOrderMoabUserObjectTest, init)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  initMoabTest();
}

// Test for second-order MOAB mesh solution
TEST_F(SecondOrderMoabUserObjectTest, setSolution)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());


  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Create a vector for setting "openmc" solution
  std::vector<double> solutionData;

  // Create a vector for libmesh solution
  std::vector<double> solutionCompareData;

  // Generate solution data (const across sub-tets)
  size_t elemDegen=8;
  size_t nElemsLibMesh = nElemsExpect/elemDegen;
  double solInitial=300.;
  double solDiff=1.;
  for(size_t iElem=0; iElem<nElemsLibMesh; iElem++){
    double solNow = solInitial * solDiff*iElem;
    solutionCompareData.push_back(solNow*elemDegen);
    for(size_t iDegen=0; iDegen<elemDegen; iDegen++){
      solutionData.push_back(solNow);
    }
  }

  EXPECT_TRUE(moabUOPtr->setSolution(var_name,
                                     solutionData,
                                     1.0,
                                     false,
                                     false));


  checkSolution(solutionCompareData);

}

// Test for second-order MOAB mesh error solution
TEST_F(SecondOrderMoabUserObjectTest, setErrors)
{
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());

  // Perform the test
  setErrorsTest(8);
}


// Test for finding surfaces
TEST_F(FindMoabSurfacesTest, constTemp)
{
  init();
  checkConstTempSurfs(300,3,4);
}

TEST_F(FindMoabSurfacesTest, singleBin)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());


  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Manufacture a solution that will map to a single temp bin
  // Bin edges are 297.5, 302.5
  double solMax = 302.;
  double solMin = 298.;
  // Absolute max radius
  double rAbsMax = getRMax();
  setSolution(ents,rAbsMax,solMax,solMin,1.0,false);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check groups, volumes and surfaces
  unsigned int nVol=3;
  unsigned int nSurf=4;
  checkAllGeomsets(nVol,nSurf);

  // Check volume->surf relationships
  VolumeInfo volinfo;

  // Copper block
  volinfo.vol_id=1;
  volinfo.surf_ids = {1};
  volinfo.isGraveyard=false;
  checkSurfs(volinfo);

  // Region of air
  volinfo.vol_id=2;
  volinfo.surf_ids = {1,2};
  checkSurfs(volinfo);

  // Graveyard
  volinfo.vol_id=3;
  volinfo.surf_ids = {3,4};
  volinfo.isGraveyard=true;
  checkSurfs(volinfo);

}

TEST_F(FindMoabSurfacesTest, extraBin)
{
  init();

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Manufacture a solution that results in one extra surface
  // Bin edges are 297.5, 302.5
  double solMax = 307.;
  double solMin = 298.;
  double rMax = 2.0*lengthscale/log(2.0);
  setSolution(ents,rMax,solMax,solMin,1.0,false);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check groups, volumes and surfaces
  unsigned int nVol=4;
  unsigned int nSurf=5;
  checkAllGeomsets(nVol,nSurf);

  // Check volume->surf relationships
  VolumeInfo volinfo;

  // Copper block
  volinfo.vol_id=1;
  volinfo.surf_ids = {1};
  volinfo.isGraveyard=false;
  checkSurfs(volinfo);

  // Region of air at cooler T
  volinfo.vol_id=2;
  volinfo.surf_ids = {1,2,3};
  checkSurfs(volinfo);

  // Region of air at hotter T
  volinfo.vol_id=3;
  volinfo.surf_ids = {3};
  checkSurfs(volinfo);

  // Graveyard
  volinfo.vol_id=4;
  volinfo.surf_ids = {4,5};
  volinfo.isGraveyard=true;
  checkSurfs(volinfo);

  // Change boundary of temperature contour to intersect material boundary
  rMax = 4.0*lengthscale/log(2.0);
  setSolution(ents,rMax,solMax,solMin,1.0,false);

  // Find the surfaces (and also test reset)
  EXPECT_TRUE(moabUOPtr->update());

  // Check groups, volumes and surfaces
  nVol=5;
  nSurf=9;
  checkAllGeomsets(nVol,nSurf);

  // Check volume->surf relationships

  // Main block of copper at lower T
  volinfo.vol_id=1;
  volinfo.surf_ids = {1,3,5};
  volinfo.isGraveyard=false;
  checkSurfs(volinfo);

  // Block of copper at higher T
  volinfo.vol_id=2;
  volinfo.surf_ids={2,3,6};
  checkSurfs(volinfo);

  // Region of air at lower T
  volinfo.vol_id=3;
  volinfo.surf_ids ={4,5,6,7};
  checkSurfs(volinfo);

  // Region of air at higher T
  volinfo.vol_id=4;
  volinfo.surf_ids ={1,2,7};
  checkSurfs(volinfo);

  // Graveyard
  volinfo.vol_id=5;
  volinfo.surf_ids ={8,9};
  volinfo.isGraveyard=true;
  checkSurfs(volinfo);

}

// Test for checking output
TEST_F(FindMoabSurfacesTest, checkOutput)
{
  init();

  // Set a constant solution
  double solConst = 300.;
  setConstSolution(nElemsExpect,solConst,var_name);

  // How many times to update
  unsigned int nUpdate=15;
  checkOutputAfterUpdate(nUpdate);
}

// Test for retrieving metadata
TEST_F(FindMoabSurfacesTest, checkMetadata)
{
  init();
  matMetadataTest();
}

// Test for finding surfaces for second order mesh
TEST_F(SecondOrderSurfacesTest, constTemp)
{
  init();
  checkConstTempSurfs(37.5,3,4,8);
}

// Test for checking output
TEST_F(FindSurfsNodalTemp, nodalTemperature)
{
  ASSERT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());

  // Fix due to a warning which throws
  Moose::_throw_on_error = false;

  // Find the surfaces
  ASSERT_TRUE(moabUOPtr->update());

  Moose::_throw_on_error = true;

  // Check groups, volumes and surfaces
  unsigned int nVol=4;
  unsigned int nSurf=5;
  checkAllGeomsets(nVol,nSurf);

  // Check volume->surf relationships
  VolumeInfo volinfo;

  // Copper block
  volinfo.vol_id=1;
  volinfo.surf_ids = {1};
  volinfo.isGraveyard=false;
  checkSurfs(volinfo);

  // Region of air at cooler T
  volinfo.vol_id=2;
  volinfo.surf_ids = {1,2,3};
  checkSurfs(volinfo);

  // Region of air at hotter T
  volinfo.vol_id=3;
  volinfo.surf_ids = {3};
  checkSurfs(volinfo);

  // Graveyard
  volinfo.vol_id=4;
  volinfo.surf_ids = {4,5};
  volinfo.isGraveyard=true;
  checkSurfs(volinfo);

}

// Check that a non-tet mesh fails
TEST_F(BadMoabUserObjectTest, failinit)
{
  // Check setup
  ASSERT_TRUE(foundMOAB);
  ASSERT_FALSE(moabUOPtr->hasProblem());

  // Set the problem
  ASSERT_TRUE(setProblem());

  // Init should throw because we didn't pass in a tet mesh
  EXPECT_THROW(moabUOPtr->initMOAB(),std::runtime_error);
}

// Test for finding surfaces for single material
TEST_F(FindSingleMatSurfs, constTemp)
{
  init();
  checkConstTempSurfs(340,2,3);
}

// Test for finding surfaces for many temperature bins
TEST_F(FindSingleMatSurfs, manyBins)
{
  init();

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Manufacture a solution that results in many nested surfaces
  double solMax = 420;
  double solMin = 340;
  double rMax = 9.0/log(8)*lengthscale;
  setSolution(ents,rMax,solMax,solMin,1.0,false);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check groups, volumes and surfaces
  unsigned int nVol=6;
  unsigned int nSurf=7;
  checkAllGeomsets(nVol,nSurf);

  // Check volume->surf relationships
  VolumeInfo volinfo;
  volinfo.isGraveyard=false;

  // Surfaces from outside to inside
  std::vector<int> surfaces = {1,3,2,4,5};
  // Width of temperature bins
  double binWidth=20.0;
  for(unsigned int iVol=1; iVol<nVol; iVol++){
    volinfo.vol_id=iVol;
    volinfo.surf_ids.clear();
    int outer = surfaces.at(iVol-1);
    volinfo.surf_ids.insert(outer);
    if(iVol<nVol-1){
      int inner = surfaces.at(iVol);;
      volinfo.surf_ids.insert(inner);
    }
    checkSurfs(volinfo);
  }

  // Graveyard
  volinfo.vol_id=nVol;
  volinfo.surf_ids = {6,7};
  volinfo.isGraveyard=true;
  checkSurfs(volinfo);

}

// Test for checking output for different output params
TEST_F(FindSingleMatSurfs, checkOutput)
{
  init();

  // Set a constant solution
  double solConst = 340.;
  setConstSolution(nElemsExpect,solConst,var_name);

  // How many times to update
  unsigned int nUpdate=15;
  checkOutputAfterUpdate(nUpdate);

}

// Test for correct initialisation of offset box
TEST_F(FindOffsetSurfs, testInit)
{
  EXPECT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(setProblem());
  ASSERT_NO_THROW(moabUOPtr->initBinningData());

  initMoabTest();
}

// Test for finding surfaces for an offset box
TEST_F(FindOffsetSurfs, update)
{
  init();
  checkConstTempSurfs(300,2,3);
}

// Test for finding surfaces for log binning
TEST_F(FindLogBinSurfs, constTemp)
{
  init();

  // Set a constant solution
  double solConst = 300.;
  setConstSolution(nElemsExpect,solConst,var_name);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  VolumeInfo volinfo;
  volinfo.isGraveyard=false;

  // Check temperature
  volinfo.vol_id=1;
  volinfo. surf_ids = {1};
  checkSurfs(volinfo);

  // Set a constant solution
  solConst = 400.;
  setConstSolution(nElemsExpect,solConst,var_name);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());
  checkSurfs(volinfo);

  // Set a constant solution
  solConst = 2000.;
  setConstSolution(nElemsExpect,solConst,var_name);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());
  checkSurfs(volinfo);

  // Set a constant solution
  solConst = 5000.;
  setConstSolution(nElemsExpect,solConst,var_name);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());
  checkSurfs(volinfo);

}

// Test for retrieving metadata
TEST_F(FindLogBinSurfs, checkMetadata)
{
  init();
  matMetadataTest();
}

// Test for finding surfaces for many temperature bins on log scale
TEST_F(FindLogBinSurfs, manyBins)
{
  init();

  // Get elems
  std::vector<moab::EntityHandle> ents;
  getElems(ents);

  // Manufacture a solution that results in many nested surfaces
  double solMax = 6000;
  double solMin = 150;
  double rMax = 2.5*lengthscale;
  setSolution(ents,rMax,solMax,solMin,1.0,false);

  // Find the surfaces
  EXPECT_TRUE(moabUOPtr->update());

  // Check groups, volumes and surfaces
  unsigned int nVol=5;
  unsigned int nSurf=6;
  checkAllGeomsets(nVol,nSurf);

  // Check volume->surf relationships
  VolumeInfo volinfo;
  volinfo.isGraveyard=false;

  // Surfaces from outside to inside
  std::vector<int> surfaces = {1,3,2,4};
  // Parameters to compare the temperature
  double tnow = pow(10,2.25);
  double proddiff = pow(10,0.5);
  for(unsigned int iVol=1; iVol<nVol; iVol++){
    volinfo.vol_id=iVol;
    volinfo.surf_ids.clear();
    int outer = surfaces.at(iVol-1);
    volinfo.surf_ids.insert(outer);
    if(iVol<nVol-1){
      int inner = surfaces.at(iVol);;
      volinfo.surf_ids.insert(inner);
    }
    checkSurfs(volinfo);
    // Update temperature for next iteration;
    tnow *= proddiff;
  }

  // Graveyard
  volinfo.vol_id=nVol;
  volinfo.surf_ids = {5,6};
  volinfo.isGraveyard=true;
  checkSurfs(volinfo);

}

TEST_F(FindDensitySurfsTest,constDensity)
{
  init();
  setConstSolution(nElemsExpect,8.920,density_name);
  checkConstTempSurfs(300.,2,3);
}

// Test linearly varying density
TEST_F(FindDensitySurfsTest,linearDensity)
{
  init();

  double temp = 300.;
  double denOrig = 8.920;
  double relDiff = 0.04;
  unsigned int nVol = 4;
  unsigned int nSurf = 7;
  linearDensityTest(denOrig,relDiff,temp,nVol,nSurf);

  // Repeat test for more bins
  relDiff = 0.08;
  nVol = 6;
  nSurf = 11;
  linearDensityTest(denOrig,relDiff,temp,nVol,nSurf);

  // Repeat for out of bounds density
  relDiff = 0.12;
  double denMin = denOrig*(1.0-relDiff);
  double denMax = denOrig*(1.0+relDiff);
  setLinearSolution(density_name,denMin,denMax,0);
  EXPECT_THROW(moabUOPtr->update(),std::runtime_error);
}

// Test linearly varying density and temp
TEST_F(FindDensitySurfsTest,linearDensityTemp)
{
  init();

  // Varying temp across single bin
  double denOrig = 8.920;
  double relDiff = 0.04;
  double tempMin = 300.;
  double tempMax = 302.;
  unsigned int nVol = 4;
  unsigned int nSurf = 7;
  linearDensityTempTest(denOrig,relDiff,tempMin,tempMax,nVol,nSurf);

  // Varying temp across two bins
  tempMin = 301.;
  tempMax = 303.;
  nVol = 7;
  nSurf = 19;
  linearDensityTempTest(denOrig,relDiff,tempMin,tempMax,nVol,nSurf);
}

// Test for retrieving density metadata
TEST_F(FindDensitySurfsTest, checkMetadata)
{
  init();
  matMetadataTest();
}


// Test for retrieving density metadata
TEST_F(FindDensitySurfsUnitTest, checkMetadata)
{
  init();
  matMetadataTest();
}

// Test to check we are using the deformed mesh if there is one
TEST_F(MoabDeformedMeshTest, checkDeformedMesh)
{
  ASSERT_FALSE(appIsNull);
  ASSERT_TRUE(foundMOAB);
  ASSERT_TRUE(foundPP);
  ASSERT_TRUE(setProblem());

  // Set the mesh
  ASSERT_NO_THROW(moabUOPtr->initMOAB());

  // Get the MOAB interface to check the data
  std::shared_ptr<moab::Interface> moabPtr = moabUOPtr->moabPtr;
  ASSERT_NE(moabPtr,nullptr);

  // Get root set
  moab::EntityHandle rootset = moabPtr->get_root_set();

  // Fetch the elements
  std::vector<moab::EntityHandle> ents;
  moab::ErrorCode rval = moabPtr->get_entities_by_type(rootset,moab::MBTET,ents);
  ASSERT_EQ(rval,moab::MB_SUCCESS);

  // Sanity check
  EXPECT_EQ(ents.size(),size_t(3000));

  // Calculate the volume of all the elements
  double meshVolTest = getTotalVolume(moabPtr,ents);

  // Compare manually calculated volume to the postprocessor values.
  double meshVolOrig = processMeshVol->getValue();
  double meshVolDef = processDeformedMeshVol->getValue();

  // Check the original volume is 5x5x5 cube
  EXPECT_LT(fabs(meshVolOrig-125.0),tol);

  // Check the deformed mesh volume is bigger from thermal expansion
  EXPECT_GT(meshVolDef,meshVolOrig);

  // Check MOAB volume equals deformed mesh volume
  EXPECT_LT(fabs(meshVolDef-meshVolTest),tol);

  // Check the difference between deformed and original is above our tolerance
  EXPECT_GT(meshVolTest-meshVolOrig,tol);

}
