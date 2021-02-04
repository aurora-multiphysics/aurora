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
  if(meshFunction!=nullptr){
    return Real(meshFunction->value(p));
  }
  else return 0.;  
}

