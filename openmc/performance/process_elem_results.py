#!/usr/bin/env python3

#import matplotlib.pyplot as plt
#import matplotlib.gridspec as gridspec

filebase="perf_test"
filetail="_element_value_sampler_0000.csv"

procs=[1,2,4]
parts=[100,1000,10000]
nthreads=1

#all_datasets={}
for np in parts:
    datasets={}
    serialdata=[]
    diffsum={}
    for nmpi in procs:

        # Define csv filename
        filename=filebase+"_np"+str(np)+"_nmpi"+str(nmpi)+"_nt"+str(nthreads)+filetail

        # Fetch the tallied data
        data=[]
        with open(filename, "r") as f:
            
            keepreading=True
            skip=True
            while keepreading:
                
                line = f.readline()            
                line = line.rstrip()
                keepreading = (line!="")

                # Skip first line
                if skip:
                    skip=False
                    continue

                if keepreading:
                    vals = line.split(",")
                    data.append(float(vals[0]))

        # save serial data
        if nmpi==1:
            serialdata = data
        else:            
            nresults = len(serialdata)

            if len(data) != nresults:
                print("ERROR for # MPI =",nmpi,": number of results differ!")
                continue

            diffsum[nmpi]=0.

            # Loop over all elemental results
            for iresult in range(nresults) :
                serialresult= serialdata[iresult]
                result = data[iresult]
                diff = abs(serialresult-result)
                diffsum[nmpi] += diff

            print("Total diff = ", diffsum[nmpi]," for # NMPI = ", nmpi, ", # particles = ",np)
        # End loop over numbers of processes
        
    # End loop over numbers of particles

# Plot specifications
