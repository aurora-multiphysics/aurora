// Moose includes
#include "DagSurfaceUserObject.C"
#include "MooseTypes.h"

registerMooseObject("OpenMCApp", DagSurfaceUserObject);

template <>
InputParameters
validParams<DagSurfaceUserObject>()
{
  InputParameters params = validParams<UserObject>();

  params.addRequiredParam<MooseEnum>(
      "boundary_type",
      "Specify the type of DAGMC boundary. Valid types are: Graveyard, Vacuum, Reflecting, White, Periodic");
  params.addRequiredParam<std::vector<BoundaryName>>(
      "boundary_names",
      "Specify the boundary ids or names on which to apply this DagMC boundary condition");
  return params;
}

// Constructor
DagSurfaceUserObject::DagSurfaceUserObject(const InputParameters & parameters) :
  UserObject(parameters),
  boundary_type(Utility::string_to_enum<DagBoundaryType>(getParam<MooseEnum>("boundary_type"))),
  boundary_names(getParam<std::vector<BoundaryName>("boundary_names"))
{
}
