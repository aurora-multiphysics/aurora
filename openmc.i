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
  neutron_source = 1.0e8
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    bin_varname = "temperature"
    n_minor = 2
  []
[]
