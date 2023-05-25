[Mesh]
 [filemesh]
    type = FileMeshGenerator
    file = copper_air_bcs_tetmesh.e
    show_info = true
  []
[]

[Problem]
  type = OpenMCProblem
[]

[Executioner]
  type = OpenMCExecutioner
  variables = 'heating-local flux'
  score_names = 'heating-local flux'
  tally_ids = '1 1'
  err_variables = 'heating-local-err flux-err'
  launch_threads=false
[]

[AuxVariables]
  [temperature]
    order = FIRST
    family = LAGRANGE
    initial_condition = 300 # Start at room temperature
  []
[]

[UserObjects]
  [dag_reflecting]
    type =  DagSurfaceUserObject
    boundary_type = Reflecting
    boundary_names = 'left right'
  []
  [dag_graveyard]
    type =  DagSurfaceUserObject
    boundary_type = Graveyard
    boundary_names = 'top-outer bottom-outer left-outer right-outer front-outer back-outer'
  []
  [moab]
    type = MoabUserObject
    length_scale = 1.0
    dag_surface_names = 'dag_reflecting dag_graveyard'
    bin_varname = temperature
    material_names = 'copper air'
    material_openmc_names = 'copper air'
    output_skins = true
  []
[]

# TODO deprecate the need for this
[Materials]
  [copper]
    type = ADGenericConstantMaterial
    prop_names = 'thermal_conductivity specific_heat density'
    prop_values = '3.97 0.385 8.920' # W/cm*K, J/g-K, g/cm^3
    block = 1
  []
  [air]
    type = ADGenericConstantMaterial
    prop_names = 'thermal_conductivity specific_heat density'
    prop_values = '0.26 1 0.001'
    block = 2
  []
[]
