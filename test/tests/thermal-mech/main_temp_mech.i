[Mesh]
  type = FileMesh
  file = copper_air_bcs_tetmesh.e
  displacements = 'disp_x disp_y disp_z'
[]

[Problem]
  type = FEProblem
[]

[Executioner]
  type = Transient
  solve_type = PJFNK
  petsc_options_iname = '-pc_type -pc_hypre_type -ksp_gmres_restart'
  petsc_options_value = 'hypre boomeramg 101'
  num_steps = 5
  dt = 1
  abort_on_solve_fail=True
[]

[Variables]
  [temp]
    order = FIRST
    family = LAGRANGE
    initial_condition = 300 # Start at room temperature
  []
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
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
  [density_local]
     order = CONSTANT
     family = MONOMIAL
  []
[]

[AuxKernels]
  [density_copy]
    type = ADMaterialRealAux
    property = density
    variable = density_local
    execute_on = timestep_end
  []
[]

[BCs]
  [air-outer]
    type = DirichletBC
    variable = temp
    boundary = 'top-outer bottom-outer left-outer right-outer front-outer back-outer'
    value = 300 # (K)
  []
  [./x_bot]
    type = DirichletBC
    variable = disp_x
    boundary = 'right top-outer bottom-outer left-outer right-outer front-outer back-outer'
    value = 0.0
  [../]
  [./y_bot]
    type = DirichletBC
    variable = disp_y
    boundary = 'right top-outer bottom-outer left-outer right-outer front-outer back-outer'
    value = 0.0
  [../]
  [./z_bot]
    type = DirichletBC
    variable = disp_z
    boundary = 'right top-outer bottom-outer left-outer right-outer front-outer back-outer'
    value = 0.0
  [../]
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

[Materials]
  [heating]
    type = ADGenericFunctionMaterial
    prop_names = 'volumetric_heat'
    prop_values = 'heating-function'
    block = '1 2'
    constant_on = 'ELEMENT'
    output_properties = 'volumetric_heat'
    outputs = exodus
  []
  [air_const_props]
    type = ADGenericConstantMaterial
    prop_names = 'thermal_conductivity specific_heat'
    prop_values = '0.26 1'
    block = 2
  []
  [air_density]
    type = ADOpenMCDensity
    density = '0.001' # g/cm^3
    displacements = 'disp_x disp_y disp_z'
    block = 2
  []
  [copper_const_props]
    type = ADGenericConstantMaterial
    prop_names = 'thermal_conductivity specific_heat'
    prop_values = '3.97 0.385' # W/cm*K, J/g-K
    block = 1
  []
  [copper_density]
    type = ADOpenMCDensity
    density = '8.920' # g/cm^3
    displacements = 'disp_x disp_y disp_z'
    block = 1
  []
  [copper_elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 1.e7
    poissons_ratio = 0.36
    block = '1'
  []
  [copper_thermal_expansion_strain]
    type = ComputeThermalExpansionEigenstrain
    stress_free_temperature = 300
    thermal_expansion_coeff = 1.8e-5 # K^-1
    temperature = temp
    eigenstrain_name = copper_eigenstrain
    block = '1'
  []
  [copper_strain] #We use small deformation mechanics
    type = ComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
    eigenstrain_names = 'copper_eigenstrain'
    block = 1
  []
  [copper_stress] #We use linear elasticity
    type = ComputeLinearElasticStress
    block = 1
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
    block = '1 2'
  []
  [TensorMechanics] #Action that creates equations for disp_x and disp_y
     displacements = 'disp_x disp_y disp_z'
     block = 1 # Only act on air
  []
  [NullKernelX]
    type = NullKernel
    variable = disp_x
    block=2
  []
  [NullKernelY]
    type = NullKernel
    variable = disp_y
    block=2
  []
  [NullKernelZ]
    type = NullKernel
    variable = disp_z
    block=2
  []
[]

[MultiApps]
  [openmc]
    type = FullSolveMultiApp
    app_type = OpenMCApp
    execute_on = "timestep_begin"
    input_files = "openmc_mech.i"
    positions = '0   0   0'
  []
[]

[Transfers]
  [./to_openmc]
    type = MoabMeshTransfer
    to_multi_app = openmc
    moabname = "moab"
  [../]
[]

[Outputs]
  exodus = true
  execute_on = 'timestep_end'
[]
