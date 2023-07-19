#!/bin/bash

set -e

Help()
{
    # Display Help
    echo
    echo "Syntax: $0 [options]"
    echo "options:"
    echo "h      Print this help."
    echo "w      Set working directory"
    echo "t      Set MOOSE github tag to check out"
    echo "b      Set MOOSE github branch to check out"
    echo "s      Set MOOSE github sha to check out"
    echo "j      Set number of MOOSE build jobs"
    echo "d      Provide HDF5 installation directory"
    echo "p      Set PETSC installation directory"
    echo "e      Set file from which to source environment"
    echo "o      Set file in which to create moose profile"
    echo "f      Set MOOSE configuration flags as a string"
    echo "r      Must supply this if user is running as root (e.g. in docker)"
    echo "c      Allow petsc to download cmake"
    echo
}

WORKDIR=
TAG=""
BRANCH=""
GITSHA=""
JOBS=1
HDF5_DIR_IN=""
HDF5_STR=""
PETSC_INSTALL_DIR=""
ENV_FILE=""
ENV_OUTFILE=""
ROOTUSER=false
DOWNLOAD_CMAKE=false
# Sensible default MOOSE configuration, but allow user to decide
CONFIG_FLAGS="--with-derivative-size=200 --with-ad-indexing-type=global"

# Read arguments
while getopts "w:t:b:s:j:d:p:e:o:f:hrc" option; do
    case $option in
        h)
            Help
            exit;;
        w)
            WORKDIR=$(realpath $OPTARG);;
        t)
            TAG=$OPTARG;;
        b)
            BRANCH=$OPTARG;;
        s)
            GITSHA=$OPTARG;;
        j)
            JOBS=$OPTARG;;
        d)
            HDF5_DIR_IN=$(realpath $OPTARG);;
        p)
            PETSC_INSTALL_DIR=$(realpath $OPTARG);;
        e)
            ENV_FILE=$(realpath $OPTARG);;
        o)
            ENV_OUTFILE=$(realpath $OPTARG);;
        f)
            CONFIG_FLAGS=$OPTARG;;
        r)
            ROOTUSER=true;;
        c)
            DOWNLOAD_CMAKE=true;;
        \?) # Invalid option
            echo "Error: Invalid option"
            exit 1;;
    esac
done

if [ "$WORKDIR" = "" ] ; then
    echo "Please set working directory through the -w flag"
    Help
    exit 1
fi

# Set up environment variables
export MOOSE_DIR=$WORKDIR/moose
export PETSC_ARCH=arch-moose
export MOOSE_JOBS=$JOBS

# Set build strings from user-provided arguments
PETSC_BUILD_DIR=$MOOSE_DIR/petsc
CMAKE_STR=""
if [ -n "$PETSC_INSTALL_DIR" ]; then
    echo "Using PETSC installation prefix $PETSC_INSTALL_DIR"
    PETSC_PREFIX_STRING="--prefix=$PETSC_INSTALL_DIR"
    export PETSC_DIR=$PETSC_INSTALL_DIR
else
    export PETSC_DIR=$MOOSE_DIR/petsc
fi
if [ -n "$HDF5_DIR_IN" ]; then
    echo "HDF5 installation location was set using HDF5_DIR=$HDF5_DIR_IN"
    HDF5_STR="--with-hdf5-dir=$HDF5_DIR_IN"
fi
if [ "$DOWNLOAD_CMAKE" = true ] ; then
    echo "User provided -c argument: PETSC will install cmake"
    CMAKE_STR="--download-cmake"
fi

SRC_STR=""
if [ -n "$ENV_FILE" ]; then
    echo "Sourcing environment from $ENV_FILE"
    source $ENV_FILE
    SRC_STR="source $ENV_FILE"
fi

if [ "$ENV_OUTFILE" = "" ]; then
    ENV_OUTFILE=$WORKDIR/.mooseprofile
    echo "No output file argument provided, defaulting to: $ENV_OUTFILE"
fi

if [ -f $ENV_OUTFILE ]; then
   echo "File $ENV_OUTFILE already exists!"
   exit 1
fi

