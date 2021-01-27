#!/usr/bin/bash

# Define executable
EXEC="openmc"

# Define input file strings
MESHPATH="../../supplementary/openmc/legacy-mats/"
MESHFILE="copper_air_tetmesh_cm.h5m"
DAGMCFILE="dagmc.h5m"
SETTINGSFILE="settings.xml"
TALLYFILE="tallies.xml"
ORIGBASE="openmc_perf_test"

# Get OpenMC input files
for FILE in $(ls ../*.xml); do
    cp $FILE .
done

# create symbolic links to mesh files
ln -s ../$DAGMCFILE
ln -s $MESHPATH$MESHFILE

# Define numbers of particles to loop over
PARTS=(100 1000 10000)
# Define numbers of processes to loop over
PROCS=(1 2 4)
# Define number of threads (just 1 for now).
NTHREAD=1

# Modify the tally file to use a file for unstructured mesh
SEDCOMMAND="s/(create=\\\")False(\\\")/\1True\2/g"
sed -i -E $SEDCOMMAND $TALLYFILE

SEDCOMMAND="s/(load=\\\")False(\\\")/\1True\2/g"
sed -i -E $SEDCOMMAND $TALLYFILE

SEDCOMMAND="s/(<)(filename) (\/)(>)/\1\2\4$MESHFILE\1\3\2\4/g"
sed -i -E "$SEDCOMMAND" $TALLYFILE

BATCHES=10
SEDCOMMAND="s/(<)(batches) (\/)(>)/\1\2\4$BATCHES\1\3\2\4/g"
sed -i -E "$SEDCOMMAND" $SETTINGSFILE

# Loop over particles and numbers of processes
for NPART in ${PARTS[@]}; do

    # Set the number of particles to value of NPART
    SEDCOMMAND="s/(<particles>)[0-9]*(<\/particles>)/\1${NPART}\2/g"
    sed -i -E $SEDCOMMAND $SETTINGSFILE

    for NMPI in ${PROCS[@]}; do

        BASE="${ORIGBASE}_np${NPART}_nmpi${NMPI}_nt${NTHREAD}"
        LOGFILE="$BASE.log"
        OUTFILE="$BASE.csv"

        # Run exec with correct number of MPI
        echo "Running $EXEC with $NMPI cores with log=$LOGFILE and output=$OUTFILE"
        mpirun -n $NMPI $EXEC > $LOGFILE 2>&1

        # Parse the hdf5 file for run times and write to csv file
        ./get_openmc_runtimes.sh > $OUTFILE

        # Clean up before next run
        rm statepoint.10.h5
        rm tally_1.10.vtk
    done
done

# Remove OpenMC input files
for FILE in $(ls *.xml); do
    rm $FILE
done

# Remove symbolic links
unlink $DAGMCFILE
unlink $MESHFILE
