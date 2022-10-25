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
    echo "i      Set the profile file to source (default: ${ENV_IN})"
    echo "o      Set the profile file to append (default: ${ENV_OUT})"
    echo
}

# Some defaults
WORKDIR=$HOME
DIRNAME="env"
ENV="moose"
ENV_IN=""
ENV_OUT="${HOME}/.${ENV}_profile"

while getopts "w:d:e:i:o:h" option; do
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
        i)
            ENV_IN=$OPTARG;;
        o)
            ENV_OUT=$OPTARG;;
        \?) # Invalid option
            echo "Error: Invalid option"
            exit 1;;
    esac
done

# Create the profile
if [ -f $ENV_OUT ]; then
   echo "File ${ENV_OUT} already exists!"
   exit 1
fi
touch ${ENV_OUT}
echo "Created ${ENV_OUT}"

# Source another environment first
if [ -n "$ENV_IN" ]; then
    echo "Sourcing environment from $ENV_IN"
    SRC_STR="source ${ENV_IN}"
    echo ${SRC_STR} >> ${ENV_OUT}
    echo ${SRC_STR}
    eval $SRC_STR
fi

# Set up environment directoy
ENVDIR="$WORKDIR/$DIRNAME"
if [ ! -d $ENVDIR ]; then
    mkdir $ENVDIR
fi

echo "Entering $ENVDIR"
cd $ENVDIR

# Create new python env
python -m venv $ENV
cd

# Update profile
SRC_STR="source $ENVDIR/$ENV/bin/activate"
echo ${SRC_STR} >> ${ENV_OUT}
echo "Updated ${ENV_OUT}"

# Start the virtual env
echo "Launching profile"
echo ${SRC_STR}
eval ${SRC_STR}

# Update pip
echo "Upgrading pip"
pip install --upgrade pip

# Packages for moose
echo "Installing python packages"
pip install packaging


