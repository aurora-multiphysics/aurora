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
    echo "t      Set MOOSE github tag to check out"
    echo "b      Set MOOSE github branch to check out"
    echo "s      Set MOOSE github sha to check out"
    echo
}

WORKDIR=
TAG=""
BRANCH=""
GITSHA=""

# Read arguments
while getopts "w:t:b:s:h" option; do
    case $option in    
        h)
            Help
            exit;;
        w)
            WORKDIR=$OPTARG;;    
        t)
            TAG=$OPTARG;;
        b)
            BRANCH=$OPTARG;;
        s)
            GITSHA=$OPTARG;;
        \?) # Invalid option
            echo "Error: Invalid option"
            exit;;
    esac
done

if [ "$WORKDIR" = "" ] ; then
    echo "Please set working directory through the -w flag"
    Help
    exit
fi

# Create working directory if non-existant
echo "Installing MOOSE in $WORKDIR"
if [ ! -d $WORKDIR ]; then
    mkdir $WORKDIR
fi
cd $WORKDIR

# Get MOOSE repository if not present
MOOSE_DIR=$WORKDIR/moose
if [ ! -d ${MOOSE_DIR} ]; then
    git clone https://github.com/idaholab/moose.git
fi

# Fetch the right git sha / tag
cd moose
if ! [ "$TAG" = "" ] ; then
    git fetch --tags
    git checkout tags/$TAG;
elif ! [ "$BRANCH" = "" ] ; then
    git checkout $BRANCH;
elif ! [ "$GITSHA" = "" ] ; then
    git checkout $GITSHA;
else
    git checkout master;
fi

# Print git log for last commit
git log -1
   
