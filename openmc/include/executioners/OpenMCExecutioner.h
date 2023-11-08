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
#include "openmc/dagmc.h" // DAGUniverse
#include "openmc/error.h"
#include "openmc/file_utils.h"
#include "openmc/geometry.h" // overlap_check_count
#include "openmc/geometry_aux.h" // finalize geometry
#include "openmc/material.h"
#include "openmc/mesh.h"
#include "openmc/message_passing.h"
#include "openmc/mgxs_interface.h" // data::mg
#include "openmc/nuclide.h" // data::nuclide_map
#include "openmc/output.h" // print_plot
#include "openmc/plot.h"
#include "openmc/tallies/filter_mesh.h"
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

/// \brief Our bespoke Executioner class to perform OpenMC runs
class OpenMCExecutioner : public Transient
{
public:
  OpenMCExecutioner(const InputParameters & parameters);

  ~OpenMCExecutioner();

  static InputParameters validParams();

  /** \brief Perform a full OpenMC run

      1) Intialisation<br>
      2) Run<br>
      3) Post-processing of results<br>
      4) Write output (standalone only)<br>
   */
  virtual void execute() override;

private:

  /// \brief Helper struct to store information about OpenMC tally filters
  struct FilterInfo{
    /// Global index
    int32_t index;
    /// Unique ID
    int32_t id;
    /// Shape data for results array
    int32_t stride;
    /// Number of filter bins
    size_t nbins;
    /// Type of filter
    openmc::FilterType type;
  };

  /// \brief Helper struct to store information about OpenMC scores
  struct ScoreData {
    /// Name of the score to extract
    std::string score_name;
    /// Name of the variable in which we will store the score mean
    std::string var_name;
    /// Name of the variable in which we will store the score std deviation
    std::string err_name;
    /// Switch to control if we save the error
    bool saveErr;
    /// Factor by which we need to multiply results to get desired units
    double scale_factor;
    /// Index for the score in the tally
    int index;
  };

  // Methods

  /// Get the moab user object
  MoabUserObject& moab();

  /// Initialise structures to store information about scores
  void initScoreData();

  /// Initialise MOAB/OpenMC
  void initialize();

  /// Run OpenMC and get results
  bool run();

  /// Pass results into FEProblem
  bool processResults();

  /// Output the results
  bool output();

  /// Update MOAB with any results from MOOSE
  void update();

  /// Initialise MOAB
  bool initMOAB();

  /// Initialise OpenMC
  bool initOpenMC();

  /// Initialise booking of DAGMC universe
  bool initDAGUniverse();

  /// Initialise material maps of names to ids
  bool initMaterials();

  /// Pass MOAB mesh into OpenMC and set up tallies with a mesh filter
  bool initMeshTallies();

  /// Return the openmc id of the material
  bool getMatID(moab::EntityHandle vol_handle, int& mat_id);

  /// Update OpenMC
  bool updateOpenMC();

  /// Process Tallies from OpenMC
  bool getResults(std::map<std::string,std::vector< double > > & var_results_by_elem);
  /// Set solution in FEProblem variable
  bool setSolution(std::vector< double > & results_by_elem,
                   std::string var_name,
                   double scale_factor,
                   bool isErr);

  // Helper methods to extract tally results

  /// Set information about OpenMC filters (IDs and strides)
  bool setFilterInfo(openmc::Tally& tally,
                     std::map<int32_t, FilterInfo>& filters_by_id,
                     int32_t& meshFilter,
                     int32_t& nFilterBins);

  /// Given a 1D results vector bin index, return a filter bin index for the given filter
  int32_t getFilterBin(int32_t iResultBin, const FilterInfo & filter);
  /// Given a 1D results vector bin index, return the set of filter bins index
  bool decomposeIntoFilterBins(int32_t iResultBin,
                               const std::map<int32_t, FilterInfo>& filters_by_id,
                               std::map<int32_t,int32_t>& filter_id_to_bin_index);


  /// Clear some data in OpenMC
  bool resetOpenMC();

  /// Update DAGMC with reset MOAB interface
  bool reloadDAGMC();

  /// Update DAGMC universe in OpenMC
  void updateDAGUniverse();

  /// Update materials
  void updateMaterials();

  /// Update materials' densities
  void updateMaterialDensities();

  /// Update mesh tallies
  void updateMeshTallies();

  /// Set up OpenMC tallies
  void setupTallies(openmc::Filter* filter_ptr);

  /// Set up an OpenMC tally
  void setupTally(int32_t& tally_id,
                  openmc::Filter* filter_ptr,
                  std::vector<ScoreData>& scores);

  /// Set up OpenMC cells
  //bool setupCells();

  /// Set up OpenMC surfaces
  //bool setupSurfaces();

  /// Set attributes of a DAGMC cell
  //bool setCellAttrib(openmc::DAGCell& cell,unsigned int index,int32_t universe_idx);

  /// Set attributes of a DAGMC surface
  // bool setSurfAttrib(openmc::DAGSurface& surf,unsigned int index);

  /// Final openMC set up after geometery has been set.
  void completeSetup();

  // Data members

  /// Constant to convert eV to joules
  static constexpr double eVinJoules = 1.60218e-19;

  /// Constant for geometric volume dimension
  static constexpr int DIM_VOL = 3;

  /// Constant for geometric surface dimension
  static constexpr int DIM_SURF = 2;

  /// Copy of the pointer to DAGMC
  std::shared_ptr<moab::DagMC> dagPtr;

  /// OpenMC index for DAGMC universe
  int32_t dag_univ_idx;

  /// Record whether we set the FE Problem locally.
  bool setProblemLocal;

  /// Save whether initialised
  bool isInit;

  /// Save if we have updated the materials
  bool matsUpdated;

  /// Save if we are updating densities
  bool updateDensity;

  /// Save whether we have a UWUW material library
  bool useUWUW;

  /// Hold OpenMC error code
  int openmc_err;

  /// Strength of fusion neutron source in neutrons/s
  double source_strength;

  /// Map of OpenMC IDs of the tallies to list of score and variable names
  std::map<int32_t, std::vector<ScoreData> > tally_ids_to_scores;

  /// Convenience map of mat name string to its id
  std::map<std::string,int32_t> mat_names_to_id;

  /// Convenience map of mat name string to its id
  std::map<int32_t,double> mat_id_to_density;

  /// Place to store graveyard entity handle
  moab::EntityHandle graveyard;

  /// Switch to control if we should automatically add variables to FEProblem
  bool add_variables;

  /// Switch to control whether openmc should launch child threads
  bool launch_threads;

  /// Number of threads to use if launch_threads = true
  unsigned int n_threads;

  /// Switch to control whether dagmc output is written to file or not.
  bool redirect_dagout;

  /// Name of dagmc logfile
  std::string dagmc_logname;

  /// Todo: how to deal with threads?
  std::fstream dagmclog;

  /// Performance timer for execution
  PerfID _execute_timer;
  /// Performance timer for initialisation
  PerfID _init_timer;
  /// Performance timer for MOAB initialisation
  PerfID _initmoab_timer;
  /// Performance timer for OpenMC initialisation
  PerfID _initopenmc_timer;
  /// Performance timer for OpenMC update between timesteps
  PerfID _updateopenmc_timer;
  /// Performance timer for OpenMC runs
  PerfID _run_timer;
  /// Performance timer for writing output in standalone mode
  PerfID _output_timer;

};
#endif // OPENMCEXECUTIONER_H
