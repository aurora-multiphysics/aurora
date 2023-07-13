#!/bin/bash

set -e

process_args()
{
    ENV_FILE=""
		TOKEN=
		BRANCH=
		
    # Read arguments
    local OPTIND # This line is crucial for this to work as a function
    while getopts "e:t:b:h" option; do
        case $option in
            h)
                Help
                exit;;
            e)
                ENV_FILE=$OPTARG;;
						t)
								TOKEN=$OPTARG;;
						b)
								BRANCH=$OPTARG;;
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
    echo "t      Coveralls token"
    echo "b      Coveralls branch"
    echo
}


coverage_report()
{
    process_args $*

		if [ ! -z ${ENV_FILE} ]; then
				echo "Sourcing environment from $ENV_FILE"
				source $ENV_FILE
		fi

		echo "Generating coverage report."
		
		if [ $TOKEN ]; then
				echo "Uploading test coverage to coveralls"
        export COVERALLS_REPO_TOKEN=$TOKEN
				if [ $BRANCH ]; then
						export CI_BRANCH=$BRANCH
				fi				        
        cpp-coveralls -i src/ -i include/ -i openmc/src/ -i openmc/include/ --exclude-pattern "/usr/*" --dump cpp_cov.json
				coveralls --merge=cpp_cov.json
		else
				echo "No coveralls token, using govr"
        gcovr -e src/main.C -e openmc/unit/ -e unit/ -e openmc/src/main.C -e test -e openmc/test build/ openmc/build/
		fi

}
		
coverage_report $*
