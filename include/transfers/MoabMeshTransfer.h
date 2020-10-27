#pragma once

// Moose includes
#include "MultiAppTransfer.h"
#include "MultiApp.h"

// Sub app includes
#include "userobject/MoabUserObject.h"

// Forward declaration
class MultiApp;
class MoabMeshTransfer;

template <>
InputParameters validParams<MoabMeshTransfer>();

class MoabMeshTransfer : public MultiAppTransfer
{
 public:
  MoabMeshTransfer(const InputParameters & parameters);

  /**
   * Execute the transfer.
   */
  virtual void execute();

  virtual void initialSetup();

 private:

  // Perform the transfer of the mesh data into MOAB user object
  bool transfer();

  bool isInit;
  unsigned int _to_problem_index;

  // Type of the app from which we are taking the mesh and systems
  std::string apptype_from;

  // Type of the app to which we are sending the mesh and systems
  std::string apptype_to;

  // Name of the MOAB user object to which we are transferring the problem and mesh
  std::string moabname;

};
