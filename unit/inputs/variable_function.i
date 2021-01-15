[Mesh]
  [mesh]
    type = FileMeshGenerator
    file = copper_air_bcs_tetmesh.e
  []
[]

[Problem]
  type = FEProblem
  solve = false
[]

[Executioner]
  type = Steady
[]

[MultiApps]
  [openmc]
    type = FullSolveMultiApp
    app_type = OpenMCApp
    input_files = "function_user_object_sub.i"
    library_path = ../openmc/lib
  []
[]

[Transfers]
  [./to_openmc]
    type = MoabMeshTransfer
    direction = to_multiapp
    multi_app = openmc
    moabname = "moab"
    apptype_from="AuroraApp"
  [../]
[]

[AuxVariables]
  [heating-local]
      order = CONSTANT
      family = MONOMIAL
  []
[]

[UserObjects]
  [uo-heating-function]
    type = FunctionUserObject
    variable = heating-local
    execute_on = "initial"
  []
[]

[Functions]
  [heating-function]
    type = VariableFunction
    uoname = "uo-heating-function"
  []
[]

[Outputs]
  console=false
[]
