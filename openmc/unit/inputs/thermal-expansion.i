[Mesh]
  # Length dimensions are cm
  type = GeneratedMesh
  dim = 3
  nx = 5
  ny = 5
  nz = 5
  displacements = 'disp_x disp_y disp_z'
  xmax = 5
  ymax = 5
  zmax = 5
  elem_type=TET4
[]

[Executioner]
  type = Transient
  solve_type = 'PJFNK'
  petsc_options_iname = '-pc_type -pc_hypre_type -ksp_gmres_restart'
  petsc_options_value = 'hypre boomeramg 101'
  num_steps=1
  dt = 1
[]

[Variables]
  [temp]
    initial_condition = 300.0
  []
  [disp_x]
  [../]
  [disp_y]
  [../]
  [disp_z]
  [../]
[]

[Functions]
  [heating-function]
    type = ParsedFunction
    value = q0*exp(-(x-x0)/(rdec))
    vars = 'rdec q0 x0'
    vals = '2 5000 0'
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
  [TensorMechanics] #Action that creates equations for disp_x and disp_y
    displacements = 'disp_x disp_y disp_z'
  []
[]

[BCs]
  [temp_bc]
    type = DirichletBC
    variable = temp
    boundary = right
    value = 300
  []
  [./x_bot]
    type = DirichletBC
    variable = disp_x
    boundary = right
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
  []
  [heating]
    type = ADGenericFunctionMaterial
    prop_names = 'volumetric_heat'
    prop_values = 'heating-function'
    output_properties = 'volumetric_heat'
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

[Postprocessors]
  [volume_calc_deformed]
    type = VolumePostprocessor
    use_displaced_mesh = true
  []
  [volume_calc_orig]
    type = VolumePostprocessor
    use_displaced_mesh = false
  []
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    length_scale = 1.0
  []
[]

[Outputs]
  console=false
[]
