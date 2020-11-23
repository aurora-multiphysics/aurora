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

  // Initialise MOAB/OpenMC
  bool initialize();

  // Run OpenMC and get results
  bool run();

  // Update MOAB with any results from MOOSE
  bool update();

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

  // constant to convert eV to joules
  static constexpr double eVinJoules = 1.60218e-19;

  // Record whether we set the FE Problem locally.
  bool setProblemLocal;

  // Save whether initialised
  bool isInit;

  // Hold OpenMC error code
  int openmc_err;

  // Name of the variable in which we will store the tally data
  std::string var_name;

  // Name of the score to extract
  std::string score_name;

  // Strength of fusion neutron source in neutrons/s
  double source_strength;

  // Factor by which we need to multiply results
  double scale_factor;

  // OpenMC ID of the tally we will use
  int32_t tally_id;

  // OpenMC ID of the mesh to which we pass data
  int32_t mesh_id;

};
#endif // OPENMCEXECUTIONER_H
