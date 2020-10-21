# pragma once

#include "ExternalProblem.h"

class OpenMCProblem;

template <>
InputParameters validParams<OpenMCProblem>();

class OpenMCProblem : public ExternalProblem
{
 public:
  OpenMCProblem (const InputParameters & parameters) :
  ExternalProblem(parameters)  {};
  
  virtual void externalSolve () override {};  
  virtual bool converged () override { return true; };  
  virtual void syncSolutions(Direction /*direction*/) override {};;

};
