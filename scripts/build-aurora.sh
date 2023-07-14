#!/bin/bash

set -e


process_args()
{
    JOBS=1
    ENV_FILE_LIST=
    ENV_OUTDIR=$HOME
    AURORADIR=$PWD
    
    # Read arguments
    local OPTIND # This line is crucial for this to work as a function
    while getopts "j:e:o:d:h" option; do
        case $option in
            h)
                Help
                exit;;
            j)
                JOBS=$OPTARG;;
            e)
                ENV_FILE_LIST=$OPTARG;;
            o)
                ENV_OUTDIR=$(realpath $OPTARG);;
            d)
                AURORADIR=$(realpath $OPTARG);;
            \?) # Invalid option
                echo "Error: Invalid option"
                exit 1;;
        esac
    done
}

Help()
{
    # Display Help
    echo
    echo "Syntax: build-aurora [options]"
    echo "options:"
    echo "h      Print this help."
    echo "j      Set number of build jobs  (default: $JOBS)"
    echo "e      Set file(s) from which to source environment"
    echo "o      Set output directory to create profiles (default:${ENV_OUTDIR} )"
    echo "d      Path to AURORA root directory  (default:${AURORADIR} )"
    echo
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
    for ENV_FILE in $(printf $ENV_FILE_LIST | xargs -d ',' -n1); do
        ENV_FILE=$(realpath ${ENV_FILE})
        echo "Sourcing environment from $ENV_FILE"
        source $ENV_FILE
        SRC_STR="source $ENV_FILE"
        echo $SRC_STR >>  $ENV_OUTFILE
    done

    XS_STR="export OPENMC_CROSS_SECTIONS=$AURORADIR/data/endfb71_hdf5/cross_sections.xml"
    eval ${XS_STR}
    echo ${XS_STR} >>  $ENV_OUTFILE
}

build_aurora()
{
    PACKAGE=aurora
    
    process_args $*
    
    create_profile
    
    echo "Building AURORA in $AURORADIR with $JOBS cores"
    
    cd $AURORADIR
    cd data
    tar xzvf endfb71_hdf5.tgz
    cd ../openmc/
    make -j$JOBS
    cd unit
    make -j$JOBS
    cd ../../
    make -j$JOBS
    cd unit
    make -j$JOBS
    ./run_tests
    cd ../
    ./run_tests
}

build_aurora $*
