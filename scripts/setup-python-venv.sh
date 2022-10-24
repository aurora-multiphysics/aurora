#!/bin/bash

set -e

Help()
{
    # Display Help
    echo
    echo "Syntax: $0 [options]"
    echo "options:"
    echo "h      Print this help."
    echo "w      Full path to working directory (default: $WORKDIR)"
    echo "d      Set the name of the sub-directory in which to install the env (default: $DIRNAME)"
    echo "e      Set the environment name (default: $ENV)"
    echo "f      Set the profile file to append (default: ${ENV_FILE})"
    echo
}

# Some defaults
WORKDIR=$HOME
DIRNAME="env"
ENV="moose"
ENV_FILE="${HOME}/.${ENV}_profile"

while getopts "w:d:e:f:h" option; do
    case $option in
        h)
            Help
            exit;;
        w)
            WORKDIR=$OPTARG;;
        d)
            DIRNAME=$OPTARG;;
        e)
            ENV=$OPTARG;;
        f)
            ENV_FILE=$OPTARG;;
        \?) # Invalid option
            echo "Error: Invalid option"
            exit 1;;
    esac
done

# Set up directoy
ENVDIR="$WORKDIR/$DIRNAME"
if [ ! -d $ENVDIR ]; then
    mkdir $ENVDIR
fi

echo "Entering $ENVDIR"
cd $ENVDIR

# Create new python env
python -m venv $ENV
cd

# Update the profile
echo "source $ENVDIR/$ENV/bin/activate" >> ${ENV_FILE}
echo "Updated ${ENV_FILE}"

# Start the virtual env
echo "Launching profile"
source ${ENV_FILE}

# Update pip
echo "Upgrading pip"
pip install --upgrade pip

# Packages for moose
echo "Installing python packages"
pip install packaging
#pip install hit


