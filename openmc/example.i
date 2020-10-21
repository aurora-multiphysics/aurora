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

[Outputs]
  exodus = true
  console = true
  execute_on = "final"
[]
