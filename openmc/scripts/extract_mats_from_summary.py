import openmc
import h5py

materials = openmc.Materials()

filename = "summary.h5"
fh = h5py.File(filename, 'r')
for group in fh['materials'].values():
    material = openmc.Material.from_hdf5(group)
    materials.append(material)
materials.export_to_xml()
