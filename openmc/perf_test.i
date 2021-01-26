[Mesh]
  [meshcm]
    type = FileMeshGenerator
    file = copper_air_tetmesh.e
  []
[]

[Problem]
  type = OpenMCProblem
[]

[Executioner]
  type = OpenMCExecutioner
  variable = 'heating-local'
[]

[Variables]
  [heating-local]
      order = CONSTANT
      family = MONOMIAL
  []
[]

[UserObjects]
  [moab]
    type = MoabUserObject
  []
[]

[VectorPostprocessors]
  [./element_value_sampler]
    type = ElementValueSampler
    variable = 'heating-local'
    sort_by = id
    execute_on = 'final'
  [../]
[]

[Postprocessors]
  [./main_total_time]
    type = PerfGraphData
    section_name = "Root"
    data_type = total
    execute_on = 'final'
  [../]
  [./execute_total_time]
    type = PerfGraphData
    section_name = "MooseApp::execute"
    data_type = total
    execute_on = 'final'
  [../]
  [./executioner_total_time]
    type = PerfGraphData
    section_name = "OpenMCExecutioner::execute"
    data_type = total
    execute_on = 'final'
  [../]
  [./executioner_init_time]
    type = PerfGraphData
    section_name = "OpenMCExecutioner::init"
    data_type = total
    execute_on = 'final'
  [../]
  [./executioner_initopenmc_time]
    type = PerfGraphData
    section_name = "OpenMCExecutioner::initopenmc"
    data_type = total
    execute_on = 'final'
  [../]
  [./executioner_run_time]
    type = PerfGraphData
    section_name = "OpenMCExecutioner::run"
    data_type = total
    execute_on = 'final'
  [../]
[]

[Outputs]
  [csv]
    type = CSV
    execute_on = 'final'
    file_base = "perf_test"
  []
  [pgraph]
    type = PerfGraphOutput
    execute_on = 'final'  # Default is "final"
    level = 2 # Default is 1
  []
[]
