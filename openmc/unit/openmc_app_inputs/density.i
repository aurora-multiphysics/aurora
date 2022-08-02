[Mesh]
  type = GeneratedMesh
  dim = 3
  nx = 1
  ny = 1
  nz = 1
  displacements = 'disp_x disp_y disp_z'
[]

[Problem]
  type=FEProblem
[]

[Executioner]
  type = Transient
  solve_type = 'PJFNK'
  num_steps=1
  dt = 1
[]

[Kernels]
  [heat_conduction]
    type = HeatConduction
    variable = temp
  []
  [heat_conduction_time_derivative]
    type = HeatConductionTimeDerivative
    variable = temp
  []
  [heat-source]
    type = HeatSource
    variable = temp
    value = 1.0
  []
  [TensorMechanics]
     displacements = 'disp_x disp_y disp_z'
  []
[]

[Variables]
  [temp]
    initial_condition = 300 # Start at room temperature
  []
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
[]

[Materials]
 [const_props]
    type = GenericConstantMaterial
    prop_names = 'thermal_conductivity specific_heat'
    prop_values = '3.97 0.385' # W/cm*K, J/g-K, g/cm^3
  []
  [test_density]
    type = OpenMCDensity
    displacements = 'disp_x disp_y disp_z'
    density = 8.0
  []
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 1.e7
    poissons_ratio = 0.36
  []
  [thermal_expansion_strain]
    type = ComputeThermalExpansionEigenstrain
    stress_free_temperature = 300
    thermal_expansion_coeff = 1.8e-5 # K^-1
    temperature = temp
    eigenstrain_name = eigenstrain
  []
  [strain]
    type = ComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
    eigenstrain_names = 'eigenstrain'
  []
  [stress]
    type = ComputeLinearElasticStress
  []
[]

[Outputs]
  console=false
[]
