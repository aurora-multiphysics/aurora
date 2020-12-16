from settings import *

def setupInput():
    
    createMaterials()
    createSettings()

setupInput()
openmc.plot_geometry()
