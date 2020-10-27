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
[]

[UserObjects]
  [moab]
    type = MoabUserObject
  []  
[]
