from settings import *

def setupInput():

    createMaterials()
    createSettings()
    createGeometry()

setupInput()
openmc.plot_geometry()
