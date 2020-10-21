###############################################################################
################### MOOSE Application Standard Makefile #######################
###############################################################################
#
# Optional Environment variables
# MOOSE_DIR        - Root directory of the MOOSE project
#
###############################################################################
# Use the MOOSE submodule if it exists and MOOSE_DIR is not set
MOOSE_SUBMODULE    := $(CURDIR)/moose
ifneq ($(wildcard $(MOOSE_SUBMODULE)/framework/Makefile),)
  MOOSE_DIR        ?= $(MOOSE_SUBMODULE)
else
  MOOSE_DIR        ?= $(shell dirname `pwd`)/moose
endif

# framework
FRAMEWORK_DIR      := $(MOOSE_DIR)/framework
include $(FRAMEWORK_DIR)/build.mk
include $(FRAMEWORK_DIR)/moose.mk

################################## MODULES ####################################
# To use certain physics included with MOOSE, set variables below to
# yes as needed.  Or set ALL_MODULES to yes to turn on everything (overrides
# other set variables).

ALL_MODULES                 := no

CHEMICAL_REACTIONS          := no
CONTACT                     := no
EXTERNAL_PETSC_SOLVER       := no
FLUID_PROPERTIES            := no
FUNCTIONAL_EXPANSION_TOOLS  := no
HEAT_CONDUCTION             := no
LEVEL_SET                   := no
MISC                        := no
NAVIER_STOKES               := no
PHASE_FIELD                 := no
POROUS_FLOW                 := no
RDG                         := no
RICHARDS                    := no
SOLID_MECHANICS             := no
STOCHASTIC_TOOLS            := no
TENSOR_MECHANICS            := no
XFEM                        := no

include $(MOOSE_DIR)/modules/modules.mk
###############################################################################

OPENMC_DIR = $(shell which openmc | sed 's/\/bin\/openmc//')
OPENMC_INC = -I$(OPENMC_DIR)/include
OPENMC_LIB =-Wl,-rpath,$(OPENMC_DIR)/lib -L$(OPENMC_DIR)/lib -lopenmc

# TODO set dynamically
HDF5_INC = -I/usr/include/hdf5/serial
HDF5_LIB = -L/usr/lib/x86_64-linux-gnu/hdf5/serial/ -lhdf5_hl

DAGMC_INC = -I/home/helen/local/dagmc/include -I/home/helen/local/moab/include -I/usr/include/eigen3
DAGMC_LIB = -L/home/helen/local/dagmc/lib -L/home/helen/local/moab/lib -lMOAB -ldagmc -lpyne_dagmc -luwuw -ldagtally -lmakeWatertight

ADDITIONAL_INCLUDES += $(OPENMC_INC) $(HDF5_INC) $(DAGMC_INC)
EXTERNAL_FLAGS += $(OPENMC_LIB) $(HDF5_LIB) $(DAGMC_LIB)
#ADDITIONAL_CPPFLAGS += 

# dep apps
APPLICATION_DIR    := $(CURDIR)
APPLICATION_NAME   := open_mc
BUILD_EXEC         := yes
GEN_REVISION       := no
include            $(FRAMEWORK_DIR)/app.mk

###############################################################################
# Additional special case targets should be added here
