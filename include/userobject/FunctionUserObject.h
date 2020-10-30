#pragma once

// MOOSE includes
#include "GeneralUserObject.h"

// libMesh includes
#include "libmesh/mesh_function.h"
#include <libmesh/system.h>

// Forward declaration
class FunctionUserObject;

template <>
InputParameters validParams<FunctionUserObject>();

// Similar to SolutionUserObject, but the variable is loaded from memory
class FunctionUserObject : public GeneralUserObject
{
 public:

  FunctionUserObject(const InputParameters & parameters);

  // Sets the mesh function from a variable in memory
  virtual void execute() override;

  //Called before execute() is ever called so that data can be cleared.
  virtual void initialize() override {};

  // Any MPI communication!
  virtual void finalize() override {};

  // Evaluate the mesh function at a point
  Number value (const Point &p) const;

private:

  // Name of variable we want to turn into a function
  std::string _var_name;

  // Pointer to the libMesh system containing our variable
  System* sysPtr;

  // Pointer to a libMesh point locator
  std::unique_ptr< PointLocatorBase > point_locator_ptr;

  // Save the system and variable number for convenience
  unsigned int iVar;
  unsigned int iSys;
};
