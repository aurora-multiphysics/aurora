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
  neutron_source = 1.0e18
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    bin_varname = "temperature"
    material_names = 'copper air'
    output_skins = true
    #uncomment to change lengthscale relative to moose
    length_scale = 100
    faceting_tol = 10
  []
[]

[Outputs]
  console = false
  [my_console]
    type = Console
    output_screen = false
  []
[]
