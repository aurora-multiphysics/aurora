#!/bin/bash

set -e

process_args()
{
    # Defaults
    INSTALLDIR=$HOME/local
    WORKDIR=.
    BUILDDIR=bld
    JOBS=1
    ENV_FILE_LIST=
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
                ENV_FILE_LIST=$OPTARG;;
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


build_autotools_package()
{
    # Package setup
    ORIGDIR=$PWD
    PACKAGE_DIR="$PACKAGE-bld"
    PACKAGE_INSTALL_DIR="$INSTALLDIR/$PACKAGE"

    # Create a profile to source later
    create_profile

    # Place to build
    echo "Building $PACKAGE in ${PACKAGE_DIR}"
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

    autoreconf -fi

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
    echo "Running configure in $BUILDDIR"
    ../${REPO_NAME}/configure \
          --prefix=${PACKAGE_INSTALL_DIR} \
          $ADDITIONAL_CONFIG_FLAGS

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

    # Create a profile to source
    echo "Creating profile in ${ENV_OUTFILE}"
    touch $ENV_OUTFILE    
    for ENV_FILE in $(printf $ENV_FILE_LIST | xargs -d ',' -n1); do
        echo "Sourcing environment from $ENV_FILE"
        source $ENV_FILE
        SRC_STR="source $ENV_FILE"
        echo $SRC_STR >>  $ENV_OUTFILE
    done 
    echo "export ${ENV_NAME}_DIR=${PACKAGE_INSTALL_DIR}" >> $ENV_OUTFILE    
    echo "export PATH=\${PATH}:${PACKAGE_INSTALL_DIR}/bin" >> $ENV_OUTFILE
    echo "export LD_LIBRARY_PATH=\${LD_LIBRARY_PATH}:${PACKAGE_INSTALL_DIR}/lib" >> $ENV_OUTFILE    
}

build_moab()
{
    process_args $*

    PACKAGE=moab
    REPO_NAME=moab
    ENV_NAME=MOAB
    PACKAGE_REPO=https://bitbucket.org/fathomteam/moab
    TAG=Version5.2.0
    ADDITIONAL_CONFIG_FLAGS="--with-hdf5=${HDF5LIBDIR} --enable-optimize --enable-shared --disable-debug"
    RUN_TEST_CMD="make check"
    PROFILE_STR='export EMBREE_DIR=${PACKAGE_INSTALL_DIR}'

    build_autotools_package
}


build_embree()
{
    process_args $*
    PACKAGE=embree
    REPO_NAME=embree
    ENV_NAME=EMBREE
    PACKAGE_REPO=https://github.com/embree/embree.git
    TAG=v3.6.1
    ADDITIONAL_CMAKE_FLAGS="-DCMAKE_CXX_COMPILER=$CXX -DCMAKE_C_COMPILER=$CC -DEMBREE_ISPC_SUPPORT=0"
    RUN_TEST_CMD=""
    
    build_cmake_package
}

build_dd()
{
    process_args $*
    PACKAGE=double-down
    REPO_NAME=double-down
    ENV_NAME=DOUBLEDOWN
    PACKAGE_REPO=https://github.com/pshriwise/double-down
    TAG=v1.0.0
    ADDITIONAL_CMAKE_FLAGS="-DMOAB_DIR=${MOAB_DIR} -DEMBREE_DIR=${EMBREE_DIR}"
    RUN_TEST_CMD=""
    
    build_cmake_package
}

build_dagmc()
{
    process_args $*
    PACKAGE=dagmc
    REPO_NAME=DAGMC
    ENV_NAME=DAGMC
    PACKAGE_REPO=https://github.com/svalinn/DAGMC
    TAG=0d07a744178af6275959c745fa4362d8b4d13559
    ADDITIONAL_CMAKE_FLAGS="-DMOAB_DIR=${MOAB_DIR} -DDOUBLE_DOWN=on -DDOUBLE_DOWN_DIR=${DOUBLEDOWN_DIR} -DBUILD_TALLY=ON"
    RUN_TEST_CMD="make test"

    build_cmake_package
}

build_njoy()
{
    process_args $*
    PACKAGE=njoy
    REPO_NAME=NJOY2016
    PACKAGE_REPO=https://github.com/njoy/NJOY2016.git
    TAG=
    ADDITIONAL_CMAKE_FLAGS="-Dstatic=on"
    RUN_TEST_CMD=""
    build_cmake_package
}

build_openmc()
{
    process_args $*
    PACKAGE=openmc
    REPO_NAME=openmc
    PACKAGE_REPO=https://github.com/openmc-dev/openmc.git
    TAG=a21174e4f968c07ab791c7b343dc0e07a7ab28b3
    ADDITIONAL_CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release -DOPENMC_USE_DAGMC=on -DCMAKE -DDAGMC_DIR=${DAGMC_DIR}/lib/cmake"
    RUN_TEST_CMD=""

    build_cmake_package
}
