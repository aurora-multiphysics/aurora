[Mesh]
  # Length dimensions are cm
  type = GeneratedMesh
  dim = 3
  nx = 1
  ny = 1
  nz = 1
  elem_type=TET10
[]

[Problem]
  type = OpenMCProblem
[]

[Executioner]
  type = Steady
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    # match up with variable below for this test
    bin_varname = "temperature"
  []
[]

[Variables]
  [temperature]
    order = CONSTANT
    family = MONOMIAL
  []
[]
