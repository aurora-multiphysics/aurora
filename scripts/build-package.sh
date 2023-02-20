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
    HDF5_DIR_IN=""

    # Read arguments
    local OPTIND # This line is crucial for this to work as a function
    while getopts "w:i:b:j:d:e:o:h" option; do
        case $option in
            h)
                Help
                exit;;
            w)
                WORKDIR=$(realpath $OPTARG);;
            i)
                INSTALLDIR=$(realpath $OPTARG);;
            b)
                BUILDDIDR=$(realpath $OPTARG);;
            j)
                JOBS=$OPTARG;;
            d)
                HDF5_DIR_IN=$OPTARG;;
            e)
                ENV_FILE_LIST=$(for i in `echo $OPTARG`; do printf $(realpath $i),; done);;
            o)
                ENV_OUTDIR=$(realpath $OPTARG);;
            \?) # Invalid option
                echo "Error: Invalid option"
                exit 1;;
        esac
    done

    if [ -n "$HDF5_DIR_IN" ]; then
       echo "Setting HDF5 dir to: $HDF5_DIR_IN"
       export "HDF5_DIR=$HDF5_DIR_IN"
    fi
}

Help()
{
    # Display Help
    echo
    echo "Syntax: build_PACKAGE [options]"
    echo "options:"
    echo "h      Print this help."
    echo "w      Set working directory"
    echo "i      Set installation directory, i.e. installs to  INSTALLDIR/$PACKAGE (default: $INSTALLDIR)"
    echo "j      Set number of build jobs  (default: $JOBS)"
    echo "d      Provide HDF5 installation directory"
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

    # Check package-specific environment variables are set
    package_check

    # Enter workdir
    if [ ! -d "$WORKDIR" ] ; then
        echo "$WORKDIR is not a directory!"
        echo "Please set working directory through the -w flag"
        Help
        exit 1
    fi
    cd $WORKDIR

    # Place to build
    if [ ! -d ${PACKAGE_DIR} ]; then
        mkdir ${PACKAGE_DIR}
    fi
    cd ${PACKAGE_DIR}
    echo "Building $PACKAGE in $WORKDIR/${PACKAGE_DIR}"

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

    # Process additional CMake flags
    CMAKE_STR=""
    for CMAKE_FLAG in ${ADDITIONAL_CMAKE_FLAGS[@]}; do
        CMD="FLAG=${CMAKE_FLAG}"
        eval $CMD
        CMAKE_STR="$CMAKE_STR $FLAG"
    done
    echo

    # Configure package
    echo "Running cmake in $BUILDDIR"
    CMAKE_CMD="cmake ../${REPO_NAME}/ -DCMAKE_INSTALL_PREFIX=${PACKAGE_INSTALL_DIR}
         ${CMAKE_STR}"

    echo ${CMAKE_CMD}
    eval ${CMAKE_CMD}

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

    # Check package-specific environment variables are set
    package_check

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


    # Process additional flags
    CONFIG_STR=""
    for CONFIG_FLAG in ${ADDITIONAL_CONFIG_FLAGS[@]}; do
        CMD="FLAG=${CONFIG_FLAG}"
        eval $CMD
        CONFIG_STR="$CONFIG_STR $FLAG"
    done

    # Configure package
    echo "Running configure in $BUILDDIR"

    CONFIG_CMD="../${REPO_NAME}/configure --prefix=${PACKAGE_INSTALL_DIR} ${CONFIG_STR}"

    echo ${CONFIG_CMD}
    eval ${CONFIG_CMD}

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

set_base_profile()
{
    for ENV_FILE in $(printf $ENV_FILE_LIST | xargs -d ',' -n1); do
        if [ ! -z ${BASE_PROFILE} ]; then
            echo "BASE_PROFILE env was already set to ${BASE_PROFILE}"
            echo "Please only pass one base profile"
            exit 1
        fi
        BASE_PROFILE=${ENV_FILE}
        echo "BASE_PROFILE=${BASE_PROFILE}"
    done
}

create_profile()
{
    if [ ! -d "${ENV_OUTDIR}" ]; then
        mkdir -p ${ENV_OUTDIR}
    fi

    echo "Profiles will be written to ${ENV_OUTDIR}"
    ENV_OUTFILE="${ENV_OUTDIR}/${PACKAGE}_profile"
    if [ -f $ENV_OUTFILE ]; then
        echo "File $ENV_OUTFILE already exists!"
        exit 1
    fi

    # Create a profile to source
    echo "Creating profile in ${ENV_OUTFILE}"
    touch $ENV_OUTFILE
    for ENV_FILE in $(printf -- $ENV_FILE_LIST | xargs -d ',' -n1); do
        echo "Sourcing environment from $ENV_FILE"
        source $ENV_FILE
        SRC_STR="source $ENV_FILE"
        echo $SRC_STR >>  $ENV_OUTFILE
    done
		if [ -n "${ENV_NAME}" ]; then
				echo "export ${ENV_NAME}_DIR=${PACKAGE_INSTALL_DIR}" >> $ENV_OUTFILE
		fi
    echo "export PATH=\${PATH}:${PACKAGE_INSTALL_DIR}/bin" >> $ENV_OUTFILE
    echo "export LD_LIBRARY_PATH=\${LD_LIBRARY_PATH}:${PACKAGE_INSTALL_DIR}/lib" >> $ENV_OUTFILE
}


package_check()
{
    if [ "$PACKAGE" = "moab" ]; then
       moab_check
    elif [ "$PACKAGE" = "embree" ]; then
       embree_check
    elif [ "$PACKAGE" = "double-down" ]; then
       dd_check
    elif [ "$PACKAGE" = "dagmc" ]; then
       dagmc_check
    fi
}

moab_check()
{
    if [ -z ${HDF5_DIR} ]; then
        echo "Please ensure HDF5_DIR is set in your profile"
        exit 1
    fi

    echo "Using HDF5_DIR=$HDF5_DIR"
}

embree_check()
{
    if [ -z ${CC} ]; then
        echo "Please ensure CC is set in your profile"
        exit 1
    elif [ -z ${CXX} ]; then
        echo "Please ensure CXX is set in your profile"
        exit 1
    fi

    echo "Using CC=$CC"
    echo "Using CXX=$CXX"
}

dd_check()
{
    if [ -z ${MOAB_DIR} ]; then
        echo "Please ensure MOAB_DIR is set in your profile"
        exit 1
    elif [ -z ${EMBREE_DIR} ]; then
        echo "Please ensure EMBREE_DIR is set in your profile"
        exit 1
    fi

    echo "Using MOAB_DIR=$MOAB_DIR"
    echo "Using EMBREE_DIR=$EMBREE_DIR"
}

dagmc_check()
{
    if [ -z ${MOAB_DIR} ]; then
        echo "Please ensure MOAB_DIR is set in your profile"
        exit 1
    elif [ -z ${DOUBLEDOWN_DIR} ]; then
        echo "Please ensure DOUBLEDOWN_DIR is set in your profile"
        exit 1
    fi

    echo "Using MOAB_DIR=$MOAB_DIR"
    echo "Using DOUBLEDOWN_DIR=$DOUBLEDOWN_DIR"
}

openmc_check()
{
    if [ -z ${DAGMC_DIR} ]; then
        echo "Please ensure DAGMC_DIR is set in your profile"
        exit 1
    fi

    echo "Using DAGMC_DIR=$DAGMC_DIR"
}

build_moab()
{
    process_args $*

    PACKAGE=moab
    REPO_NAME=moab
    ENV_NAME=MOAB
    PACKAGE_REPO=https://bitbucket.org/fathomteam/moab
    TAG=Version5.2.0
    ADDITIONAL_CONFIG_FLAGS=("--with-hdf5=\${HDF5_DIR}" "--enable-optimize" "--enable-shared" "--disable-debug")
    RUN_TEST_CMD="make check"

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
    ADDITIONAL_CMAKE_FLAGS=("-DCMAKE_CXX_COMPILER=\$CXX" "-DCMAKE_C_COMPILER=\$CC" "-DEMBREE_ISPC_SUPPORT=0" "-DEMBREE_TUTORIALS=OFF")
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
    ADDITIONAL_CMAKE_FLAGS=("-DMOAB_DIR=\${MOAB_DIR}" "-DEMBREE_DIR=\${EMBREE_DIR}")
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
    ADDITIONAL_CMAKE_FLAGS=("-DMOAB_DIR=\${MOAB_DIR}" "-DDOUBLE_DOWN=on" "-DDOUBLE_DOWN_DIR=\${DOUBLEDOWN_DIR}" "-DBUILD_TALLY=ON")
    RUN_TEST_CMD="make test"

    build_cmake_package
}

build_njoy()
{
    process_args $*
    PACKAGE=njoy
    ENV_NAME=
    REPO_NAME=NJOY2016
    PACKAGE_REPO=https://github.com/njoy/NJOY2016.git
    TAG=
    ADDITIONAL_CMAKE_FLAGS=("-Dstatic=on")
    RUN_TEST_CMD=""
    build_cmake_package
}

build_openmc()
{
    process_args $*

    PACKAGE=openmc
    REPO_NAME=openmc
		ENV_NAME=
    PACKAGE_REPO=https://github.com/openmc-dev/openmc.git
    TAG=a21174e4f968c07ab791c7b343dc0e07a7ab28b3
    ADDITIONAL_CMAKE_FLAGS=("-DCMAKE_BUILD_TYPE=Release" "-DOPENMC_USE_DAGMC=on" "-DDAGMC_DIR=\${DAGMC_DIR}/lib/cmake")
    RUN_TEST_CMD=""

    build_cmake_package
}

build_aurora_deps()
{
    process_args $*

    set_base_profile

    # Build MOAB
    build_moab -w $WORKDIR \
               -j $JOBS \
               -i $INSTALLDIR  \
               -e ${BASE_PROFILE} \
               -o ${ENV_OUTDIR}

    # Build Embree
    build_embree -w $WORKDIR \
                 -j $JOBS \
                 -i $INSTALLDIR  \
                 -e ${BASE_PROFILE} \
                 -o ${ENV_OUTDIR}

    # Build DoubleDown
    build_dd -w $WORKDIR \
             -j $JOBS \
             -i $INSTALLDIR  \
             -e "${ENV_OUTDIR}/moab_profile,${ENV_OUTDIR}/embree_profile" \
             -o ${ENV_OUTDIR}

    # Build DagMC
    build_dagmc -w  $WORKDIR \
                -j $JOBS \
                -i $INSTALLDIR \
                -e "${ENV_OUTDIR}/double-down_profile" \
                -o ${ENV_OUTDIR}

    # Build NJOY
    build_njoy -w $WORKDIR \
               -j $JOBS \
               -i $INSTALLDIR  \
               -e ${BASE_PROFILE} \
               -o ${ENV_OUTDIR}

    # Build OpenMC
    build_openmc -w $WORKDIR \
                 -j $JOBS \
                 -i $INSTALLDIR  \
                 -e "${ENV_OUTDIR}/njoy_profile,${ENV_OUTDIR}/dagmc_profile" \
                 -o ${ENV_OUTDIR}
}
