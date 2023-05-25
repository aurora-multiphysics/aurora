#pragma once

// MOOSE includes
#include "UserObject.h"

/**
    \brief UserObject class to specify a DAGMC surface boundary condition
 */
class DagSurfaceUserObject : public UserObject
{
 public:

  /// Constructor
  DagSurfaceUserObject(const InputParameters & parameters);

  static InputParameters validParams();

  /// Override MOOSE virtual method to do nothing
  virtual void execute(){};
  /// Override MOOSE virtual method to do nothing
  virtual void initialize(){};
  /// Override MOOSE virtual method to do nothing
  virtual void finalize(){};
  /// Override MOOSE virtual method to do nothing
  virtual void threadJoin(const UserObject & /*uo*/){};

  /// Return the type
  std::string get_boundary_type() const { return boundary_type_; };
  /// Return the boundary ids
  std::vector<std::string> get_boundary_names() const { return boundary_names_; };

 private:

  /// Store the type of the boundary as enum 
  std::string boundary_type_;
  /// Store the names of the boundaries upon which the condition is applied
  std::vector<std::string> boundary_names_;

};
