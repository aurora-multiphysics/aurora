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
    echo "i      Set installation directory, i.e. installs to  INSTALLDIR/$PACKAGE (default: $INSTALLDIR)"
    echo "t      Set tag check out  (default: $TAG)"
    echo "j      Set number of build jobs  (default: $JOBS)"
    echo "e      Set file from which to source environment"
    echo "o      Set file in which to create output profile (default:${ENV_OUTFILE} )"
    echo
}

setup_env()
{
if [ "$WORKDIR" = "" ] ; then
    echo "Please set working directory through the -w flag"
    Help
    exit 1
fi
SRC_STR=""
if [ -n "$ENV_FILE" ]; then
    echo "Sourcing environment from $ENV_FILE"
    source $ENV_FILE
    SRC_STR="source $ENV_FILE"
fi

#if [ -f $ENV_OUTFILE ]; then
#   echo "File $ENV_OUTFILE already exists!"
#   exit 1
#fi

## Create a profile
#echo "Creating profile in ${ENV_OUTFILE}"
#touch $ENV_OUTFILE
#if [ -n "$SRC_STR" ]; then
#    echo $SRC_STR >>  $ENV_OUTFILE
#fi
}

build_package()
{
    # Package setup
    ORIGDIR=$PWD
    PACKAGE_DIR="$PACKAGE-bld"
    PACKAGE_INSTALL_DIR="$INSTALLDIR/$PACKAGE"

    # Place to build
    echo "Building $PACKAGE in $WORKDIR/${PACKAGE_DIR}"
    cd $WORKDIR
    if [ ! -d ${PACKAGE_DIR} ]; then
        mkdir ${PACKAGE_DIR}
    fi
    cd ${PACKAGE_DIR}

    # Get the source code
    if [ ! -d ${REPO_NAME}  ]; then
        git clone ${PACKAGE_REPO}
        cd ${REPO_NAME}
    else
        cd ${REPO_NAME}
        git clean -xfd
    fi
    git checkout $TAG
    cd ../

    # Create clean build directory
    if [ -d $BUILDDIR]; then
        rm -r $BUILDDIR
    fi
    mkdir $BUILDDIR
    cd $BUILDDIR

    # Configure
    echo "Running cmake in $BUILDDIR"
    cmake ../${REPO_NAME}/ \
          -DCMAKE_INSTALL_PREFIX=${PACKAGE_INSTALL_DIR} \
          $ADDITIONAL_CMAKE_FLAGS

    # Build
    make -j$JOBS

    # Test
    if [ -n "${RUN_TEST_CMD}" ]; then
        eval $RUN_TEST_CMD
    fi

    # Install
    make install

    # Return to starting directory
    cd $ORIGDIR
}

# Defaults
INSTALLDIR=$HOME/local
WORKDIR=""
TAG=v3.6.1
BUILDDIR=bld
JOBS=1
ENV_FILE=
ENV_OUTFILE="$HOME/.embreeprofile"

# Read arguments
while getopts "w:i:t:d:j:e:o:h" option; do
    case $option in
        h)
            Help
            exit;;
        w)
            WORKDIR=$OPTARG;;
        i)
            INSTALLDIR=$OPTARG;;
        t)
            TAG=$OPTARG;;
        j)
            JOBS=$OPTARG;;
        e)
            ENV_FILE=$OPTARG;;
        o)
            ENV_OUTFILE=$OPTARG;;
        \?) # Invalid option
            echo "Error: Invalid option"
            exit 1;;
    esac
done

PACKAGE=embree
REPO_NAME=embree
PACKAGE_REPO=https://github.com/embree/embree.git
ADDITIONAL_CMAKE_FLAGS="-DCMAKE_CXX_COMPILER=$CXX -DCMAKE_C_COMPILER=$CC -DEMBREE_ISPC_SUPPORT=0"
RUN_TEST_CMD=""

setup_env
build_package
