[Mesh]
  [meshcm]
    type = FileMeshGenerator
    file = copper_air_tetmesh.e
  []
[]

[Problem]
  type = OpenMCProblem
[]

[Executioner]
  type = OpenMCExecutioner
  variable = 'heating-local'
[]

[Variables]
  [heating-local]
      order = CONSTANT
      family = MONOMIAL
  []
[]

[UserObjects]
  [moab]
    type = MoabUserObject
  []
[]

[Outputs]
  exodus = true
  execute_on = "final"
[]
