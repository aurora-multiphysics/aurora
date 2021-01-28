#!/usr/bin/env python3

import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import sys
import numpy

class PlotData():
    def __init__(self):
        self.title = ""
        self.x_label = ""
        self.y_label = ""
        self.markers = []
        self.colours = []
        self.dataset_name=""
        self.cmpdataset_name=""
        self.labels=[]
        self.cmplabels=[]

        self.outfile=""

def makePlot(all_datasets,all_cmpdatasets,plotdata=[]):
    # Create a new plot
    fig = plt.figure(tight_layout=True)
    gs = gridspec.GridSpec(1,1)

    #fig, axs = plt.subplots(1, 2, constrained_layout=True)
    ax=fig.add_subplot(gs[0])
    ax.set_xlabel(plotdata.x_label)
    ax.set_ylabel(plotdata.y_label)
    ax.set_xscale("log")
    ax.set_yscale("log")

    #ax2=fig.add_subplot(gs[1])
    #ax2.set_xlabel(x_label)
    #ax2.set_ylabel(y_label)
    #ax2.set_xscale("log")
    #ax2.set_yscale("log")

    nsets = len(all_datasets)
    for iset in range(nsets):
        data=all_datasets[iset][plotdata.dataset_name][0]
        errs=all_datasets[iset][plotdata.dataset_name][1]
        ax.errorbar(parts,data,yerr=errs,c=plotdata.colours[iset],marker=plotdata.markers[iset],label=plotdata.labels[iset])

        cmpdata=all_cmpdatasets[iset][plotdata.cmpdataset_name][0]
        cmperrs=all_cmpdatasets[iset][plotdata.cmpdataset_name][1]
        ax.errorbar(parts,cmpdata,yerr=cmperrs,c=plotdata.colours[iset],mec=plotdata.colours[iset],mfc='none',marker=plotdata.markers[iset],label=plotdata.cmplabels[iset],ls="--")

    ax.legend(loc='upper left');
    #ax2.legend(loc='upper left');
    fig.suptitle(plotdata.title)

    # Save
    plt.savefig(plotdata.outfile)


def get_csv_data(filename):
    timers={}
    with open(filename, "r") as f:
        line = f.readline()
        line = line.rstrip()
        timer_keys = line.split(",")

        ntimes=len(timer_keys)
        all_times=[]
        for i in range(ntimes):
            all_times.append([])

        while True:
            line = f.readline()
            line = line.rstrip()
            if line == "":
                break

            times=line.split(",")
            if len(times) != ntimes:
                print("datasets have different lengths")
                sys.exit()

            for i in range(ntimes):
                time = times[i]
                all_times[i].append(time)

        timers=dict(zip(timer_keys,all_times))
    return timers

def append_datasets(timers,datasets):
    for key,times in timers.items():
        #Strip quotes
        key=key.strip("\"")
        if key=="":
            continue
        if not key in datasets :
            datasets[key]=[[],[]]

        ntimes=len(times)
        sumtime=0.
        sumsqrs=0.
        for time in times:
            val = float(time)
            sumtime += val
            sumsqrs += val*val
        sumtime /= float(ntimes)
        sumsqrs /= float(ntimes)
        var = sumsqrs-sumtime*sumtime
        err = numpy.sqrt(var)

        datasets[key][0].append(sumtime)
        datasets[key][1].append(err)

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

plotdata=PlotData()

#Plot title
plotdata.title="Average time spent in OpenMCExecutioner::run vs. openmc_run"
plotdata.x_label="# Particles"
plotdata.y_label="Time / s"
plotdata.markers=["^","o","s"]
plotdata.colours=["red","green","blue"]
plotdata.labels=labels
plotdata.cmplabels=cmplabels

# Specify the dataset we want
plotdata.dataset_name="executioner_run_time"
plotdata.cmpdataset_name="simulation"

# Specify an output name
plotdata.outfile=plotdata.dataset_name+".png"

makePlot(all_datasets,all_cmpdatasets,plotdata)
