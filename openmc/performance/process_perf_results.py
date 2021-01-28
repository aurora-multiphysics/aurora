#!/usr/bin/env python3

import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec

def get_csv_data(filename):
    timers={}
    with open(filename, "r") as f:
        line = f.readline()
        line = line.rstrip()
        timer_keys = line.split(",")
        line = f.readline()
        line = line.rstrip()
        times = line.split(",")
        timers=dict(zip(timer_keys,times))
    return timers

def append_datasets(timers,datasets):
    for key,time in timers.items():
        #Strip quotes
        key=key.strip("\"")
        if not key in datasets :
            datasets[key]=[]
        datasets[key].append(float(time))

def append_datasets_from_file(filebase,np,nmpi,nthreads,ext,datasets):
    # Define csv filename
    filename=filebase+"_np"+str(np)+"_nmpi"+str(nmpi)+"_nt"+str(nthreads)+ext
    # Extract data from file
    timers=get_csv_data(filename)
    # Store timings against their keys
    append_datasets(timers,datasets)

# Patterns for data files
filebase="perf_test"
ext=".csv"

# Run specifications
procs=[1,2,4]
parts=[100,1000,10000]
nthreads=1

# Get all the performance data in a dictionary
all_datasets=[]
labels=[]

for nmpi in procs:

    # Dictionaries to store the data for different numbers of processes
    datasets={}

    # Loop over runs with different numbers of particles
    for np in parts:
        append_datasets_from_file(filebase,np,nmpi,nthreads,ext,datasets)

    label="# MPI = "+str(nmpi)
    labels.append(label)
    all_datasets.append(datasets)

# Plot specifications

#Plot title
title="Time spent in OpenMCExecutioner::run"
x_label="# Particles"
y_label="Time / s"
markers=["^","o","s"]
colours=["red","green","blue"]

# Specify the dataset we want
dataset_name="executioner_run_time"

# Specify an output name
outfile=dataset_name+".png"

# Create a new plot
fig = plt.figure(tight_layout=True)
gs = gridspec.GridSpec(1,1)

ax=fig.add_subplot(gs[0])
ax.set_xlabel(x_label)
ax.set_ylabel(y_label)
ax.set_xscale("log")
ax.set_yscale("log")

nsets = len(all_datasets)

for iset in range(nsets):
    data=all_datasets[iset][dataset_name]
    ax.scatter(parts,data,c=colours[iset],marker=markers[iset],label=labels[iset])

plt.legend(loc='upper left');
plt.title(title)

# Save
plt.savefig(outfile)
