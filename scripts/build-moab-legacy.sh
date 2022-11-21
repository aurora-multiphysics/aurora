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
    echo "t      Set moab tag check out  (default: $MOABTAG)"
    echo "d      Provide HDF5 library installation directory (default: $HDF5LIBDIR)"
    echo "j      Set number of build jobs  (default: $JOBS)"
    echo "e      Set file from which to source environment"
    echo "o      Set file in which to create MOAB profile (default:${ENV_OUTFILE} )"
    echo
}

build_moab()
{
    # Package setup
    ORIGDIR=$PWD
    PACKAGE_DIR="$PACKAGE-bld"
    PACKAGE_INSTALL_DIR="$INSTALLDIR/$PACKAGE"
    
    # Place to build
    echo "Building MOAB in $WORKDIR/${PACKAGE_DIR}"
    cd $WORKDIR    
    if [ ! -d ${PACKAGE_DIR} ]; then
        mkdir ${PACKAGE_DIR}
    fi
    cd ${PACKAGE_DIR}
    
    # Get the source code
    if [ ! -d $PACKAGE  ]; then
        git clone https://bitbucket.org/fathomteam/moab
        cd $PACKAGE
    else
        cd $PACKAGE
        git clean -xfd
    fi
    git checkout $MOABTAG
    autoreconf -fi
    cd ../
    
    # Create clean build directory
    if [ -d $BUILDDIR]; then
        rm -r $BUILDDIR
    fi
    mkdir $BUILDDIR
    cd $BUILDDIR

    # Configure
    echo "Running configure in $BUILDDIR"
    ../moab/configure --prefix=${PACKAGE_INSTALL_DIR} \
                      --with-hdf5=$HDF5LIBDIR \
                      --enable-optimize \
                      --enable-shared \
                      --disable-debug
    # Build
    make -j$JOBS
    make check
    make install
    
    # Return to starting directory
    cd $ORIGDIR
}

# Defaults 
PACKAGE=moab
INSTALLDIR=$HOME/local
WORKDIR=""
MOABTAG=Version5.2.0
BUILDDIR=bld
HDF5LIBDIR=/usr/lib/x86_64-linux-gnu/hdf5/serial/
JOBS=1
ENV_FILE=
ENV_OUTFILE="$HOME/.moabprofile"

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
        d)
            HDF5LIBDIR=$OPTARG;;
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

if [ -f $ENV_OUTFILE ]; then
   echo "File $ENV_OUTFILE already exists!"
   exit 1
fi

# Create a moab profile
echo "Creating MOAB profile in ${ENV_OUTFILE}"
touch $ENV_OUTFILE
if [ -n "$SRC_STR" ]; then
    echo $SRC_STR >>  $ENV_OUTFILE
fi

build_moab

echo "MOAB_DIR=${PACKAGE_INSTALL_DIR}" >>  $ENV_OUTFILE
