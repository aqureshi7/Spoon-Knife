#include <EventLoop/Job.h>
#include <EventLoop/StatusCode.h>
#include <EventLoop/Worker.h>

#include "xAODRootAccess/TEvent.h"
#include "xAODRootAccess/TStore.h"

#include "SUSYTools/SUSYObjDef_xAOD.h"

#include <RJigsawTools/WriteOutputNtuple.h>
#include <CommonTools/NtupManager.h>

#include <RJigsawTools/printDebug.h>
#include <RJigsawTools/strongErrorCheck.h>

#include <iostream>

// this is needed to distribute the algorithm to the workers
ClassImp(WriteOutputNtuple)

WriteOutputNtuple :: WriteOutputNtuple () :
m_ntupManager(nullptr)
{
  // Here you put any code for the base initialization of variables,
  // e.g. initialize all pointers to 0.  Note that you should only put
  // the most basic initialization here, since this method will be
  // called on both the submission and the worker node.  Most of your
  // initialization code will go into histInitialize() and
  // initialize().
}



EL::StatusCode WriteOutputNtuple :: setupJob (EL::Job& job)
{
  // Here you put code that sets up the job on the submission object
  // so that it is ready to work with your algorithm, e.g. you can
  // request the D3PDReader service or add output files.  Any code you
  // put here could instead also go into the submission script.  The
  // sole advantage of putting it here is that it gets automatically
  // activated/deactivated when you add/remove the algorithm from your
  // job, which may or may not be of value to you.
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode WriteOutputNtuple :: histInitialize ()
{
  // Here you do everything that needs to be done at the very
  // beginning on each worker node, e.g. create histograms and output
  // trees.  This method gets called before any input files are
  // connected.
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode WriteOutputNtuple :: fileExecute ()
{
  // Here you do everything that needs to be done exactly once for every
  // single file, e.g. collect a list of all lumi-blocks processed
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode WriteOutputNtuple :: changeInput (bool firstFile)
{
  // Here you do everything you need to do when we change input files,
  // e.g. resetting branch addresses on trees.  If you are using
  // D3PDReader or a similar service this method is not needed.
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode WriteOutputNtuple :: initialize ()
{
  // Here you do everything that you need to do after the first input
  // file has been connected and before the first event is processed,
  // e.g. create additional histograms based on which variables are
  // available in the input files.  You can also create all of your
  // histograms and trees in here, but be aware that this method
  // doesn't get called if no events are processed.  So any objects
  // you create here won't be available in the output if you have no
  // input events.
  ATH_MSG_INFO("writing to output file : " << outputName );
  ATH_MSG_INFO("region name            : " << regionName );
  ATH_MSG_INFO("systematic name        : " << systName );
  ATH_MSG_INFO("writing to tree        : " << regionName + systName );

  m_ntupManager = new NtupManager;
  m_ntupManager->initialize( outputName+"_"+regionName+"_"+systName, wk()->getOutputFile(outputName));//todo make the treename smarter

  return EL::StatusCode::SUCCESS;
}



EL::StatusCode WriteOutputNtuple :: execute ()
{
  // Here you do everything that needs to be done on every single
  // events, e.g. read input variables, apply cuts, and fill
  // histograms and trees.  This is where most of your actual analysis
  // code will go.
  xAOD::TStore * store = wk()->xaodStore();
  xAOD::TEvent* event = wk()->xaodEvent();

  const xAOD::EventInfo* eventInfo = 0;
  STRONG_CHECK(event->retrieve( eventInfo, "EventInfo"));

  // If it hasn't been selected in any of the regions from any of the select algs, don't bother writing anything out
  // Note: You can put in a "post-pre-selection" which can cut on e.g. RJR vars and set the region to a blank string
  // So this needn't be logically the same as the decision in CalculateRJigsawVariables::execute()


  //todo wtf is happening here
  std::string const & eventRegionName =  eventInfo->auxdecor< std::string >("regionName");
  ATH_MSG_DEBUG("Event falls in region: " << eventRegionName  );



  if( regionName == "" ) return EL::StatusCode::SUCCESS;

  ATH_MSG_DEBUG("Our event passed one of the selection " << regionName);

  // Furthermore! If the event doesn't pass this region def, don't write it out to this tree.
  if( eventRegionName != regionName ) return EL::StatusCode::SUCCESS;

  ATH_MSG_DEBUG("Storing map in output " << regionName  );

  std::map<std::string,double> * mymap = nullptr;
  if(store->contains<std::map<std::string,double> >("RJigsawVarsMap") ){
    STRONG_CHECK(store->retrieve( mymap,   "RJigsawVarsMap"));
    for (auto const& it : *mymap ) {
      ATH_MSG_VERBOSE("Storing map(key,value) into ntupManager: (" << it.first << " , " << it.second  << ")");
      m_ntupManager->setProperty(it.first,
  			       it.second
  			       );
    }
  }

  std::map<std::string,double> * mymapRegionVars = nullptr;
  STRONG_CHECK(store->retrieve( mymapRegionVars,   "RegionVarsMap"));

  for (auto const& it : *mymapRegionVars ) {
    ATH_MSG_VERBOSE("Storing map(key,value) into ntupManager: (" << it.first << " , " << it.second  << ")");
    m_ntupManager->setProperty(it.first,
			       it.second
			       );
  }


  std::map<std::string, std::vector<double> > * mymapVecRegionVars = nullptr;
  STRONG_CHECK(store->retrieve( mymapVecRegionVars,   "VecRegionVarsMap"));

  for (auto const& it : *mymapVecRegionVars ) {
    //    ATH_MSG_VERBOSE("Storing map(key,value) into ntupManager: (" << it.first << " , " << it.second  << ")");
    for(auto const& vecIt : it.second)
      m_ntupManager->pushProperty(it.first,//push for vector branches, use the same name and they will end up in the same branch
				  vecIt
				  );
  }



  m_ntupManager->fill();
  m_ntupManager->clear();

  printDebug();
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode WriteOutputNtuple :: postExecute ()
{
  // Here you do everything that needs to be done after the main event
  // processing.  This is typically very rare, particularly in user
  // code.  It is mainly used in implementing the NTupleSvc.
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode WriteOutputNtuple :: finalize ()
{
  // This method is the mirror image of initialize(), meaning it gets
  // called after the last event has been processed on the worker node
  // and allows you to finish up any objects you created in
  // initialize() before they are written to disk.  This is actually
  // fairly rare, since this happens separately for each worker node.
  // Most of the time you want to do your post-processing on the
  // submission node after all your histogram outputs have been
  // merged.  This is different from histFinalize() in that it only
  // gets called on worker nodes that processed input events.
  return EL::StatusCode::SUCCESS;
}



EL::StatusCode WriteOutputNtuple :: histFinalize ()
{
  // This method is the mirror image of histInitialize(), meaning it
  // gets called after the last event has been processed on the worker
  // node and allows you to finish up any objects you created in
  // histInitialize() before they are written to disk.  This is
  // actually fairly rare, since this happens separately for each
  // worker node.  Most of the time you want to do your
  // post-processing on the submission node after all your histogram
  // outputs have been merged.  This is different from finalize() in
  // that it gets called on all worker nodes regardless of whether
  // they processed input events.
  return EL::StatusCode::SUCCESS;
}
