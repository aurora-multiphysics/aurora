#pragma once

// MOOSE includes
#include "UserObject.h"

// MOAB includes
#include "moab/Core.hpp"

// Libmesh includes
#include <libmesh/elem.h>
#include <libmesh/enum_io_package.h>
#include <libmesh/enum_order.h>
#include <libmesh/enum_fe_family.h>
#include <libmesh/equation_systems.h>
#include <libmesh/system.h>

// Forward Declarations
class MoabUserObject;

template <>
InputParameters validParams<MoabUserObject>();

// User object which is just a wrapper around a MOAB ptr
class MoabUserObject : public UserObject
{
 public: 

  MoabUserObject(const InputParameters & parameters);

  virtual void execute(){};
  virtual void initialize(){};
  virtual void finalize(){};
  virtual void threadJoin(const UserObject & /*uo*/){};

  // Pass in the FE Problem
  void setProblem(FEProblemBase * problem) {
    if(problem==nullptr)
      throw std::logic_error("Problem passed is null");
    _problem_ptr = problem;
  } ;

  // Check if problem has been set
  bool hasProblem(){ return !( _problem_ptr == nullptr ); }; 
  
  // Initialise MOAB
  void initMOAB();

  // // Intialise libMesh systems containing var_now
  // bool initSystem(std::string var_now);
  
  // Pass the OpenMC results into the libMesh systems solution
  bool setSolution(std::string var_now,std::vector< double > &results, double scaleFactor=1., bool normToVol=true);

  // Publically available pointer to MOAB interface
  std::shared_ptr<moab::Interface> moabPtr;

private:

  // Private methods

  // Get a modifyable reference to the underlying libmesh mesh.
  MeshBase& mesh();
  
  // Get a modifyable reference to the underlying libmesh equation systems
  EquationSystems & systems();

  System& system(std::string var_now);

  FEProblemBase& problem();
    
  // Helper methods to set MOAB database
  moab::ErrorCode createNodes(std::map<dof_id_type,moab::EntityHandle>& node_id_to_handle);
  moab::ErrorCode createElems(std::map<dof_id_type,moab::EntityHandle>& node_id_to_handle,std::map<dof_id_type,moab::EntityHandle>& elem_handle_to_id);

  // Helper method to set the results in a given system and variable
  void setSolution(unsigned int iSysNow, unsigned int iVarNow,std::vector< double > &results, double scaleFactor, bool normToVol);

  // Helper methods to convert between indices
  dof_id_type elem_id_to_soln_index(unsigned int iSysNow, unsigned int iVarNow, dof_id_type id);
  dof_id_type bin_index_to_elem_id(unsigned int index);

  // Return the volume of an element (use for tally normalisation)
  double elemVolume(dof_id_type id);
 
  // Pointer to the feProblem we care about
  FEProblemBase * _problem_ptr;
  
  // Map from MOAB element entity handles to libmesh ids.
  std::map<dof_id_type,moab::EntityHandle> _elem_handle_to_id;
  
};
