#pragma once

// MOOSE includes
#include "UserObject.h"

// Forward Declarations
class DagSurfaceUserObject;

template <>
InputParameters validParams<DagSurfaceUserObject>();

/// Encode the different types of DAGMC boundary type
MooseEnum DagBoundaryType("Graveyard","Vacuum","Reflecting","White","Periodic");

/**
    \brief UserObject class to specify a DAGMC surface boundary condition
 */
class DagSurfaceUserObject : public UserObject
{
 public:

  /// Constructor
  DagSurfaceUserObject(const InputParameters & parameters);

  /// Override MOOSE virtual method to do nothing
  virtual void execute(){};
  /// Override MOOSE virtual method to do nothing
  virtual void initialize(){};
  /// Override MOOSE virtual method to do nothing
  virtual void finalize(){};
  /// Override MOOSE virtual method to do nothing
  virtual void threadJoin(const UserObject & /*uo*/){};

  /// Return the type
  DagBoundaryType get_boundary_type() const { return boundary_type; };
  /// Return the boundary ids
  std::vector<boundary_id_type> get_boundary_ids() const { return boundary_ids; };

 private:

  /// Store the type of the boundary as a MOOSE enum
  DagBoundaryType boundary_type;
  /// Store the names of the boundaries upon which the condition is applied
  std::vector<BoundaryName> boundary_names;
  /// Store the ids of the boundaries upon which the condition is applied
  std::vector<boundary_id_type> boundary_ids;

}
