[Mesh]
  type = FileMesh
  file = copper_air_bcs_tetmesh.e
  displacements = 'disp_x disp_y disp_z'
[]

[Problem]
  type = FEProblem
  solve = false
[]

[Executioner]
  type = Steady
[]

[Materials]
  [copper_density]
    type = ADOpenMCDensity
    density = '8920.' # kg/m^3
    displacements = 'disp_x disp_y disp_z'
    block = '1 2'
    compute=false
  []
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    bin_varname = "temperature"
    density_name = "density_local"
    material_names = 'copper_density'
    material_openmc_names = 'copper'
    rel_den_min = -0.1
    rel_den_max = 0.1
    n_density_bins = 5
    density_scale = 0.001 # factor to change density in material back to g/cc
  []
[]

[Variables]
  [temperature]
    order = CONSTANT
    family = MONOMIAL
  []
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
[]

[AuxVariables]
  [density_local]
     order = CONSTANT
     family = MONOMIAL
  []
[]
