// Moose includes
#include "VariableFunction.h"

registerMooseObject("AuroraApp", VariableFunction);

InputParameters
VariableFunction::validParams()
{
  // Get the Function input parameters
  InputParameters params = Function::validParams();


  // Add required parameters
  params.addRequiredParam<UserObjectName>("uoname",
                                          "Name of the FunctionUserObject name to retrieve the mesh function from.");

  return params;
}

VariableFunction::VariableFunction(const InputParameters & parameters) :
  Function(parameters),
  userObjName("uoname")
{}

void
VariableFunction::initialSetup()
{
  meshFunction = &getUserObject<FunctionUserObject>(userObjName);
}

Real
VariableFunction::value(Real /*t*/, const Point & p) const
{
  Real value(0.);
  bool exception=false;
  std::string errmsg("");

#ifdef LIBMESH_HAVE_OPENMP
#pragma omp critical
#endif
  {
    // If multi-threaded, execute one thread at a time because
    // meshFunction references the same UserObject
    // so need to avoid a data race
    // TODO: make this better
    if(!exception){
      exception = !getValue(p,value,errmsg);
    }
  }
  // End critical section

  if(exception){
    throw std::logic_error(errmsg);
  }

  // Syncronise threads here
#ifdef LIBMESH_HAVE_OPENMP
#pragma omp barrier
#endif

  return value;
}

bool
VariableFunction::getValue(const Point & p, Real& value, std::string& errmsg) const
{
  // We must catch any exceptions thrown within critical block
  // and rethrow otherwise program will exit
  try{
    if(meshFunction!=nullptr){
      value = Real(meshFunction->value(p));
    }
  }
  catch(std::logic_error& e){
    errmsg=e.what();
    return false;
  }

  return true;
}
