#!/usr/bin/env python3

from openmc import Material, Materials
import openmc
import os
import subprocess

def createMaterials(createplots=True):
    
    air = openmc.Material(1, name='air')
    air.set_density('g/cc', 0.001205)
    air.add_element('N', 0.784431)
    air.add_element('O', 0.210748)
    air.add_element('Ar',0.0046)

    copper = Material(2, "copper")
    copper.add_element('Cu', 1.0)
    copper.set_density('g/cm3', 8.96)
    copper.temperature = 300
    
    mats = openmc.Materials([copper, air])
    mats.export_to_xml()
    print("Created materials.xml")

    if createplots:
        p_xy = openmc.Plot()
        p_xy.width = (25.0, 25.0)
        p_xy.pixels = (400, 400)
        p_xy.origin = (0.,0.,0.)
        p_xy.color_by = 'material'
        p_xy.colors = {copper: 'orange', air: 'blue'}
        p_xy.basis = 'xy'

        p_xz = openmc.Plot()
        p_xz.width = (25.0, 25.0)
        p_xz.pixels = (400, 400)
        p_xz.origin = (0.,0.,0.)
        p_xz.color_by = 'material'
        p_xz.colors = {copper: 'orange', air: 'blue'}
        p_xz.basis = 'xz'

        p_yz = openmc.Plot()
        p_yz.width = (25.0, 25.0)
        p_yz.pixels = (400, 400)
        p_yz.origin = (5.,0.,0.)
        p_yz.color_by = 'material'
        p_yz.colors = {copper: 'orange', air: 'blue'}
        p_yz.basis = 'yz'
    
        plots = openmc.Plots([p_xy,p_xz,p_yz])
        plots.export_to_xml()
        print("Created plots.xml")

def createSettings(  ):

    # Create neutron source
    point = openmc.stats.Point((0, 0, 0))
    src = openmc.Source(space=point)
    src.particle='neutron'

    # Create settings object
    settings = openmc.Settings()
    settings.source = src
    settings.run_mode = 'fixed source'
    settings.photon_transport = False
    
    # Turn on dagmc
    settings.dagmc = True
    settings.batches = 10
    settings.inactive = 2
    settings.particles = 5000

    # Interpolate temperatures
    settings.temperature['method'] = 'interpolation'
    
    settings.export_to_xml()
    print("Created settings.xml")

def createTallies(meshname, create=True, load=True):

    # Filters
    
    # Unstructured mesh to calculate tallies upon
    umesh = openmc.UnstructuredMesh(meshname, create, load)    
    mesh_filter = openmc.MeshFilter(umesh)

    # Tallies
    tally = openmc.Tally()
    # energy filter 0 - 1 MeV, 1 - 5 MeV
    energy_filter = openmc.EnergyFilter((0.0, 1.e+06, 5.e+06))
    tally.filters = [energy_filter,mesh_filter]
    tally.scores = ['heating-local', 'flux']
    tally.estimator = 'tracklength'    
    tallies = openmc.Tallies([tally])
    tallies.export_to_xml()
    print("Created tallies.xml")
