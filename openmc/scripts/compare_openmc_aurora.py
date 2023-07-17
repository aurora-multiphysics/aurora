import openmc
import argparse

def process_statepoint(filename):
    print("Opening statepoint file: ",filename)
    sp = openmc.StatePoint(filename)
    print("Data loaded succesfully")

    tallies=list(sp.tallies.values())
    tally=tallies[0]
    scores=tally.scores
    # shape of the tally data array (# filter bins, # nuclide bins, # scores)
    shape = tally.shape

    data={}
    keys=[]
    for i_score, score_name in enumerate(scores):
        err_name=score_name+"-err"
        mean = tally.mean[:,0,i_score].flatten()
        err  = tally.std_dev[:,0,i_score].flatten()
        data[score_name]=mean
        data[err_name]=err

    return data

def process_csv(filename):
    data={}
    keys=[]
    with open(filename, "r") as f:            
        keepreading=True
        header=True
        while keepreading:        
            line = f.readline()            
            line = line.rstrip()
            keepreading = (line!="")
            if keepreading:
                vals = line.split(",")
                # Initialise header row
                if header:
                    header=False
                    keys = [ str(key_str) for key_str in vals ]
                    for key in keys:
                        data[key] = []
                else:
                    for i_key, key in enumerate(keys):
                        data[key].append(float(vals[i_key]))

    return data

def compare_data(sp_data,csv_data,tol=1e-9):
    count_large_diff={}
    for key in sp_data.keys():
        if key not in csv_data.keys():
            raise RuntimeError("Missing expected key in csv: "+key)
    
        openmc_data = sp_data[key]
        moose_data = csv_data[key]
        openmc_len = len(openmc_data)
        moose_len = len(moose_data)

        if not openmc_len == moose_len:
            msg="Data set sizes differ between MOOSE csv and OpenMC statepoint files"
            raise RuntimeError(msg)

        count_large_diff[key]=0
        for i_elem in range(moose_len):
            diff = abs(openmc_data[i_elem] - moose_data[i_elem])
            if diff > tol:
                count_large_diff[key]+=1

    print("Summary of differences exceeding tolerance = ",tol)
    for key, count in count_large_diff.items():
        print(key,": ",count)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compare an OpenMC statepoint file with a MOOSE csv file"
    )
    parser.add_argument(
        "--csv",
        action="store",
        dest="csv",
        required=True,
        help="Name of MOOSE CSV file",
    )
    parser.add_argument(
        "--sp",
        action="store",
        dest="sp",
        required=True,
        help="Name of OpenMC statepoint file",
    )
    
    args = parser.parse_args()
    return args
    

args = parse_args()
sp_file  = args.sp
csv_file = args.csv

sp_data = process_statepoint(sp_file)
csv_data = process_csv(csv_file)
        
compare_data(sp_data,csv_data)
