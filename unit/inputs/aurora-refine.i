[Mesh]
  [cube]
    # Length dimensions are cm
    type = GeneratedMeshGenerator
    dim = 3
    nx = 5
    ny = 5
    nz = 5
    xmax = 1
    ymax = 1
    zmax = 1
    elem_type=TET4
  []
  [cube_id]
    type = SubdomainIDGenerator
    input = cube
    subdomain_id = 1
  []
[]

[Executioner]
  type = Transient
  solve_type = 'PJFNK'
  petsc_options_iname = '-pc_type -pc_hypre_type -ksp_gmres_restart'
  petsc_options_value = 'hypre boomeramg 101'
  num_steps=2
  dt = 0.1
[]

[Variables]
  [temp]
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

[Functions]
  [heat-function]
    type = ParsedFunction
    value = (q1-q0)*(x-x0)+q0
    vars = 'q1 q0 x0 '
    vals = '1000 0 0'
  []
[]

[Kernels]
  [heat_conduction]
    type = ADHeatConduction
    variable = temp
  []
  [heat_conduction_time_derivative]
    type = ADHeatConductionTimeDerivative
    variable = temp
  []
  [heat-source]
    type = ADMatHeatSource
    material_property = volumetric_heat
    variable = temp
  []
[]

[BCs]
  [temp-bc]
    type = DirichletBC
    variable = temp
    boundary = left
    value = 300
  []
[]

[Materials]
  [copper]
    type = ADGenericConstantMaterial
    prop_names = 'thermal_conductivity specific_heat density'
    prop_values = '3.97 0.385 8.920' # W/cm*K, J/g-K, g/cm^3
    block = 1
  []
  [heating]
    type = ADGenericFunctionMaterial
    prop_names = 'volumetric_heat'
    prop_values = 'heat-function'
    block = '1'
    constant_on = 'ELEMENT'
  []
[]

[MultiApps]
  [openmc]
    type = FullSolveMultiApp
    app_type = OpenMCApp
    execute_on = "timestep_begin"
    input_files = "openmc-refine.i"
    positions = '0   0   0'
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

[Adaptivity]
  marker = error_frac
  max_h_level = 1
  start_time = 0.1
  [Indicators]
    [temperature_jump]
      type = GradientJumpIndicator
      variable = temp
      scale_by_flux_faces = true
    []
  []
  [Markers]
    [error_frac]
      type = ErrorFractionMarker
      indicator = temperature_jump
      refine = 1
      coarsen = 0
    []
  []
[]

[UserObjects]
  [uo-heating-function]
    type = FunctionUserObject
    variable = heating-local
    execute_on = "initial"
  []
[]

[Outputs]
  console=false
[]
