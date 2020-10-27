#pragma once

// MOOSE includes
#include "Function.h"
#include "MeshFunctionUserObject.h"

class VariableFunction;

template <>
InputParameters validParams<VariableFunction>();

class VariableFunction: public Function
{
public:
  
  static InputParameters validParams();
  
  VariableFunction(const InputParameters & parameters);
  
  ~VariableFunction(){};
  
  void initialSetup() override;

  Real value(Real t, const Point & p) const override;
  
private:

  // Pointer to the mesh function user object
  const MeshFunctionUserObject * meshFunction;

  // Name of meshFunctionUserObject to retrieve
  std::string userObjName;
  
};
