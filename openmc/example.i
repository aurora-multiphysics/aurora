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
  launch_threads=true
  n_threads=4
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

[Postprocessors]
  [total-heating]
    type = ElementIntegralVariablePostprocessor
    variable = heating-local
    execute_on = "final"
  []
  [total-flux]
    type = ElementIntegralVariablePostprocessor
    variable = flux
    execute_on = "final"
  []
  [copper-heating]
    type = ElementIntegralVariablePostprocessor
    variable = heating-local
    block = 1
    execute_on = "final"
  []
  [copper-flux]
    type = ElementIntegralVariablePostprocessor
    variable = flux
    block = 1
    execute_on = "final"
  []
[]

[Outputs]
  exodus = true
  csv = true
  execute_on = "final"
[]
