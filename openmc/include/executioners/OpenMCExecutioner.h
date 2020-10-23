#ifndef OPENMCEXECUTIONER_H
#define OPENMCEXECUTIONER_H

// Moose includes
#include "Transient.h"
#include "MoabUserObject.h"

// OpenMC includes
#include "openmc/capi.h"
#include "openmc/error.h"
#include "openmc/mesh.h"
#include "openmc/tallies/tally.h"
#include "openmc/tallies/filter.h"
#include "xtensor/xio.hpp"

class OpenMCExecutioner;

template <>
InputParameters validParams<OpenMCExecutioner>();

class OpenMCExecutioner : public Transient
{
public:
  OpenMCExecutioner(const InputParameters & parameters);
  
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

  // Get the moab user object
  MoabUserObject& moab();

  // Initialise MOAB
  bool initMOAB();
  
  // Initialise OpenMC
  bool initOpenMC();

  // Process Tallies from OpenMC
  bool getResults(std::vector< std::vector< double > > &results_by_mat );

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
    
  // Data members

  // Record whether we set the FE Problem locally.
  bool setProblemLocal;
  
  // Hold OpenMC error code
  int openmc_err;
      
  // To-do get from file...
  std::string var_name;
  
};
#endif // OPENMCEXECUTIONER_H
