[Mesh]
  type = FileMesh
  file = copper_air_tetmesh.e
[]

[Problem]
  type = OpenMCProblem
[]

[Executioner]
  type = OpenMCExecutioner
  variables = 'heating-local flux'
  score_names = 'heating-local flux'
  tally_ids = '1 1'
  err_variables = 'heating-local-err flux-err'
  launch_threads=false
[]

[AuxVariables]
  [heating-local]
      order = CONSTANT
      family = MONOMIAL
  []
  [heating-local-err]
      order = CONSTANT
      family = MONOMIAL
  []
  [flux]
      order = CONSTANT
      family = MONOMIAL
  []
  [flux-err]
      order = CONSTANT
      family = MONOMIAL
  []
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    length_scale = 1.0
  []
[]

[Outputs]
  exodus = true
  execute_on = "final"
[]
