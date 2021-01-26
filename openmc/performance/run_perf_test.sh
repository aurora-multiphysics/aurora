#!/usr/bin/bash

# Define executable
EXEC="../open_mc-opt"

# Define input file strings
EXOFILE="copper_air_tetmesh.e"
DAGMCFILE="dagmc.h5m"
SETTINGSFILE="settings.xml"
ORIGBASE="perf_test"
        
# Get OpenMC input files
for FILE in $(ls ../*.xml); do
    cp $FILE .
done

# create symbolic links to mesh files
ln -s ../$DAGMCFILE
ln -s ../$EXOFILE

# Define numbers of particles to loop over
PARTS=(100 1000 10000)
# Define numbers of processes to loop over
PROCS=(1 2 4)
# Define number of threads (just 1 for now).
NTHREAD=1

# Loop over particles and numbers of processes
for NPART in ${PARTS[@]}; do

    # Set the number of particles to value of NPART
    SEDCOMMAND="s/(<particles>)[0-9]*(<\/particles>)/\1${NPART}\2/g"
    sed -i -E $SEDCOMMAND $SETTINGSFILE

    for NMPI in ${PROCS[@]}; do

        # Set moose inputfile name
        INPUTBASE="${ORIGBASE}_np${NPART}_nmpi${NMPI}_nt${NTHREAD}"
        ORIGFILE="../$ORIGBASE.i"
        INPUTFILE="$INPUTBASE.i"

        # Create an input file with the right file base
        cp $ORIGFILE $INPUTFILE
        SEDCOMMAND="s/(\\\")($ORIGBASE)(\\\")/\1$INPUTBASE\3/"
        sed -i -E $SEDCOMMAND $INPUTFILE

        # Define a log file for console output
        LOGFILE="$INPUTBASE.log"

        # Run exec with correct number of MPI

        echo "Running $EXEC with $NMPI cores with  input=$INPUTFILE and log=$LOGFILE"
        mpirun -n $NMPI $EXEC -i $INPUTFILE > $LOGFILE 2>&1
    done
done

# Remove OpenMC input files
for FILE in $(ls *.xml); do
    rm $FILE
done
# Remove symbolic links
unlink $DAGMCFILE
unlink $EXOFILE
