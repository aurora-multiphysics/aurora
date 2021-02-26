[Mesh]
  type = GeneratedMesh
  dim = 1
  nx = 1
[]

[Problem]
  type = OpenMCProblem
[]

[Executioner]
  type = OpenMCExecutioner
  variable = 'heating-local'
  neutron_source = 1.0e16
  launch_threads=true
  n_threads=4
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    bin_varname = "temperature"
    material_names = 'copper_props air_props'
    material_openmc_names = 'copper air'
    output_skins = true
    faceting_tol = 10
    length_scale = 1.0
  []
[]

#  Uncomment these lines to disable output to screen
#[Outputs]
#  console = false
#  [my_console]
#    type = Console
#    output_screen = false
#  []
#[]
