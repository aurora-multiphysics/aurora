#ifndef OPENMCEXECUTIONER_H
#define OPENMCEXECUTIONER_H

// Moose includes
#include "Transient.h"

// Libmesh includes
#include <libmesh/elem.h>
#include <libmesh/enum_io_package.h>
#include <libmesh/enum_order.h>
#include <libmesh/enum_fe_family.h>
#include <libmesh/equation_systems.h>
#include <libmesh/system.h>

// OpenMC includes
#include "openmc/capi.h"
#include "openmc/error.h"
#include "openmc/mesh.h"
#include "openmc/tallies/tally.h"
#include "openmc/tallies/filter.h"
#include "xtensor/xio.hpp"

// MOAB includes
#include "moab/Core.hpp"

class OpenMCExecutioner;

template <>
InputParameters validParams<OpenMCExecutioner>();

class OpenMCExecutioner : public Transient
{
public:
  OpenMCExecutioner(const InputParameters & parameters);
  
  virtual void init() override;
  virtual void execute() override;
 
private:

  // Helper struct
  struct FilterInfo{
    int32_t index; //Global index
    int32_t id; // Unique ID
    int32_t stride; // Shape data for results array
    size_t nbins; // Number of filter bins
    std::string type; // Type of filter 
  };

  // Methods

  // Get a modifyable reference to the underlying libmesh mesh.
  MeshBase& mesh();

  // Get a modifyable reference to the underlying libmesh equation systems
  EquationSystems & systems();

  System& system(std::string var_now);

  // Intialise libMesh equations systems that we will use to enter our results
  bool initSystems();

  // Initialise MOAB
  bool initMOAB();
  
  // Initialise OpenMC
  bool initOpenMC();

  // Initialise the MOOSE FEProblem
  bool initProblem();

  // Process Tallies from OpenMC
  bool getResults(std::vector< std::vector< double > > &results_by_mat );

  // Pass the OpenMC results into the libMesh systems solution
  void setSolution(unsigned int iSysNow, unsigned int iVarNow,std::vector< double > &results);

  // Helper methods to set MOAB database
  moab::ErrorCode createNodes(std::map<dof_id_type,moab::EntityHandle>& node_id_to_handle);
  moab::ErrorCode createElems(std::map<dof_id_type,moab::EntityHandle>& node_id_to_handle,std::map<dof_id_type,moab::EntityHandle>& elem_handle_to_id);

  // Helper methods to extract tally results
  bool setFilterInfo(openmc::Tally& tally,
                     std::map<int32_t, FilterInfo>& filters_by_id,
                     int32_t& meshFilter,
                     int32_t& matFilter,
                     int32_t& nFilterBins);
  
  int32_t getFilterBin(int32_t iResultBin, const FilterInfo & filter);
  bool decomposeIntoFilterBins(int32_t iResultBin,
                               const std::map<int32_t, FilterInfo>& filters_by_id,
                               std::map<int32_t,int32_t>& filter_id_to_bin_index);

  moab::EntityHandle bin_index_to_handle(unsigned int index);
  
  dof_id_type bin_index_to_soln_index(unsigned int iSysNow, unsigned int iVarNow, unsigned int index);
  
  
  // Data members
  bool isInit;
  
  // Hold OpenMC error code
  int openmc_err;
  
  // Pointer to MOAB interface
  std::shared_ptr<moab::Interface> moabPtr;
  
  // Map form MOAB element entity handles to libmesh ids.
  std::map<dof_id_type,moab::EntityHandle> _elem_handle_to_id;
    
  // ID to retrieve the system of interest that lives in equation systems
  unsigned int iSys;

  // ID to retrieve the variable of interest that lives in system
  unsigned int iVar;

  // To-do get from file...
  std::string var_name;
  
};
#endif // OPENMCEXECUTIONER_H
