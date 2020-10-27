// Moose includes
#include "MeshFunctionUserObject.h"

registerMooseObject("AuroraApp", MeshFunctionUserObject);

template <>
InputParameters
validParams<MeshFunctionUserObject>()
{
  InputParameters params = validParams<GeneralUserObject>();

  params.addRequiredParam<std::string>(
      "variable", "Variable name to evaluate the mesh function on.");
  return params;
}

MeshFunctionUserObject::MeshFunctionUserObject(const InputParameters & parameters) :
  GeneralUserObject(parameters),
  _var_name(getParam<std::string>("variable"))
{
  std::cout<<"Created MeshFunctionUserObject with name "<<this->name()
           <<" "<< &_fe_problem
           <<std::endl;
}

void
MeshFunctionUserObject::execute()
{
 
  // Get the system containing named variable
  System& sys = _fe_problem.getSystem(_var_name);

  // Get the equations systems that contains this system
  const EquationSystems & es = sys.get_equation_systems();

  // Get a reference to the solution vector
  const NumericVector< Number > & vec = *(sys.solution);
  
  // Get the degree of freedom map
  const DofMap & 	dofs = sys.get_dof_map();
  
  // Create a vector containing the variable number
  unsigned int iVar = sys.variable_number(_var_name);
  const std::vector< unsigned int > vars(iVar,1);
  
  _mesh_function = std::make_unique<MeshFunction>(es, vec, dofs, vars);
  _mesh_function->init();
}

// Evaluate the mesh function at a point
Number
MeshFunctionUserObject::value (const Point &p, const Real t) const
{
  if(_mesh_function!=nullptr)
    return (*_mesh_function)(p,t);
  else{
    return Number(0.);
  }
}

