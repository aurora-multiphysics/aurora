// Moose includes
#include "MoabMeshTransfer.h"

registerMooseObject("AuroraApp", MoabMeshTransfer);

template <>
InputParameters
validParams<MoabMeshTransfer>()
{
  InputParameters params = validParams<MultiAppTransfer>();

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
    std::cerr<<"Failed to initialise MoabMeshTransfer."<<std::endl;
    return;
  }

  // Transfer the mesh and systems into MOAB and initialise
  if(!transfer()){
    std::cerr<<"Failed to perform mesh transfer to into MOAB user object in MoabMeshTransfer"<<std::endl;
    return;
  }

}

void
MoabMeshTransfer::initialSetup()
{

  std::string apptype = _fe_problem.getMooseApp().type();
  if(apptype != apptype_from){
    std::cerr<<"From direction app is incorrect."<<std::endl;
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
    std::cerr<<"To direction app is incorrect."<<std::endl;
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
      std::cerr<<"Failed to fetch MOAB User object '"<<moabname<<"'. Please add one to your input file."<<std::endl;
      return false;
    }

  return true;
}
