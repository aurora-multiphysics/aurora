#!/usr/bin/env python3

from pyne.material import Material
from pyne.material_library import MaterialLibrary

mat_lib = MaterialLibrary()

# Create a pyne material for copper
copper = Material({'Cu':1})
# Density in g/cc
copper.density = 8.920
copper = copper.expand_elements()

# Add to material library
mat_lib["copper"] = copper

# Create a pyne material for copper
air = Material({'N':0.784431, 'O16':0.210748, 'Ar':0.004821})
# Density in g/cc
air.density = 0.001
air = air.expand_elements()

# Add to material library
mat_lib["air"] = air

#graveyard = Material();
#mat_lib["graveyard"] = graveyard

#vacuum = Material();
#mat_lib["vacuum"] = vacuum


mat_lib.write_hdf5("material_library.h5","/materials","/nucid")
