// Moose includes
#include "MoabMeshTransfer.h"

registerMooseObject("AuroraApp", MoabMeshTransfer);

InputParameters
MoabMeshTransfer::validParams()
{
  InputParameters params = MultiAppTransfer::validParams();

  params.addParam<std::string>("apptype_from", "AuroraTestApp","Type of app from which we are taking mesh and systems.");
  params.addParam<std::string>("apptype_to", "OpenMCApp","Type of app to which we are sending mesh and systems.");
  params.addRequiredParam<std::string>(
      "moabname", "Name of the MoabUserObject to which we are transferring the mesh and systems.");

  return params;
}

MoabMeshTransfer::MoabMeshTransfer(const InputParameters & parameters) :
  MultiAppTransfer(parameters),
  isInit(false),
  _to_problem_index(0),
  apptype_from(getParam<std::string>("apptype_from")),
  apptype_to(getParam<std::string>("apptype_to")),
  moabname(getParam<std::string>("moabname"))
{
}

void
MoabMeshTransfer::execute()
{
  if(_direction==Transfer::DIRECTION::FROM_MULTIAPP) return;

  if(!isInit){
    mooseError("Failed to initialise MoabMeshTransfer.");
    return;
  }

  // Transfer the mesh and systems into MOAB and initialise
  if(!transfer()){
    mooseError("Failed to perform mesh transfer to into MOAB user object in MoabMeshTransfer");
    return;
  }

}

void
MoabMeshTransfer::initialSetup()
{

  std::string apptype = _fe_problem.getMooseApp().type();
  if(apptype != apptype_from){
    mooseError("From direction app is incorrect.");
    return;
  }

  getAppInfo();

  bool found_problem=false;
  for(unsigned int iprob=0; iprob<_to_problems.size();  iprob++){
    std::string apptype = _to_problems.at(iprob)->getMooseApp().type();
    if(apptype == apptype_to ){
      found_problem=true;
      _to_problem_index=iprob;
      break;
    }
  }
  if(!found_problem){
    mooseError("To direction app is incorrect.");
    return;
  }

  isInit=true;

}


bool
MoabMeshTransfer::transfer()
{

  try
    {
      // Fetch the problem of the app we are sending the mesh to
      FEProblemBase & problemTo =  *_to_problems.at(_to_problem_index);

      // Fetch a named instance of MOAB user object from the multi app problem
      MoabUserObject& moabUO = problemTo.getUserObject<MoabUserObject>(moabname);

      // Pass in the top level FE Problem
      moabUO.setProblem(&_fe_problem);

      // Initialise objects which are require to bin elements by temperature
      moabUO.initBinningData();

     }
  catch(std::exception &e)
    {
      mooseError("Failed to fetch MOAB User object '"+moabname+"'. Please add one to your input file.");
      return false;
    }

  return true;
}
