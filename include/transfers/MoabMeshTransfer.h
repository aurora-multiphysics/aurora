#pragma once

// Moose includes
#include "MultiAppTransfer.h"
#include "MultiApp.h"

// Sub app includes
#include "userobject/MoabUserObject.h"

// Forward declaration
class MultiApp;
class MoabMeshTransfer;

/**
   \brief Transfer a pointer to the FEProblem from the parent to child app in a MultiApp.

   Specifically, pass the data into the MoabUserObject with the name contained in
   moabname, and fail if this does not exist.  Also perform some intialisation of
   the MoabUserObject that should only be done once.

 */
class MoabMeshTransfer : public MultiAppTransfer
{
 public:
  MoabMeshTransfer(const InputParameters & parameters);

  /// Execute the transfer.
  virtual void execute();

  virtual void initialSetup();

  static InputParameters validParams();

 private:

  /// Perform the transfer of the mesh data into MOAB user object
  bool transfer();

  /// Save whether initialisation suceeded.
  bool isInit;

  /// Index of the FEProblem within _to_problems to which we are sending data
  unsigned int _to_problem_index;

  /// Type of the app from which we are taking the mesh and systems
  std::string apptype_from;

  /// Type of the app to which we are sending the mesh and systems
  std::string apptype_to;

  /// Name of the MOAB user object to which we are transferring the problem and mesh
  std::string moabname;

};
