#!/usr/bin/bash

# Simple script to extract the timing data from OpenMC statepoint file

FILENAME="statepoint.10.h5"
PATH="/runtime/"

DATASETS=("accumulating tallies"
          "active batches"
          "reading cross sections"
          "simulation"
          "total"
          "total initialization"
          "transport"
          "writing statepoints")

COLUMNS=""
RESULTS=""
# Loop over the datasets
for i in $(echo ${!DATASETS[@]}); do
    DATASET=${DATASETS[$i]};
    # Get the output of h5ls
    FULLPATH="$FILENAME$PATH$DATASET"
    OUTPUT=$(/usr/bin/h5ls -d "$FULLPATH")
    # Convert to array
    OUTARRAY=($OUTPUT)

    # Get size as the data is the last value
    SIZE="${#OUTARRAY[@]}"
    INDEX=$((SIZE-1))

    # Get the time
    VAL="${OUTARRAY[$INDEX]}"

    # Format data for output
    COLUMNS="$COLUMNS\"$DATASET\","
    RESULTS="$RESULTS$VAL,"
done

# Print the output
echo $COLUMNS
echo $RESULTS
