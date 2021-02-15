#!/usr/bin/bash

# Start with definitions

# Define the build method
METHOD="opt"

# Define executable
EXEC="../open_mc-$METHOD"

# Define input file strings
EXOFILE="copper_air_tetmesh.e"
DAGMCFILE="dagmc.h5m"
SETTINGSFILE="settings.xml"
ORIGBASE="perf_test"

# Define numbers of particles to loop over
PARTS=(100 1000 10000)
# Define numbers of processes to loop over
PROCS=(1 2 4)
# Define number of threads (just 1 for now).
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
ln -s ../$EXOFILE

# Turn back on statepoint generation to match OpenMC perf runs
BATCHES=10
SEDCOMMAND="s/(<)(batches) (\/)(>)/\1\2\4$BATCHES\1\3\2\4/g"
sed -i -E "$SEDCOMMAND" $SETTINGSFILE

# Loop over particles and numbers of processes
for NPART in ${PARTS[@]}; do

    # Set the number of particles to value of NPART
    SEDCOMMAND="s/(<particles>)[0-9]*(<\/particles>)/\1${NPART}\2/g"
    sed -i -E $SEDCOMMAND $SETTINGSFILE

    # Loop over number of MPI
    for NMPI in ${PROCS[@]}; do

        MPIOPT=""
        if [ $NMPI -eq 1 ]; then
            MPIOPT="--bind-to none"
        fi

        # Loop over number of threads
        for NTHREAD in ${THREADS[@]}; do

            # Don't exceed the maximum number of cores
            TOTALPROCS=$((NMPI*NTHREAD))
            if [ $TOTALPROCS  -gt $MAXPROCS ]; then
                continue
            fi

            # Set moose inputfile name
            INPUTBASE="${ORIGBASE}-${METHOD}_np${NPART}_nmpi${NMPI}_nt${NTHREAD}"
            ORIGFILE="../$ORIGBASE.i"
            INPUTFILE="$INPUTBASE.i"

            # Create a log file for console output
            LOGFILE="$INPUTBASE.log"

            # Create a file in which to combine runtime data
            OUTFILE="$INPUTBASE.csv"

            # Logging
            echo "Performing $RUNS runs of $EXEC -i $INPUTFILE with $NMPI MPI processes and $NTHREAD threads." > $LOGFILE
            echo "Performing $RUNS runs of $EXEC with $NMPI MPI processes and $NTHREAD threads"
            echo "  input = $INPUTFILE"
            echo "  log = $LOGFILE"
            echo "  output = $OUTFILE"

            # Loop over runs
            for irun in `seq 1 $RUNS`; do

                # Logging
                echo "  Run $irun of $RUNS"
                echo "Run $irun" >> $LOGFILE

                # Create an input file with the right file base
                cp $ORIGFILE $INPUTFILE

                # Set the pattern for output files
                FILEBASE="${INPUTBASE}_${irun}"
                SEDCOMMAND="s/(\\\")($ORIGBASE)(\\\")/\1$FILEBASE\3/"
                sed -i -E $SEDCOMMAND $INPUTFILE

                # Run exec with correct number of MPI and threads
                echo "mpirun $MPIOPT  -np $NMPI $EXEC --n-threads=$NTHREAD -i $INPUTFILE" >> $LOGFILE
                mpirun $MPIOPT  -np $NMPI $EXEC --n-threads=$NTHREAD -i $INPUTFILE >> $LOGFILE 2>&1
                if [ $? -ne 0 ] ; then
                    cat $LOGFILE
                    exit 1
                fi

                # Combine output in one file
                TMPFILE="$FILEBASE.csv"

                # File headings for first run
                if [ "$irun" -eq "1" ]
                then
                    head -1 $TMPFILE > $OUTFILE
                fi
                # Append file with data
                tail -1 $TMPFILE >> $OUTFILE

                # Delete temporary file
                rm $TMPFILE

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
unlink $EXOFILE
