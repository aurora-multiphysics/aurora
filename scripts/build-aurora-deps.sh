#!/bin/bash

set -e

HDF5LIBDIR=/usr/lib/x86_64-linux-gnu/hdf5/serial/
BASE_PROFILE=/home/helen/.parallelinit
WORKDIR=/home/helen/Projects/MooseApps/aurora/tmp/work
INSTALLDIR=/home/helen/Projects/MooseApps/aurora/tmp/install
ENVDIR=/home/helen/Projects/MooseApps/aurora/tmp/env
WORKDIR=/home/helen/Projects/MooseApps/aurora/tmp/work
JOBS=4

./build-moab.sh -w $WORKDIR \
    -j $JOBS \
    -i $INSTALLDIR  \
    -e ${BASE_PROFILE} \
    -o $ENVDIR
