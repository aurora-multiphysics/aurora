[Mesh]
  type = FileMesh
  file = copper_air_tetmesh.e
[]

[Problem]
  type = OpenMCProblem
[]

  
[Executioner]
  type = OpenMCExecutioner
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
