#pragma once

// MOOSE includes
#include "GeneralUserObject.h"
#include "DisplacedProblem.h"

// libMesh includes
#include "libmesh/mesh_function.h"
#include <libmesh/system.h>

// Forward declaration
class FunctionUserObject;

/** \brief This UserObject class creates a function from a variable in memory given
 *  a string containing the variable name.
 */
class FunctionUserObject : public GeneralUserObject
{
 public:

  FunctionUserObject(const InputParameters & parameters);

  static InputParameters validParams();

  /// Sets the mesh function from a variable in memory
  virtual void execute() override;

  /// Called before execute() is ever called so that data can be cleared.
  virtual void initialize() override {};

  /// Any MPI communication!
  virtual void finalize() override {};

  /// Evaluate the mesh function at a point
  Number value (const Point &p) const;

private:

  /// Reference to the mesh
  MooseMesh& mesh();

  /// Name of variable we want to turn into a function
  std::string _var_name;

  /// Tolerance to pass to point locator
  double tolerance;

  /// Pointer to the libMesh system containing our variable
  System* sysPtr;

  /// Pointer to a libMesh point locator
  std::unique_ptr< PointLocatorBase > point_locator_ptr;

  /// Save the system and variable number for convenience
  unsigned int iVar;
  unsigned int iSys;
};
