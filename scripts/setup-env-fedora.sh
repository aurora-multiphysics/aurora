#!/bin/bash

set -e

# Install Packages
dnf -y install cmake \
    git gcc gcc-c++ gcc-gfortran \
    hdf5-devel blas-devel lapack-devel \
    wget eigen3-devel \
    openmpi-devel mpich mpich-devel \
    autoconf automake libtool vim emacs \
    bison flex bison-devel flex-devel \
    python3 python3-devel libtirpc libtirpc-devel \
    rsync tbb-devel glfw-devel
