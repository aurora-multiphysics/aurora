#pragma once

// MOOSE includes
#include "Function.h"
#include "FunctionUserObject.h"

class VariableFunction;

/**
   \brief Function object which returns the value of a variable at a point.

   This class wraps a FunctionUserObject.
*/
class VariableFunction: public Function
{
public:

  static InputParameters validParams();

  VariableFunction(const InputParameters & parameters);

  ~VariableFunction(){};

  /// Just retrieve the named FunctionUserObject
  void initialSetup() override;

  /** \brief Return the value of a variable at a point.

      Time argument is not used, it only exists as this overrides
      virtual function called in MOOSE.

      Mostly calls getValue, with some additional controls for thread safety.
   */
  Real value(Real t, const Point & p) const override;

  /// Wrapper for FunctionUserObject::value
  bool getValue(const Point & p, Real& value, std::string& errmsg) const;

private:

  /// Pointer to the FunctionUserObject
  const FunctionUserObject * meshFunction;

  /// Name of FunctionUserObject to retrieve
  std::string userObjName;

};
