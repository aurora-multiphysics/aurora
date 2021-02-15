#pragma once

// MOOSE includes
#include "UserObject.h"
#include "MaterialBase.h"

// MOAB includes
#include "moab/Core.hpp"
#include "moab/Skinner.hpp"
#include "moab/GeomTopoTool.hpp"
#include "MBTagConventions.hpp"

// Libmesh includes
#include <libmesh/elem.h>
#include <libmesh/enum_io_package.h>
#include <libmesh/enum_order.h>
#include <libmesh/enum_fe_family.h>
#include <libmesh/equation_systems.h>
#include <libmesh/system.h>
#include <libmesh/mesh_tools.h>

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

  // Intialise objects needed to perform binning of elements
  void initBinningData();

  // Clear mesh data
  void reset();

  // Update MOAB with any results from MOOSE
  bool update();

  // Pass the OpenMC results into the libMesh systems solution
  bool setSolution(std::string var_now,std::vector< double > &results, double scaleFactor=1., bool normToVol=true);

  double getTemperature(moab::EntityHandle vol);

  // Publically available pointer to MOAB interface
  std::shared_ptr<moab::Interface> moabPtr;

private:

  // Private types
  enum Sense { BACKWARDS=-1, FORWARDS=1};
  struct VolData{
    moab::EntityHandle vol;
    Sense sense;
  };

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
  moab::ErrorCode createTags();
  moab::ErrorCode createGroup(unsigned int id, std::string name,moab::EntityHandle& group_set);
  moab::ErrorCode createVol(unsigned int id,moab::EntityHandle& volume_set,moab::EntityHandle group_set);
  moab::ErrorCode createSurf(unsigned int id,moab::EntityHandle& surface_set, moab::Range& faces,  std::vector<VolData> & voldata);
  moab::ErrorCode createSurfaces(moab::Range& reversed, VolData& voldata, unsigned int& surf_id);


  moab::ErrorCode createSurfaceFromBox(const BoundingBox& box, VolData& voldata, unsigned int& surf_id, bool normalout, double factor=1.0);
  moab::ErrorCode createNodesFromBox(const BoundingBox& box,double factor,std::vector<moab::EntityHandle>& vert_handles);
  moab::ErrorCode createCornerTris(const std::vector<moab::EntityHandle> & verts,
                                   unsigned int corner, unsigned int v1,
                                   unsigned int v2 ,unsigned int v3,
                                   bool normalout, moab::Range &surface_tris);
  moab::ErrorCode createTri(const std::vector<moab::EntityHandle> & vertices,unsigned int v1, unsigned int v2 ,unsigned int v3, moab::Range &surface_tris);

  moab::ErrorCode updateSurfData(moab::EntityHandle surface_set,VolData data);
  moab::ErrorCode setTags(moab::EntityHandle ent,std::string name, std::string category, unsigned int id, int dim);
  moab::ErrorCode setTagData(moab::Tag tag, moab::EntityHandle ent, std::string data, unsigned int SIZE);
  moab::ErrorCode setTagData(moab::Tag tag, moab::EntityHandle ent, void* data);

  // Build the graveyard (needed by OpenMC)
  moab::ErrorCode buildGraveyard(unsigned int & vol_id, unsigned int & surf_id);

  // Get the coords of the box back as an array (possibly scaled)
  std::vector<Point> boxCoords(const BoundingBox& box, double factor);

  // Look for materials in the FE problem
  void findMaterials();

  // Clear the maps between entity handles and dof ids
  void clearElemMaps();

  // Add an element to maps
  void addElem(dof_id_type id,moab::EntityHandle ent);

  // Helper method to setthe results in a given system and variable
  void setSolution(unsigned int iSysNow, unsigned int iVarNow,std::vector< double > &results, double scaleFactor, bool normToVol);

  // Helper methods to convert between indices
  dof_id_type elem_to_soln_index(const Elem& elem,unsigned int iSysNow, unsigned int iVarNow);
  unsigned int elem_id_to_bin_index(dof_id_type id);

  // Sort elems in to bins of a given temperature
  bool sortElemsByResults();

  // Group the binned elems into local temperature regions and find their surfaces
  bool findSurfaces();

  // Group a given bin into local regions
  // NB elems in param is a copy, localElems is a reference
  void groupLocalElems(std::set<dof_id_type> elems, std::vector<moab::Range>& localElems);

  // Given a value of our variable, find what bin this corresponds to.
  int getResultsBin(double value);
  inline int getResultsBinLin(double value);
  int getResultsBinLog(double value);

  // Calculate the variable evaluatec at the bin midpoints
  void calcMidpoints();
  void calcMidpointsLin();
  void calcMidpointsLog();

  // Clear the containers of elements grouped into bins of constant temp
  void resetContainers();

  // Clear MOAB entity sets
  bool resetMOAB();

  // Find the surfaces for the provided range and add to group
  bool findSurface(const moab::Range& region,moab::EntityHandle group, unsigned int & vol_id, unsigned int & surf_id,moab::EntityHandle& volume_set);

  bool writeSurfaces();

  void communicateDofSet(std::set<dof_id_type>& dofset);

  // Pointer to the feProblem we care about
  FEProblemBase * _problem_ptr;

  // Pointer to a moab skinner for finding temperature surfaces
  std::unique_ptr< moab::Skinner > skinner;

  // Pointer for gtt for setting surface sense
  std::unique_ptr< moab::GeomTopoTool > gtt;

  // Convert MOOSE units to dagmc length units
  double lengthscale;

  // Map from MOAB element entity handles to libmesh ids.
  std::map<moab::EntityHandle,dof_id_type> _elem_handle_to_id;

  // Reverse map
  std::map<dof_id_type,moab::EntityHandle> _id_to_elem_handle;

  // Members required to sort elems into bins given a result evaluated on that elem
  std::string var_name; // Name of the MOOSE variable

  bool binElems; // Whether or not to perform binning
  bool logscale; // Whether or not to bin in a log scale

  double var_min; // Minimum value of our variable
  double var_max; // Maximum value of our variable for binning on a linear scale
  double bin_width; // Fixed bin width for binning on a linear scale

  int powMin; // Minimum power of 10
  int powMax; // Maximum power of 10

  unsigned int nVarBins; // Number of variable bins to use
  unsigned int nPow; // Number of powers of 10 to bin in for binning on a log scale
  unsigned int nMinor; // Number of minor divisions for binning on a log scale
  unsigned int nMatBins; // Number of distinct subdomains (e.g. vols, mats)
  unsigned int nSortBins; // Number of bins needed for sorting results (mats*varbins)

  std::vector<std::set<dof_id_type> > sortedElems; // Container for elems sorted by variable bin and materials

  // System Variables
  libMesh::System* sysPtr;
  unsigned int iSysBin;
  unsigned int iVarBin;

  // A mesh function to evaluate temperature
  //std::shared_ptr<MeshFunction> meshFunctionPtr;

  // Materials data
  std::vector<std::string> mat_names; // material names
  std::vector< std::set<SubdomainID> > mat_blocks; // all element blocks assigned to mats

  // An entitiy handle to represent the set of all tets
  moab::EntityHandle meshset;

  // Save some topological data: map from surface handle to vol handle and sense
  std::map<moab::EntityHandle, std::vector<VolData> > surfsToVols;

  // Save a map of EntityHandle to temperature
  std::map<moab::EntityHandle,double> volToTemp;

  // Some moab tags
  moab::Tag geometry_dimension_tag;
  moab::Tag id_tag;
  moab::Tag faceting_tol_tag;
  moab::Tag geometry_resabs_tag;
  moab::Tag category_tag;
  moab::Tag name_tag;
  moab::Tag material_tag; // Moab tag handle corresponding to material label

  // DagMC settings
  double faceting_tol;
  double geom_tol;

  // Store the temperature corresponding to the bin mipoint
  std::vector<double> midpoints;

  // Settings to control the optional writing of surfaces to file.
  bool output_skins;
  std::string output_base;
  unsigned int n_output; // Number of writes
  unsigned int n_period; // Period of writes (skip every n_period -1)
  unsigned int n_write; // Number of times file has been written to
  unsigned int n_its; // Store the number of times writeSurfaces is called

  PerfID _init_timer;
  PerfID _update_timer;
  PerfID _setsolution_timer;

};
