###############################################################################
################### MOOSE Application Standard Makefile #######################
###############################################################################
#
# Optional Environment variables
# MOOSE_DIR        - Root directory of the MOOSE project
#
###############################################################################
AURORA_DIR          := $(abspath $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))
CONTRIB_DIR         := $(AURORA_DIR)/contrib
MOOSE_SUBMODULE     ?= $(CONTRIB_DIR)/moose

# Use the MOOSE submodule if it exists and MOOSE_DIR is not set
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
HEAT_CONDUCTION             := yes
LEVEL_SET                   := no
MISC                        := yes
NAVIER_STOKES               := no
PHASE_FIELD                 := no
POROUS_FLOW                 := no
RDG                         := no
RICHARDS                    := no
SOLID_MECHANICS             := no
STOCHASTIC_TOOLS            := no
TENSOR_MECHANICS            := yes
XFEM                        := no

include $(MOOSE_DIR)/modules/modules.mk
###############################################################################

OPENMC_APP_NAME = open_mc
OPENMC_APP_LIB_NAME = $(OPENMC_APP_NAME)-$(METHOD)
OPENMC_APP_DIR = $(CURDIR)/openmc
OPENMC_APP_INC = -I$(OPENMC_APP_DIR)/include
OPENMC_APP_LIB = -Wl,-rpath,$(OPENMC_APP_DIR)/lib -L$(OPENMC_APP_DIR)/lib -l$(OPENMC_APP_LIB_NAME)

include $(OPENMC_APP_DIR)/config.inc

ADDITIONAL_INCLUDES += $(OPENMC_APP_INC) $(OPENMC_INC) $(MOAB_INC) $(DAGMC_INC)
EXTERNAL_FLAGS += $(OPENMC_APP_LIB) $(OPENMC_LIB) $(MOAB_LIB) $(DAGMC_LIB)

# dep apps
APPLICATION_DIR    := $(CURDIR)
APPLICATION_NAME   := aurora
BUILD_EXEC         := yes
GEN_REVISION       := no
include            $(FRAMEWORK_DIR)/app.mk

###############################################################################
# Additional special case targets should be added here
