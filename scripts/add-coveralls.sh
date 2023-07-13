#!/bin/bash

set -e

process_args()
{
    ENV_FILE=""

    # Read arguments
    local OPTIND # This line is crucial for this to work as a function
    while getopts "e:h" option; do
        case $option in
            h)
                Help
                exit;;
            e)
                ENV_FILE=$OPTARG;;
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
    echo "options:"
    echo "h      Print this help."
    echo "e      Set file from which to source environment"
    echo
}


install_coveralls()
{
    process_args $*

		echo ${ENV_FILE}
		
		if [ ! -z ${ENV_FILE} ]; then
				echo "Sourcing environment from $ENV_FILE"
				source $ENV_FILE
		fi
		
		echo "pip = $(which pip)"
		
		pip install cpp-coveralls
		pip install coveralls
    pip install gcovr
}
		
install_coveralls $*
