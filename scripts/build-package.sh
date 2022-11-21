#!/bin/bash

set -e

process_args()
{
    # Defaults
    INSTALLDIR=$HOME/local
    WORKDIR=.
    BUILDDIR=bld
    JOBS=1
    ENV_FILE=
    ENV_OUTDIR=$HOME

    # Read arguments
    while getopts "w:i:b:j:e:o:h" option; do
        case $option in
            h)
                Help
                exit;;
            w)
                WORKDIR=$OPTARG;;
            i)
                INSTALLDIR=$OPTARG;;
            b)
                BUILDDIDR=$OPTARG;;
            j)
                JOBS=$OPTARG;;
            e)
                ENV_FILE=$OPTARG;;
            o)
                ENV_OUTDIR=$OPTARG;;
            \?) # Invalid option
                echo "Error: Invalid option"
                exit 1;;
        esac
    done

    # Enter workdir
    if [ ! -d "$WORKDIR" ] ; then
        echo "$WORKDIR is not a directory!"
        echo "Please set working directory through the -w flag"
        Help
        exit 1
    fi

    echo "Building packages in $WORKDIR"
    cd $WORKDIR
}

Help()
{
    # Display Help
    echo
    echo "Syntax: $0 [options]"
    echo "options:"
    echo "h      Print this help."
    echo "w      Set working directory"
    echo "i      Set installation directory, i.e. installs to  INSTALLDIR/$PACKAGE (default: $INSTALLDIR)"
    echo "j      Set number of build jobs  (default: $JOBS)"
    echo "e      Set file from which to source environment"
    echo "o      Set output directory to create profiles (default:${ENV_OUTDIR} )"
    echo
}

build_cmake_package()
{
    # Package setup
    ORIGDIR=$PWD
    PACKAGE_DIR="$PACKAGE-bld"
    PACKAGE_INSTALL_DIR="$INSTALLDIR/$PACKAGE"

    # Create a profile to source later
    create_profile

    # Place to build
    echo "Building $PACKAGE in $WORKDIR/${PACKAGE_DIR}"
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

    # Checkout a specific TAG
    if [ ! -z $TAG ]; then
        git checkout $TAG
    fi

    # Return to package dir
    cd ../

    # Create clean build directory
    if [ -d $BUILDDIR ]; then
        rm -r $BUILDDIR
    fi
    mkdir $BUILDDIR

    # Enter build directory
    cd $BUILDDIR

    # Configure package
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


create_profile()
{
    SRC_STR=""
    if [ -n "$ENV_FILE" ]; then
        echo "Sourcing environment from $ENV_FILE"
        source $ENV_FILE
        SRC_STR="source $ENV_FILE"
    fi

    if [ ! -d "${ENV_OUTDIR}" ]; then
        mkdir -p ${ENV_OUTDIR}
    fi
    echo "Profiles will be written to ${ENV_OUTDIR}"

    ENV_OUTFILE="${ENV_OUTDIR}/.${PACKAGE}_profile"
    echo $ENV_OUTFILE
    if [ -f $ENV_OUTFILE ]; then
        echo "File $ENV_OUTFILE already exists!"
        exit 1
    fi

    # Create a profile to source in the next package
    echo "Creating profile in ${ENV_OUTFILE}"
    touch $ENV_OUTFILE
    if [ -n "$SRC_STR" ]; then
        echo $SRC_STR >>  $ENV_OUTFILE
    fi

    if [ -n "$PROFILE_STR" ]; then
        echo ${PROFILE_STR} >> $ENV_OUTFILE
    fi
}


build_embree(){
    process_args $*
    PACKAGE=embree
    REPO_NAME=embree
    PACKAGE_REPO=https://github.com/embree/embree.git
    TAG=v3.6.1
    ADDITIONAL_CMAKE_FLAGS="-DCMAKE_CXX_COMPILER=$CXX -DCMAKE_C_COMPILER=$CC -DEMBREE_ISPC_SUPPORT=0"
    RUN_TEST_CMD=""
    PROFILE_STR='export EMBREE_DIR=${PACKAGE_INSTALL_DIR}'

    build_cmake_package
}
