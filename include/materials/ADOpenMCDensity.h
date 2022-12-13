//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "Density.h"

/** \brief Derived ADDensity class which allows the original density
parameter to be returned.
*/
class ADOpenMCDensity : public ADDensity
{
public:
  static InputParameters validParams();

  ADOpenMCDensity(const InputParameters & params);

  /// Return the starting density of the material (i.e. prior to deformation)
  const Real origDensity(){ return _initial_density; };

};
