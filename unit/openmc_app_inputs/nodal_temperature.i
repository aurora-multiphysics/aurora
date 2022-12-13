[Mesh]
  [meshcm]
    type = FileMeshGenerator
    file = copper_air_bcs_tetmesh.e
  []
  construct_side_list_from_node_list=true
[]

[Problem]
  type = FEProblem
[]

[Executioner]
  type = Transient
  num_steps = 1
  dt = 1
  solve_type = NEWTON
  abort_on_solve_fail=True
[]

[Variables]
  [temperature]
    order = FIRST
    family = LAGRANGE
    initial_condition = 300 # Start at room temperature
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

[Functions]
  [heating-function]
    type = ParsedFunction
    value = q0*exp(-(sqrt(x*x+y*y+z*z))/r0)
    vars = 'r0 q0'
    vals = '1 10000'
  []
[]

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
    block = '2'
  []
[]

[Kernels]
  [heat_conduction_time_derivative]
    type = ADHeatConductionTimeDerivative
    variable = temperature
  []
  [heat-source]
    type = ADMatHeatSource
    material_property = volumetric_heat
    variable = temperature
    block = 2
  []
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    bin_varname = "temperature"
    material_names = 'copper air'
  []
[]

[Outputs]
  console=false
[]
