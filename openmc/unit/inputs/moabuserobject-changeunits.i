[Mesh]
  [meshcm]
    type = FileMeshGenerator
    file = copper_air_bcs_tetmesh.e
  []
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
    # change the units used in moab
    length_scale = 1000.0
  []
[]

[Variables]
  [temperature]
    order = CONSTANT
    family = MONOMIAL
  []
[]
