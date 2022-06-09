// Moose includes
#include "FunctionUserObject.h"

registerMooseObject("AuroraApp", FunctionUserObject);

InputParameters
FunctionUserObject::validParams()
{
  InputParameters params = GeneralUserObject::validParams();

  params.addRequiredParam<std::string>(
      "variable", "Variable name to evaluate the mesh function on.");
  params.addParam<double>("tolerance", 1e-9,"Set point locator tolerance.");

  return params;
}

FunctionUserObject::FunctionUserObject(const InputParameters & parameters) :
  GeneralUserObject(parameters),
  _var_name(getParam<std::string>("variable")),
  tolerance(getParam<double>("tolerance"))
{}

void
FunctionUserObject::execute()
{

  // Fetch a pointer to the point locator object
  point_locator_ptr = mesh().getPointLocator();
  point_locator_ptr->set_close_to_point_tol(tolerance);

  // Fetch a pointer to the system containing named variable
  sysPtr = &_fe_problem.getSystem(_var_name);

  // Get the system number
  iSys = sysPtr->number();

  // Get the variable number
  iVar = sysPtr->variable_number(_var_name);

}

// Evaluate the mesh function at a point
Number
FunctionUserObject::value (const Point &p) const
{

  if(point_locator_ptr!=nullptr){
    // Find the element corresponding to this point
    const Elem * elemPtr = (*point_locator_ptr)(p);

    if(elemPtr == nullptr){
      throw std::logic_error("Could not locate element");
    }

    // Expect only one component, but check anyay
    unsigned int n_components = elemPtr->n_comp(iSys,iVar);
    if(n_components != 1){
      throw std::logic_error("Unexpected number of expected solution components");
    }

    // Get the degree of freedom number for this element
    dof_id_type soln_index = elemPtr->dof_number(iSys,iVar,0);

    // Get the solution value for this element
    Number value = sysPtr->solution->el(soln_index);

    return value;

  }
  else{
    throw std::logic_error("Point locator is null in FunctionUserObject.");
  }
}

MooseMesh&
FunctionUserObject::mesh()
{
  if(_fe_problem.haveDisplaced()){
    return _fe_problem.getDisplacedProblem()->mesh();
  }
  return _fe_problem.mesh();
}
