[Mesh]
  type = FileMesh
  file = copper_air_bcs_tetmesh.e
  second_order = true
[]

[Problem]
  type = OpenMCProblem
[]

[Executioner]
  type = OpenMCExecutioner
  variables = 'heating-local'
  score_names = 'heating-local'
  tally_ids = '1'
  err_variables = 'heating-local-err'
[]

[Variables]
  [heating-local]
    order = CONSTANT
    family = MONOMIAL
  []
  [heating-local-err]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[UserObjects]
  [moab]
    type = MoabUserObject
  []
[]

# Worryingly this is needed when multiple app tests are run in sequence
# presumably the console object does not get properly destroyed...
[Outputs]
  console=false
[]
