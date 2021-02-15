#!/usr/bin/bash

# Start with definitions

# Define executable
EXEC="openmc"

# Define input file strings
MESHPATH="../../supplementary/openmc/legacy-mats/"
MESHFILE="copper_air_tetmesh_cm.h5m"
DAGMCFILE="dagmc.h5m"
SETTINGSFILE="settings.xml"
TALLYFILE="tallies.xml"
ORIGBASE="openmc_perf_test"

# Define numbers of particles to loop over
PARTS=(100 1000 10000)
# Define numbers of processes to loop over
PROCS=(1 2 4)

# Define number of threads to loop over
THREADS=(1 2 4 8)

# I only have 4 physical cores each with 2 threads, so cap at 8 procs
MAXPROCS=8

# Specify how many repeats to do of each configuration
RUNS=5

# End of definitions

# Get OpenMC input files
for FILE in $(ls ../*.xml); do
    cp $FILE .
done

# create symbolic links to mesh files
ln -s ../$DAGMCFILE
ln -s $MESHPATH$MESHFILE

# Modify the tally file to use a file for unstructured mesh
SEDCOMMAND="s/(create=\\\")False(\\\")/\1True\2/g"
sed -i -E $SEDCOMMAND $TALLYFILE

SEDCOMMAND="s/(load=\\\")False(\\\")/\1True\2/g"
sed -i -E $SEDCOMMAND $TALLYFILE

SEDCOMMAND="s/(<)(filename) (\/)(>)/\1\2\4$MESHFILE\1\3\2\4/g"
sed -i -E "$SEDCOMMAND" $TALLYFILE

# Turn back on statepoint generation
BATCHES=10
SEDCOMMAND="s/(<)(batches) (\/)(>)/\1\2\4$BATCHES\1\3\2\4/g"
sed -i -E "$SEDCOMMAND" $SETTINGSFILE

# Loop over particles and numbers of processes
for NPART in ${PARTS[@]}; do

    # Set the number of particles to value of NPART
    SEDCOMMAND="s/(<particles>)[0-9]*(<\/particles>)/\1${NPART}\2/g"
    sed -i -E $SEDCOMMAND $SETTINGSFILE

    for NMPI in ${PROCS[@]}; do

        MPIOPT=""
        if [ $NMPI -eq 1 ]; then
            MPIOPT="--bind-to none"
        fi

        for NTHREAD in ${THREADS[@]}; do

            TOTALPROCS=$((NMPI*NTHREAD))
            if [ $TOTALPROCS  -gt $MAXPROCS ]; then
                continue
            fi

            BASE="${ORIGBASE}_np${NPART}_nmpi${NMPI}_nt${NTHREAD}"
            LOGFILE="$BASE.log"
            OUTFILE="$BASE.csv"

            ## Logging
            echo "Performing $RUNS runs of $EXEC with $NMPI MPI processes, $NTHREAD threads."
            echo "  log = $LOGFILE"
            echo "  output=$OUTFILE"
            echo "Performing $RUNS runs of $EXEC with $NMPI MPI processes, $NTHREAD threads, and with output=$OUTFILE" > $LOGFILE

            for irun in `seq 1 $RUNS`; do

                # Run exec with correct number of MPI
                echo "  Run $irun of $RUNS"
                echo "Run $irun" >> $LOGFILE
                echo "mpirun $MPIOPT -np $NMPI $EXEC -s $NTHREAD" >> $LOGFILE
                mpirun $MPIOPT -np $NMPI $EXEC -s $NTHREAD >> $LOGFILE 2>&1
                if [ $? -ne 0 ] ; then
                    cat $LOGFILE
                    exit 1
                fi

                # Parse the hdf5 file for run times and write to csv file
                TMPFILE="tmp.txt"
                ./get_openmc_runtimes.sh > $TMPFILE
                if [ "$irun" -eq "1" ]; then
                    head -1 $TMPFILE > $OUTFILE
                fi

                # Append file with data
                tail -1 $TMPFILE >> $OUTFILE

                # Clean up before next run
                rm $TMPFILE
                rm statepoint.10.h5
                rm tally_1.10.vtk

            done # End loop over runs
        done # End loop over nthreads
    done # End loop over nmpi
done # End loop over particle numbers

# Remove OpenMC input files
for FILE in $(ls *.xml); do
    rm $FILE
done

# Remove symbolic links
unlink $DAGMCFILE
unlink $MESHFILE
