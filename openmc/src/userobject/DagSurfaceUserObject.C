// Moose includes
#include "DagSurfaceUserObject.h"
//#include "MooseTypes.h"
#include "MooseEnum.h"

registerMooseObject("OpenMCApp", DagSurfaceUserObject);

InputParameters
DagSurfaceUserObject::validParams()
{
  InputParameters params = UserObject::validParams();

  MooseEnum dag_bc_types("Vacuum Reflecting White Periodic Graveyard");
  params.addRequiredParam<MooseEnum>(
      "dag_bc_type",
      dag_bc_types,
      "Specify the type of DAGMC boundary coundition. Valid types are: Graveyard, Vacuum, Reflecting, White, Periodic");
  params.addRequiredParam<std::vector<BoundaryName>>(
      "boundary_names",
      "Specify the boundary ids or names on which to apply this DagMC boundary condition");
  return params;
}

// Constructor
DagSurfaceUserObject::DagSurfaceUserObject(const InputParameters & parameters) :
  UserObject(parameters)
  //, boundary_type(Utility::string_to_enum<DagBoundaryType>(getParam<MooseEnum>("boundary_type")))
  //,  boundary_names(getParam<std::vector<BoundaryName>("boundary_names"))
{
}
