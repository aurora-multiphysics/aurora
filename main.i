[Mesh]
  type = FileMesh
  file = copper_air_tetmesh.e
[]

[Problem]
  type = FEProblem
  solve = true
[]

[Executioner]
  type = Transient
  num_steps = 1
  dt = 1
  solve_type = NEWTON
[]

[Variables]
  [temperature]
    order = FIRST
    family = LAGRANGE
    initial_condition = 300 # Start at room temperature
  []
[]

[AuxVariables]
  [heating-local]
      order = CONSTANT
      family = MONOMIAL
  []
[]

[Materials]
  [copper]
    type = ADGenericConstantMaterial
    prop_names = 'thermal_conductivity specific_heat density'
    prop_values = '397.48 385.0 8920.0' # W/m*K, J/kg-K, kg/m^3 rounded to nice numbers
    block = 1
  []
  [air]
    type = ADGenericConstantMaterial
    prop_names = 'thermal_conductivity specific_heat density'
    prop_values = '26.3 1000 1'
    block = 2
  []
[]

[UserObjects]
  [mesh-heating-function]
    type = MeshFunctionUserObject
    variable = heating-local
    execute_on = "initial"
  []
[]

[Functions]
  [heating-function]
    type = VariableFunction
    uoname = "mesh-heating-function"
  []
[]

[Kernels]
  [heat_conduction]
    type = ADHeatConduction
    variable = temperature
  []
  [heat_conduction_time_derivative]
    type = ADHeatConductionTimeDerivative
    variable = temperature
  []
  [heat-source]
    type = HeatSource
    variable = temperature
    function = heating-function
  []
[]

[MultiApps]
  [openmc]
    type = FullSolveMultiApp
    app_type = OpenMCApp
    execute_on = "initial"
    input_files = "openmc.i"
    positions = '0   0   0'
    library_path = ./openmc/lib
  []
[]

[Transfers]
  [./to_openmc]
    type = MoabMeshTransfer
    direction = to_multiapp
    multi_app = openmc
    moabname = "moab"
  [../]
[]

[Outputs]
  exodus = true
[]
