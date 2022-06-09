#include "OpenMCProblem.h"

registerMooseObject("OpenMCApp", OpenMCProblem);

InputParameters
OpenMCProblem::validParams()
{
  InputParameters params = ExternalProblem::validParams();
  return params;
}
