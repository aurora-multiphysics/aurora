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
  displacements = 'disp_x disp_y disp_z'
[]



[Executioner]
  type = Transient
  solve_type = 'PJFNK'
  petsc_options_iname = '-pc_type -pc_hypre_type -ksp_gmres_restart'
  petsc_options_value = 'hypre boomeramg 101'
  abort_on_solve_fail=True
  num_steps=2
  dt = 0.1
  nl_abs_tol = 1e-10
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
[]

[AuxVariables]
  [temp]
  []
  [heating-local]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[Functions]
  [temp-function]
    type = ParsedFunction
    value = T0*exp((x-x0)/(rdec))
    vars = 'rdec T0 x0'
    vals = '2 1000 1'
  []
[]

[Kernels]
  [TensorMechanics]
    displacements = 'disp_x disp_y disp_z'
  []
[]

[AuxKernels]
  [temp-kernel]
    type = FunctionAux
    variable = temp
    function = temp-function
  []
[]

[BCs]
  [./x_bot]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0.0
  [../]
  [./y_bot]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0.0
  [../]
  [./z_bot]
    type = DirichletBC
    variable = disp_z
    boundary = back
    value = 0.0
  [../]
[]

[Materials]
  [copper]
    type = ADGenericConstantMaterial
    prop_names = 'thermal_conductivity specific_heat density'
    prop_values = '3.97 0.385 8.920' # W/cm*K, J/g-K, g/cm^3
    block = 1
  []
  [./elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 1e7 # 100 GPa =  10^7 N / (cm)^2
    poissons_ratio = 0.3
  [../]
  [./strain] #We use small deformation mechanics
    type = ComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
    eigenstrain_names = eigenstrain
  [../]
  [./stress]
    type = ComputeLinearElasticStress
  [../]
  [./thermal_expansion_strain]
    type = ComputeThermalExpansionEigenstrain
    stress_free_temperature = 300
    thermal_expansion_coeff = 1.8e-5 # K^-1
    temperature = temp
    eigenstrain_name = eigenstrain
  [../]
[]


[MultiApps]
  [openmc]
    type = FullSolveMultiApp
    app_type = OpenMCApp
    execute_on = "timestep_begin"
    input_files = "openmc-thermal-expansion.i"
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

[UserObjects]
  [uo-heating-function]
    type = FunctionUserObject
    variable = heating-local
    execute_on = "initial"
  []
[]

[Postprocessors]
  [volume_calc_deformed]
    type = VolumePostprocessor
    use_displaced_mesh = true
  []
  [volume_calc_orig]
    type = VolumePostprocessor
    use_displaced_mesh = false
  []
  #[total-heating]
  #  type = ElementIntegralVariablePostprocessor
  #  variable = heating-local
  #  use_displaced_mesh = true
  #[]
[]

[Outputs]
  console=false
[]
