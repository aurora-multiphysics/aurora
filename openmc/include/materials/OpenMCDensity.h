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

// Derived Density class which allows the original density
// parameter to be returned
class OpenMCDensity : public Density
{
public:
  static InputParameters validParams();

  OpenMCDensity(const InputParameters & params);

  const Real origDensity(){ return _initial_density; };

};
