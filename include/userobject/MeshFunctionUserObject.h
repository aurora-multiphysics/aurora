#pragma once

// MOOSE includes
#include "GeneralUserObject.h"

// libMesh includes
#include "libmesh/mesh_function.h"
#include <libmesh/system.h>

// Forward declaration
class MeshFunctionUserObject;

template <>
InputParameters validParams<MeshFunctionUserObject>();

// Similar to SolutionUserObject, but the variable is loaded from memory
class MeshFunctionUserObject : public GeneralUserObject
{
 public:

  MeshFunctionUserObject(const InputParameters & parameters);

  // Sets the mesh function from a variable in memory
  virtual void execute() override;

  //Called before execute() is ever called so that data can be cleared.
  virtual void initialize() override {};

  // Any MPI communication!
  virtual void finalize() override {};

  // Evaluate the mesh function at a point
  Number value (const Point &p, const Real t=0.) const;

private:
  
  EquationSystems & systems();

  System& system(std::string var_now);
  
  // Pointer to the libmesh mesh function object
  std::unique_ptr<MeshFunction> _mesh_function;

  // Name of variable we want to turn into a function
  std::string _var_name;
};
