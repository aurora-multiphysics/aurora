#include "OpenMCAppTest.h"
#include "Executioner.h"
#include "FEProblemBase.h"
#include "OpenMCDensity.h"
#include "ADOpenMCDensity.h"


class OpenMCDensityTestBase : public OpenMCAppInputTest {
protected:

  OpenMCDensityTestBase(std::string input, std::string mat_name_in,
                        Real density) :
    OpenMCAppInputTest(input),
    mat_name(mat_name_in),
    origDensityCompare(density) {};

  virtual void SetUp() override {

    // Create app
    OpenMCAppInputTest::SetUp();

    if(appIsNull) return;

    try{

      ASSERT_NO_THROW(app->run());

      // Get the FE problem
      problemPtr = &(app->getExecutioner()->feProblem());

      Moose::_throw_on_error = false;

      // Look for the material
      matPtr = problemPtr->getMaterial(mat_name, Moose::BLOCK_MATERIAL_DATA);

      Moose::_throw_on_error = true;

      if(matPtr != nullptr){
        foundMat = true;
      }

     }
    catch(std::exception& e){
      std::cout<<e.what()<<std::endl;
    }
  }

  FEProblemBase* problemPtr;
  std::shared_ptr<MaterialBase> matPtr;
  std::string mat_name;
  bool foundMat;
  Real origDensityCompare;
};

class OpenMCDensityTest : public OpenMCDensityTestBase {
protected:
  OpenMCDensityTest():
    OpenMCDensityTestBase("density.i", "test_density",8.0)
  {};

};

class ADOpenMCDensityTest : public OpenMCDensityTestBase {
protected:
  ADOpenMCDensityTest():
    OpenMCDensityTestBase("addensity.i", "test_density",8.0)
  {};

};

// Test that we can retrieve the original input density for Density mat
TEST_F(OpenMCDensityTest, orig_density)
{
  ASSERT_TRUE(foundMat);

  // Upcast pointer to correct type
  auto densityPtr =  std::dynamic_pointer_cast<OpenMCDensity>(matPtr);

  ASSERT_NE(densityPtr,nullptr);

  Real testDensity = densityPtr->origDensity();

  ASSERT_LE(fabs(testDensity-origDensityCompare),tol);
}


// Test that we can retrieve the original input density for ADDensity mat
TEST_F(ADOpenMCDensityTest, orig_density)
{
  ASSERT_TRUE(foundMat);

  // Upcast pointer to correct type
  auto densityPtr =  std::dynamic_pointer_cast<ADOpenMCDensity>(matPtr);

  ASSERT_NE(densityPtr,nullptr);

  Real testDensity = densityPtr->origDensity();

  ASSERT_LE(fabs(testDensity-origDensityCompare),tol);
}
