from settings import *

def setupInput():
    
    createMaterials()
    createSettings()
    createTallies("copper_air_tetmesh.h5m")

setupInput()
openmc.plot_geometry()
