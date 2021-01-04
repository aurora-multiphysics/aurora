# pragma once

#include "ExternalProblem.h"

class OpenMCProblem;

template <>
InputParameters validParams<OpenMCProblem>();

// Minimal definition for a FE problem.
// Only included as it is required by Moose
// and our constructro avoids unneccessary initialisation.
// Our executioner does not actually call these methods
//  - it will not successfully run for a normal moose executioner.
class OpenMCProblem : public ExternalProblem
{
 public:
  OpenMCProblem (const InputParameters & parameters) :
  ExternalProblem(parameters)  {};
  
  virtual void externalSolve () override {};  
  virtual bool converged () override { return true; };  
  virtual void syncSolutions(Direction /*direction*/) override {};

};
