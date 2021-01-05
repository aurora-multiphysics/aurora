[Mesh]
  [meshcm]
    type = FileMeshGenerator
    file = copper_air_bcs_tetmesh.e
  []
[]

[Problem]
  type = OpenMCProblem
[]

[Executioner]
  type = Steady
[]

[UserObjects]
  [moab]
    type = MoabUserObject
  []
[]
