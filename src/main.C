//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "AuroraApp.h"
#include "MooseInit.h"
#include "Moose.h"
#include "MooseApp.h"
#include "AppFactory.h"
#include "CommandLine.h"

// Create a performance log
PerfLog Moose::perf_log("Aurora");

// Begin the main program.
int
main(int argc, char * argv[])
{
  // Initialize MPI, solvers and MOOSE
  MooseInit init(argc, argv);

  // Get which app to run
  CommandLine::Option app_opt = {
      "which app to run", {"--app", "app_name"}, false, CommandLine::ARGUMENT::OPTIONAL, {}};

  auto cmds = CommandLine(argc, argv);
  cmds.addOption("which_app", app_opt);

  std::string which_app;
  cmds.search("which_app", which_app);

  // Register this application's MooseApp and any it depends on
  AuroraApp::registerApps();

  std::string app_class_name;
  if (which_app == "openmc")
    app_class_name = "OpenMCApp";
  else
    app_class_name = "AuroraApp";

  // Create an instance of the application and store it in a smart pointer for easy cleanup
  std::shared_ptr<MooseApp> app = AppFactory::createAppShared(app_class_name, argc, argv);

  // Execute the application
  app->run();

  return 0;
}
