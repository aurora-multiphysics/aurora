OPENMC_DIR = $(shell which openmc | sed 's/\/bin\/openmc//')
OPENMC_INCDIR = $(OPENMC_DIR)/include
OPENMC_LIBDIR = $(OPENMC_DIR)/lib
OPENMC_INC = -I$(OPENMC_INCDIR)
OPENMC_LIB = -Wl,-rpath,$(OPENMC_LIBDIR) -L$(OPENMC_LIBDIR) -lopenmc

HDF5_DIR = $(shell which h5ls | sed 's/\/bin\/h5ls//')
HDF5_INCDIR = $(HDF5_DIR)/include/hdf5/serial
HDF5_INC = -I$(HDF5_INCDIR)

MOAB_DIR =  $(shell which mbconvert | sed 's/\/bin\/mbconvert//')
MOAB_INCDIR = $(MOAB_DIR)/include
MOAB_LIBDIR = $(MOAB_DIR)/lib
MOAB_INC = -I$(MOAB_INCDIR)
MOAB_LIB = -L$(MOAB_LIBDIR) -lMOAB

DAGMC_DIR = $(shell which make_watertight | sed 's/\/bin\/make_watertight//')
DAGMC_LIBDIR = $(DAGMC_DIR)/lib
DAGMC_INCDIR = $(DAGMC_DIR)/include
DAGMC_INC = -I$(DAGMC_INCDIR) $(MOAB_INC) $(HDF5_INC)
DAGMC_LIB = -L$(DAGMC_LIBDIR) $(MOAB_LIB) -ldagmc -lpyne_dagmc -luwuw -ldagtally -lmakeWatertight