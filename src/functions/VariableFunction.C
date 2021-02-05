// Moose includes
#include "VariableFunction.h"

registerMooseObject("AuroraApp", VariableFunction);

defineLegacyParams(VariableFunction);

InputParameters
VariableFunction::validParams()
{
  // Get the Function input parameters
  InputParameters params = Function::validParams();


  // Add required parameters
  params.addRequiredParam<UserObjectName>("uoname",
                                          "Name of the FunctionUserObject name to retrieve the mesh function from.");

  return params;
}

VariableFunction::VariableFunction(const InputParameters & parameters) :
  Function(parameters),
  userObjName("uoname")
{}

void
VariableFunction::initialSetup()
{
  meshFunction = &getUserObject<FunctionUserObject>(userObjName);
}

Real
VariableFunction::value(Real /*t*/, const Point & p) const
{
  Real value(0.);
  {
    // If multi-threaded, execute one thread at a time because
    // meshFunction references the same UserObject
    // so need to avoid a data race
    // TODO: make this better
#ifdef LIBMESH_HAVE_OPENMP
#pragma omp critical
#endif
    if(meshFunction!=nullptr){
      value = Real(meshFunction->value(p));
    }
  }
  return value;
}
