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

  // Update MOAB with any results from MOOSE
  bool update();

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
  moab::ErrorCode createElems(std::map<dof_id_type,moab::EntityHandle>& node_id_to_handle);

  // Clear the maps between entity handles and dof ids
  void clearElemMaps();

  // Add an element to maps
  void addElem(dof_id_type id,moab::EntityHandle ent);

  // Helper method to set the results in a given system and variable
  void setSolution(unsigned int iSysNow, unsigned int iVarNow,std::vector< double > &results, double scaleFactor, bool normToVol);

  // Helper methods to convert between indices
  dof_id_type elem_id_to_soln_index(unsigned int iSysNow, unsigned int iVarNow, dof_id_type id);
  dof_id_type bin_index_to_elem_id(unsigned int index);

  // Return the volume of an element (use for tally normalisation)
  double elemVolume(dof_id_type id);

  // Sort elems in to bins of a given temperature
  bool sortElemsByResults();

  // Group the binned elems into local temperature regions
  bool groupLocalElems();

  // Group a given bin into local regions
  // NB elems in param is a copy, localElems is a reference
  void groupLocalElems(std::set<dof_id_type> elems, std::vector<moab::Range>& localElems);


  // Given a value of our variable, find what bin this corresponds to.
  int getResultsBin(double value);

  // Clear the containers of elements grouped into bins of constant temp
  void resetContainers();

  // Pointer to the feProblem we care about
  FEProblemBase * _problem_ptr;

  // Map from MOAB element entity handles to libmesh ids.
  std::map<moab::EntityHandle,dof_id_type> _elem_handle_to_id;

  // Reverse map
  std::map<dof_id_type,moab::EntityHandle> _id_to_elem_handle;

  // Members required to sort elems into bins given a result evaluated on that elem

  // Input Settings
  bool binElems;
  std::string var_name; // Name of the MOOSE variable
  double var_min; // Minimum value of our variable
  unsigned int nPow; // Number of powers of 10 to bin in
  unsigned int nMinor; // Number of minor divisions for binning on a log scale

  int powMin; // Number of powers of 10 to bin in
  unsigned int nBins; // nPow*nMinor
  // std::vector<double> edges; // Bin edges
  // std::vector<double> midpoints; // Evaluate the temperature representing the bin mipoint
  std::vector<std::set<dof_id_type> > sortedElems; // Sort elems into bins
  std::vector<std::vector<moab::Range> > elemGroups; // Neighbouring ranges of moab elems to skin in bins of temperature

};
