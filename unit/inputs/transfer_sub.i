[Mesh]
  type = GeneratedMesh
  dim = 1
  nx = 1
[]

[Problem]
  type = FEProblem
  solve = false
[]

[Executioner]
  type = Steady
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    bin_varname = "heating-local"
  []
[]

#[Outputs]
#  console=false
#[]
