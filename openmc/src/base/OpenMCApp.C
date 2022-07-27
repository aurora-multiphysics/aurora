#include "OpenMCApp.h"
#include "Moose.h"
#include "AppFactory.h"
#include "ModulesApp.h"
#include "MooseSyntax.h"

InputParameters
OpenMCApp::validParams()
{
  InputParameters params = MooseApp::validParams();
  params.set<bool>("use_legacy_material_output") = false;
  return params;
}

OpenMCApp::OpenMCApp(InputParameters parameters) : MooseApp(parameters)
{
  OpenMCApp::registerAll(_factory, _action_factory, _syntax);
}

OpenMCApp::~OpenMCApp() {}

void
OpenMCApp::registerAll(Factory & f, ActionFactory & af, Syntax & s)
{
  ModulesApp::registerAll(f, af, s);
  Registry::registerObjectsTo(f, {"OpenMCApp"});
  Registry::registerActionsTo(af, {"OpenMCApp"});

  /* register custom execute flags, action syntax, etc. here */
}

void
OpenMCApp::registerApps()
{
  registerApp(OpenMCApp);
}

/***************************************************************************************************
 *********************** Dynamic Library Entry Points - DO NOT MODIFY ******************************
 **************************************************************************************************/
extern "C" void
OpenMCApp__registerAll(Factory & f, ActionFactory & af, Syntax & s)
{
  OpenMCApp::registerAll(f, af, s);
}
extern "C" void
OpenMCApp__registerApps()
{
  OpenMCApp::registerApps();
}
