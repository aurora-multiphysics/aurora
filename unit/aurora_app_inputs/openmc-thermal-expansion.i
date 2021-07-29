[Mesh]
  type = GeneratedMesh
  dim = 1
  nx = 1
[]

[Problem]
  type = OpenMCProblem
[]

[Executioner]
  type = OpenMCExecutioner
  variables = 'heating-local'
  err_variables = ''
  score_names = 'heating-local'
  tally_ids = '1'
  neutron_source = 1.0e18
  launch_threads=false
  n_threads=1
  no_scaling = true
[]

[UserObjects]
  [moab]
    type = MoabUserObject
    bin_varname = "temp"
    material_names = 'copper'
    material_openmc_names = 'copper'
    #output_skins = true
    length_scale = 1.0
    graveyard_scale_inner = 1.05
    n_bins = 1
    var_max = 1200.
  []
[]

[Outputs]
  console=false
[]
