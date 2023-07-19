import openmc

# Unstructured mesh to calculate tallies upon
meshname = "moab_full_0.h5m"    
umesh = openmc.UnstructuredMesh(meshname, library='moab')
mesh_filter = openmc.MeshFilter(umesh)

# Tallies
tally = openmc.Tally()
tally.filters = [mesh_filter]
tally.scores = ['heating-local']
tally.estimator = 'tracklength'
tallies = openmc.Tallies([tally])
tallies.export_to_xml()
print("Created tallies.xml")
