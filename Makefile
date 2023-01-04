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

HEAT_CONDUCTION             := yes
MISC                        := yes
TENSOR_MECHANICS            := yes

include $(MOOSE_DIR)/modules/modules.mk
###############################################################################

include config.inc

ADDITIONAL_INCLUDES += $(OPENMC_INC) $(MOAB_INC) $(DAGMC_INC)
EXTERNAL_FLAGS += $(OPENMC_LIB) $(MOAB_LIB) $(DAGMC_LIB)

# dep apps
APPLICATION_DIR    := $(CURDIR)
APPLICATION_NAME   := aurora
BUILD_EXEC         := yes
GEN_REVISION       := no
include            $(FRAMEWORK_DIR)/app.mk

###############################################################################
# Additional special case targets should be added here
