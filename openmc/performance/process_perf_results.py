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
        if key=="":
            continue
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
comparefilebase="openmc_perf_test"
ext=".csv"

# Run specifications
procs=[1,2,4]
parts=[100,1000,10000]
nthreads=1

# Get all the performance data in a dictionary
all_datasets=[]
all_cmpdatasets=[]

# Place to keep legend text
labels=[]
cmplabels=[]

for nmpi in procs:

    # Dictionaries to store the data for different numbers of processes
    datasets={}
    cmpdatasets={}

    # Loop over runs with different numbers of particles
    for np in parts:
        append_datasets_from_file(filebase,np,nmpi,nthreads,ext,datasets)
        append_datasets_from_file(comparefilebase,np,nmpi,nthreads,ext,cmpdatasets)

    # Create legend lables
    labelbase="# MPI = "+str(nmpi)
    label=labelbase+" (MOOSE)"
    cmplabel=labelbase+" (OpenMC)"
    labels.append(label)
    cmplabels.append(cmplabel)

    # Save all the data
    all_datasets.append(datasets)
    all_cmpdatasets.append(cmpdatasets)

# Plot specifications

#Plot title
title="Time spent in OpenMCExecutioner::run vs. openmc_run"
x_label="# Particles"
y_label="Time / s"
markers=["^","o","s"]
colours=["red","green","blue"]

# Specify the dataset we want
dataset_name="executioner_run_time"
cmpdataset_name="simulation"

# Specify an output name
outfile=dataset_name+".png"

# Create a new plot
fig = plt.figure(tight_layout=True)
gs = gridspec.GridSpec(1,2)

#fig, axs = plt.subplots(1, 2, constrained_layout=True)


ax=fig.add_subplot(gs[0])
ax2=fig.add_subplot(gs[1])
ax.set_xlabel(x_label)
ax.set_ylabel(y_label)
ax.set_xscale("log")
ax.set_yscale("log")

ax2.set_xlabel(x_label)
ax2.set_ylabel(y_label)
ax2.set_xscale("log")
ax2.set_yscale("log")

nsets = len(all_datasets)

for iset in range(nsets):
    data=all_datasets[iset][dataset_name]
    ax.scatter(parts,data,c=colours[iset],marker=markers[iset],label=labels[iset])

    cmpdata=all_cmpdatasets[iset][cmpdataset_name]
    ax2.scatter(parts,cmpdata,edgecolors=colours[iset],facecolors='none',marker=markers[iset],label=cmplabels[iset])


ax.legend(loc='upper left');
ax2.legend(loc='upper left');
fig.suptitle(title)

# Save
plt.savefig(outfile)
