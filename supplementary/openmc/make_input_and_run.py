from settings import *

def setupInput():

    createMaterials(False)
    createSettings(False)
    createGeometry()
    createTallies("copper_air_tetmesh_cm.h5m")

setupInput()
openmc.run()
