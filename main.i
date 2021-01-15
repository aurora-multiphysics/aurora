[Mesh]
  [meshcm]
    type = FileMeshGenerator
    file = copper_air_bcs_tetmesh.e
  []
  construct_side_list_from_node_list=true
[]

[Problem]
  type = FEProblem
  solve = true
[]

[Executioner]
  type = Transient
  num_steps = 5
  dt = 1
  solve_type = NEWTON
  abort_on_solve_fail=True
[]

[Variables]
  [temperature]
    order = CONSTANT
    family = MONOMIAL
    #order = FIRST
    #family = LAGRANGE
    initial_condition = 300 # Start at room temperature
  []
[]

[AuxVariables]
  [heating-local]
      order = CONSTANT
      family = MONOMIAL
  []
[]

[BCs]
  [air-outer]
    #type = DirichletBC
    # penalty is a variant for monomials
    type = PenaltyDirichletBC
    penalty = 1e5
    variable = temperature
    boundary = 'top-outer bottom-outer left-outer right-outer front-outer back-outer'
    value = 300 # (K)
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

#[Functions]
#  [heating-function]
#    type = ParsedFunction
#    value = q0*exp(-(sqrt(x*x+y*y+z*z))/r0)
#    vars = 'r0 q0'
#    vals = '5 100000'
#  []
#[]

[Materials]
  [copper]
    type = ADGenericConstantMaterial
    prop_names = 'thermal_conductivity specific_heat density'
    prop_values = '397.48 385.0 8920.0' # W/m*K, J/kg-K, kg/m^3
    block = 1
  []
  [air]
    type = ADGenericConstantMaterial
    prop_names = 'thermal_conductivity specific_heat density'
    prop_values = '26.3 1000 1'
    block = 2
  []
  [heating]
    type = ADGenericFunctionMaterial
    prop_names = 'volumetric_heat'
    prop_values = 'heating-function'
    block = '1 2'
    constant_on = 'ELEMENT'
    output_properties = 'volumetric_heat'
    outputs = exodus
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
    type = ADMatHeatSource
    material_property = volumetric_heat
    variable = temperature
  []
[]

[MultiApps]
  [openmc]
    type = FullSolveMultiApp
    app_type = OpenMCApp
    execute_on = "timestep_begin"
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
  execute_on = 'timestep_end'
#  Uncomment these lines to disable output to screen
#  console = false
#  [my_console]
#    type = Console
#    output_screen = false
#  []
[]
