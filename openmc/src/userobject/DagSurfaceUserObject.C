// Moose includes
#include "DagSurfaceUserObject.h"
//#include "MooseTypes.h"
#include "MooseEnum.h"

registerMooseObject("OpenMCApp", DagSurfaceUserObject);

InputParameters
DagSurfaceUserObject::validParams()
{
  InputParameters params = UserObject::validParams();

  MooseEnum dag_bc_types("Vacuum Reflecting White Graveyard");
  params.addRequiredParam<MooseEnum>(
      "boundary_type",
      dag_bc_types,
      "Specify the type of DAGMC boundary coundition. Valid types are: Graveyard, Vacuum, Reflecting, White, Periodic");
  params.addRequiredParam<std::vector<std::string>>(
      "boundary_names",
      "Specify the boundary ids or names on which to apply this DagMC boundary condition");
  return params;
}

// Constructor
DagSurfaceUserObject::DagSurfaceUserObject(const InputParameters & parameters) :
  UserObject(parameters),
  boundary_type_(std::string(getParam<MooseEnum>("boundary_type"))),
  boundary_names_(getParam<std::vector<std::string>>("boundary_names"))
{
}
