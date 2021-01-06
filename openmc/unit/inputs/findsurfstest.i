[Mesh]
  [meshcm]
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

[Materials]
  [copper]
    type = ADGenericConstantMaterial
    prop_names = 'dummy_prop'
    prop_values = '1.0'
    compute = false
    block = 1
  []
  [air]
    type = ADGenericConstantMaterial
    prop_names = 'dummy_prop'
    prop_values = '1.0'
    compute = false
    block = 2
  []
[]
  
[UserObjects]
  [moab]
    type = MoabUserObject
    # match up with variable below for this test
    bin_varname = "temperature"
    material_names = 'copper air'
  []
[]

[Variables]
  [temperature]
    order = CONSTANT
    family = MONOMIAL
  []
[]
