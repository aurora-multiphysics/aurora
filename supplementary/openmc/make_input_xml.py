#!/usr/bin/env python3

from settings import *

def setupInput( ):

    createMaterials()
    createSettings()
    createGeometry("dagmc_uwuw.h5m")

setupInput()
