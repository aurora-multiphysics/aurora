[Mesh]
  [offset-box]
    type = GeneratedMeshGenerator
    dim = 3
    nx = 1
    ny = 1
    nz = 1
    xmax = 5
    ymax = 5
    zmax = 5
    xmin = 4
    ymin = 4
    zmin = 4
    elem_type=TET4
  []
  [offset-box-id]
    type = SubdomainIDGenerator
    input = offset-box
    subdomain_id = 1
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
    block = 1
    compute = false
  []
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    # match up with variable below for this test
    bin_varname = "temperature"
    material_names = 'copper'
    output_skins = true
  []
[]

[Variables]
  [temperature]
    order = CONSTANT
    family = MONOMIAL
  []
[]
  
