[Mesh]
  type = FileMesh
  file = copper_air_tetmesh.e
[]

[Problem]
  type = FEProblem
  solve = false
[]
  
[Executioner]
  type = Transient
  num_steps = 1
  dt = 1
[]
  
[Variables]
  [heating-local]
      order = CONSTANT
      family = MONOMIAL
  []
[]

[MultiApps]
  [openmc]
    type = FullSolveMultiApp
    app_type = OpenMCApp
    execute_on = "initial"
    input_files = 'openmc.i'
    positions = '0   0   0'
    library_path = ./openmc/lib
  []
[]

[Transfers]
  [./to_openmc]
    type = MoabMeshTransfer
    direction = to_multiapp
    multi_app = openmc
  [../]
[]
  
[Outputs]
  exodus = true
[]
