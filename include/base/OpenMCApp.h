//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MooseApp.h"

/**
   \brief Standalone app to wrap OpenMC

   When run as a sub-app in a MultiApp data may be passed in and out via
   MoabUserObject
 */
class OpenMCApp : public MooseApp
{
public:
  static InputParameters validParams();

  OpenMCApp(InputParameters parameters);
  virtual ~OpenMCApp();

  static void registerApps();
  static void registerAll(Factory & f, ActionFactory & af, Syntax & s);
};

