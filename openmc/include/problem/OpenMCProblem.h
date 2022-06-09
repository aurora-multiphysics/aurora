# pragma once

#include "ExternalProblem.h"

class OpenMCProblem;

/** \brief Minimal definition for a FE problem.

    Only included as it is required by Moose
    and ensures our constructron avoids unneccessary initialisation.

    Our executioner does not actually call these methods - it
    will not successfully run for a normal MOOSE executioner.
*/
class OpenMCProblem : public ExternalProblem
{
 public:
  OpenMCProblem (const InputParameters & parameters) :
  ExternalProblem(parameters)  {};

  static InputParameters validParams();

  /// Does nothing
  virtual void externalSolve () override {};
  /// Does nothing
  virtual bool converged () override { return true; };
  /// Does nothing
  virtual void syncSolutions(Direction /*direction*/) override {};

};