# Create a MOOSE profile
touch $ENV_OUTFILE
if [ -n "$SRC_STR" ]; then
    echo $SRC_STR >>  $ENV_OUTFILE
fi
echo "export MOOSE_JOBS=$MOOSE_JOBS" >> $ENV_OUTFILE
echo "export MOOSE_DIR=$MOOSE_DIR" >> $ENV_OUTFILE
echo "export PETSC_DIR=$PETSC_DIR" >> $ENV_OUTFILE
echo "export PETSC_ARCH=$PETSC_ARCH" >> $ENV_OUTFILE

echo "Created MOOSE profile in $ENV_OUTFILE"
echo
cat $ENV_OUTFILE
echo

# Create working directory if non-existant
echo "Installing MOOSE in $WORKDIR"
if [ ! -d $WORKDIR ]; then
    mkdir $WORKDIR
fi
cd $WORKDIR

# Get MOOSE repository if not present
if [ ! -d ${MOOSE_DIR} ]; then
    git clone https://github.com/idaholab/moose.git
fi

# Fetch the right git sha / tag
cd moose
if ! [ "$TAG" = "" ] ; then
    git fetch --tags
    git checkout tags/$TAG;
elif ! [ "$BRANCH" = "" ] ; then
    git checkout $BRANCH;
elif ! [ "$GITSHA" = "" ] ; then
    git checkout $GITSHA;
else
    git checkout master;
fi
# Print git log for last commit
git log -1

# Get latest petsc
git submodule update --init --recursive petsc

# Configure PETSC ourselves because MOOSE script can't find HDF5 when on Fedora
cd ${PETSC_BUILD_DIR}
./configure "$PETSC_PREFIX_STRING" \
            --CC=$CC \
            --CXX=$CXX \
            --FC=$FC \
            --with-shared-libraries=1 \
            --with-hdf5=1 \
            "$HDF5_STR" \
            --with-make-np=$MOOSE_JOBS \
            --with-debugging=no \
            "$CMAKE_STR" \
            --download-hypre=1 \
            --download-fblaslapack=1 \
            --download-metis=1 \
            --download-ptscotch=1 \
            --download-parmetis=1 \
            --download-superlu_dist=1 \
            --download-mumps=1 \
            --download-strumpack=1 \
            --download-scalapack=1 \
            --download-slepc=1 \
            --with-mpi=1 \
            --with-openmp=1 \
            --with-cxx-dialect=C++11 \
            --with-fortran-bindings=0 \
            --with-sowing=0 \
            --with-64-bit-indices
# Build PETSC
make -j${MOOSE_JOBS}
# Install PETSC
if [ -n "$PETSC_INSTALL_DIR" ]; then
    make install
fi

# Build Libmesh
cd ${MOOSE_DIR}
METHODS="opt" ./scripts/update_and_rebuild_libmesh.sh --with-mpi --skip-submodule-update --enable-exodus-long-name

# Configure MOOSE
if [ -n "${CONFIG_FLAGS}" ]; then
    CONFIG_CMD="./configure ${CONFIG_FLAGS}"
    echo ${CONFIG_CMD}
    eval ${CONFIG_CMD}
fi

# Build MOOSE framework and tests
cd test
METHOD="opt" make -j${MOOSE_JOBS}

# Build MOOSE modules
cd ${MOOSE_DIR}/modules
METHOD="opt" make -j${MOOSE_JOBS}

# This is needed or it mpiexec complains because docker runs as root
# Discussion on this issue https://github.com/open-mpi/ompi/issues/4451
if [ "$ROOTUSER" = true ] ; then
    export OMPI_ALLOW_RUN_AS_ROOT=1
    export OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1
fi

# Run tests
cd ${MOOSE_DIR}/test
./run_tests -j${MOOSE_JOBS}
cd ${MOOSE_DIR}/modules
./run_tests -j${MOOSE_JOBS}

# Unset those OMPI environment variables we set before
if [ "$ROOTUSER" = true ] ; then
    export OMPI_ALLOW_RUN_AS_ROOT=
    export OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=
fi

echo "Succesfully built MOOSE!"
echo
echo "To set up your environment for using MOOSE, please run:"
echo "source $ENV_OUTFILE"
