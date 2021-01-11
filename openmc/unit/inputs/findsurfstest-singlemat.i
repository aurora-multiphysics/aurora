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
    block = '1 2'
  []
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    # match up with variable below for this test
    bin_varname = "temperature"
    var_min = 330
    var_max = 430
    n_bins = 5
    material_names = 'copper'
    output_skins = true
    output_base = "random_name"
    n_output = 4
    n_skip = 2
  []
[]

[Variables]
  [temperature]
    order = CONSTANT
    family = MONOMIAL
  []
[]
