#include "OpenMCProblem.h"

registerMooseObject("OpenMCApp", OpenMCProblem);

template <>
InputParameters
validParams<OpenMCProblem>()
{
  InputParameters params = validParams<ExternalProblem>();
  return params;
}
