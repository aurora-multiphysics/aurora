from settings import *

def setupInput():

    createMaterials(False)
    createSettings(False)
    createGeometry("dagmc_uwuw.h5m")
    createTallies("copper_air_tetmesh_cm.h5m")

setupInput()
openmc.run()
