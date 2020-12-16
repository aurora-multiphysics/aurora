#ifndef OPENMCEXECUTIONER_H
#define OPENMCEXECUTIONER_H

// Moose includes
#include "Transient.h"
#include "MoabUserObject.h"

// DagMC includes
#include "DagMC.hpp"
#include "dagmcmetadata.hpp"

#define DAGMC 1

// OpenMC includes
#include "openmc/capi.h"
#include "openmc/cell.h"
#include "openmc/constants.h" // enum RunMode
#include "openmc/cross_sections.h"
#include "openmc/dagmc.h" // model::DAG
#include "openmc/error.h"
#include "openmc/geometry.h" // overlap_check_count
#include "openmc/geometry_aux.h" // finalize geometry
#include "openmc/material.h"
#include "openmc/mesh.h"
#include "openmc/message_passing.h"
#include "openmc/mgxs_interface.h" // data::mg
#include "openmc/nuclide.h" // data::nuclide_map
#include "openmc/output.h" // print_plot
#include "openmc/plot.h"
#include "openmc/tallies/filter.h"
#include "openmc/tallies/tally.h"
#include "openmc/timer.h" // simulation:time_read_xs
#include "openmc/thermal.h" // data::thermal_scatt_map
#include "openmc/settings.h" // settings::run_mode
#include "openmc/string_utils.h"
#include "openmc/summary.h"
#include "openmc/surface.h"
#include "xtensor/xio.hpp"

#include "uwuw.hpp"

class OpenMCExecutioner;

template <>
InputParameters validParams<OpenMCExecutioner>();

class OpenMCExecutioner : public Transient
{
public:
  OpenMCExecutioner(const InputParameters & parameters);

  ~OpenMCExecutioner();

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
  void initialize();

  // Run OpenMC and get results
  bool run();

  // Update MOAB with any results from MOOSE
  void update();

  // Initialise MOAB
  bool initMOAB();

  // Initialise OpenMC
  bool initOpenMC();

  // Initialise material maps (either local map or UWUW)
  bool initMaterials();

  // Set up map of names to ids
  bool initMatNames();

  // Return the openmc id of the material
  bool getMatID(moab::EntityHandle vol_handle, int& mat_id);

    // Update OpenMC
  bool updateOpenMC();

  // Process Tallies from OpenMC
  bool getResults(std::vector< double > &results_by_elem);

  // Helper methods to extract tally results
  bool setFilterInfo(openmc::Tally& tally,
                     std::map<int32_t, FilterInfo>& filters_by_id,
                     int32_t& meshFilter,
                     int32_t& nFilterBins);

  int32_t getFilterBin(int32_t iResultBin, const FilterInfo & filter);
  bool decomposeIntoFilterBins(int32_t iResultBin,
                               const std::map<int32_t, FilterInfo>& filters_by_id,
                               std::map<int32_t,int32_t>& filter_id_to_bin_index);


  // clear some data in OpenMC
  bool resetOpenMC();

  // Pass in mesh, new surfaces, setup metadata
  bool reloadDAGMC();

  // Set up OpenMC cells, surfaces
  bool setupCells();
  bool setupSurfaces();

  bool setCellAttrib(openmc::DAGCell& cell,unsigned int index,int32_t universe_idx);
  bool setSurfAttrib(openmc::DAGSurface& surf,unsigned int index);

  // Final openMC set up after geometery has been set.
  void completeSetup();

  // Data members

  // constant to convert eV to joules
  static constexpr double eVinJoules = 1.60218e-19;

  // Geometric dimensions
  static constexpr int DIM_VOL = 3;
  static constexpr int DIM_SURF = 2;

  moab::DagMC* dagPtr; // Copy of the pointer to DAGMC
  std::unique_ptr<dagmcMetaData> dmdPtr;
  std::unique_ptr<UWUW> uwuwPtr;

  // Record whether we set the FE Problem locally.
  bool setProblemLocal;

  // Save whether initialised
  bool isInit;

  // Save whether we have a UWUW material library
  bool useUWUW;

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

  std::map<std::string,int32_t> mat_names_to_id;

  // Place to store graveyard entity handle
  moab::EntityHandle graveyard;

  // Switch to control whether dagmc output is written to file or not.
  bool redirect_dagout;

  // Name of dagmc logfile
  std::string dagmc_logname;

  // Todo: how to deal with threads?
  std::fstream dagmclog;


};
#endif // OPENMCEXECUTIONER_H
