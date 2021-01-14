#include "AuroraApp.h"
#include "Moose.h"
#include "AppFactory.h"
#include "ModulesApp.h"
#include "MooseSyntax.h"
#include "base/OpenMCApp.h"

InputParameters
AuroraApp::validParams()
{
  InputParameters params = MooseApp::validParams();

  // Do not use legacy DirichletBC, that is, set DirichletBC default for preset = true
  params.set<bool>("use_legacy_dirichlet_bc") = false;

  return params;
}

AuroraApp::AuroraApp(InputParameters parameters) : MooseApp(parameters)
{
  AuroraApp::registerAll(_factory, _action_factory, _syntax);
}

AuroraApp::~AuroraApp() {}

void
AuroraApp::registerAll(Factory & f, ActionFactory & af, Syntax & s)
{
  ModulesApp::registerAll(f, af, s);
  OpenMCApp::registerAll(f, af, s);
  Registry::registerObjectsTo(f, {"AuroraApp"});
  Registry::registerActionsTo(af, {"AuroraApp"});

  /* register custom execute flags, action syntax, etc. here */
}

void
AuroraApp::registerApps()
{
  registerApp(AuroraApp);
}

/***************************************************************************************************
 *********************** Dynamic Library Entry Points - DO NOT MODIFY ******************************
 **************************************************************************************************/
extern "C" void
AuroraApp__registerAll(Factory & f, ActionFactory & af, Syntax & s)
{
  AuroraApp::registerAll(f, af, s);
}
extern "C" void
AuroraApp__registerApps()
{
  AuroraApp::registerApps();
}
