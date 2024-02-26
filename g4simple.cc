#include <iostream>
#include <vector>
#include <string>
#include <regex>
#include <utility>

#include "G4RunManager.hh"
#include "G4Run.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4GeneralParticleSource.hh"
#include "G4UIterminal.hh"
#include "G4UItcsh.hh"
#include "G4UImanager.hh"
#include "G4PhysListFactory.hh"
#include "G4VisExecutive.hh"
#include "G4UserSteppingAction.hh"
#include "G4Track.hh"
#include "G4EventManager.hh"
#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithABool.hh"
#include "G4GDMLParser.hh"
#include "G4TouchableHandle.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4tgbVolumeMgr.hh"
#include "G4tgrMessenger.hh"
#include "G4UserLimits.hh"
#include "G4UnitsTable.hh"

#include "g4root.hh"
#include "g4xml.hh"
#include "g4csv.hh"
#ifdef GEANT4_USE_HDF5
#include "g4hdf5.hh"
#endif

using namespace std;
using namespace CLHEP;


class G4SimpleSteppingAction : public G4UserSteppingAction, public G4UImessenger
{
  protected:
    G4UIcommand* fVolIDCmd;
    G4UIcmdWithAString* fOutputFormatCmd;
    G4UIcmdWithAString* fOutputOptionCmd;
    G4UIcmdWithABool* fRecordAllStepsCmd;
    G4UIcmdWithAString* fSilenceOutputCmd;
    G4UIcmdWithAString* fAddOutputCmd;

    enum EFormat { kCsv, kXml, kRoot, kHdf5 };
    EFormat fFormat;
    enum EOption { kStepWise, kEventWise };
    EOption fOption;
    bool fRecordAllSteps;

    vector< pair<regex,string> > fPatternPairs;
 
    G4int fNEvents;
    G4int fEventNumber;
    vector<G4int> fPID; 
    vector<G4int> fTrackID;
    vector<G4int> fParentID;
    vector<G4int> fStepNumber;
    vector<G4double> fKE;
    vector<G4double> fEDep;
    vector<G4double> fX;
    vector<G4double> fY;
    vector<G4double> fZ;
    vector<G4double> fLX;
    vector<G4double> fLY;
    vector<G4double> fLZ;
    vector<G4double> fPdX;
    vector<G4double> fPdY;
    vector<G4double> fPdZ;
    vector<G4double> fT;
    vector<G4int> fVolID;
    vector<G4int> fIRep;

    map<G4VPhysicalVolume*, int> fVolIDMap;

    // bools for setting which fields to write to output
    G4bool fWEv, fWPid, fWTS, fWKE, fWEDep, fWR, fWLR, fWP, fWT, fWV;

  public:
    G4SimpleSteppingAction() : fNEvents(0), fEventNumber(0), 
      fWEv(true), fWPid(true), fWTS(true), fWKE(true), fWEDep(true),
      fWR(true), fWLR(true), fWP(true), fWT(true), fWV(true) 
    {
      ResetVars(); 

      fVolIDCmd = new G4UIcommand("/g4simple/setVolID", this);
      fVolIDCmd->SetParameter(new G4UIparameter("pattern", 's', false));
      fVolIDCmd->SetParameter(new G4UIparameter("replacement", 's', false));
      fVolIDCmd->SetGuidance("Volumes with name matching [pattern] will be given volume ID "
                             "based on the [replacement] rule. Replacement rule must produce an integer."
                             " Patterns which replace to 0 or -1 are forbidden and will be omitted.");

      fOutputFormatCmd = new G4UIcmdWithAString("/g4simple/setOutputFormat", this);
      string candidates = "csv xml root";
#ifdef GEANT4_USE_HDF5
      candidates += " hdf5";
#endif
      fOutputFormatCmd->SetCandidates(candidates.c_str());
      fOutputFormatCmd->SetGuidance("Set output format");
      fFormat = kCsv;

      fOutputOptionCmd = new G4UIcmdWithAString("/g4simple/setOutputOption", this);
      candidates = "stepwise eventwise";
      fOutputOptionCmd->SetCandidates(candidates.c_str());
      fOutputOptionCmd->SetGuidance("Set output option:");
      fOutputOptionCmd->SetGuidance("  stepwise: one row per step");
      fOutputOptionCmd->SetGuidance("  eventwise: one row per event");
      fOption = kStepWise;

      fRecordAllStepsCmd = new G4UIcmdWithABool("/g4simple/recordAllSteps", this);
      fRecordAllStepsCmd->SetParameterName("recordAllSteps", true);
      fRecordAllStepsCmd->SetDefaultValue(true);
      fRecordAllStepsCmd->SetGuidance("Write out every single step, not just those in sensitive volumes.");
      fRecordAllSteps = false;

      fSilenceOutputCmd = new G4UIcmdWithAString("/g4simple/silenceOutput", this);
      fSilenceOutputCmd->SetGuidance("Silence output fields");
      fAddOutputCmd = new G4UIcmdWithAString("/g4simple/addOutput", this);
      fAddOutputCmd->SetGuidance("Add output fields");
      candidates = "event pid track_step kinetic_energy energy_deposition ";
      candidates += "position local_position momentum time volume all";
      fSilenceOutputCmd->SetCandidates(candidates.c_str());
      fAddOutputCmd->SetCandidates(candidates.c_str());
    }

    G4VAnalysisManager* GetAnalysisManager() {
      if(fFormat == kCsv) return G4Csv::G4AnalysisManager::Instance();
      if(fFormat == kXml) return G4Xml::G4AnalysisManager::Instance();
      if(fFormat == kRoot) return G4Root::G4AnalysisManager::Instance();
      if(fFormat == kHdf5) {
#ifdef GEANT4_USE_HDF5
        return G4Hdf5::G4AnalysisManager::Instance();
#else
        cout << "Warning: You need to compile Geant4 with cmake flag "
             << "-DGEANT4_USE_HDF5 in order to generate the HDF5 output format.  "
             << "Reverting to ROOT." << endl;
        return G4Root::G4AnalysisManager::Instance();
#endif
      }
      cout << "Error: invalid format " << fFormat << endl;
      return NULL;
    }

    ~G4SimpleSteppingAction() { 
      G4VAnalysisManager* man = GetAnalysisManager();
      if(man->IsOpenFile()) {
        if(fOption == kEventWise && fPID.size()>0) WriteRow();
        man->Write();
        man->CloseFile();
      }
      delete fVolIDCmd;
      delete fOutputFormatCmd;
      delete fOutputOptionCmd;
      delete fRecordAllStepsCmd;
    } 

    void SetNewValue(G4UIcommand *command, G4String newValues) {
      if(command == fVolIDCmd) {
        istringstream iss(newValues);
        string pattern;
        string replacement;
        iss >> pattern >> replacement;
        fPatternPairs.push_back(pair<regex,string>(regex(pattern),replacement));
      }
      if(command == fOutputFormatCmd) {
        // also set recommended options.
        // override option by subsequent call to /g4simple/setOutputOption
        if(newValues == "csv") {
          fFormat = kCsv;
          fOption = kStepWise;
        }
        if(newValues == "xml") {
          fFormat = kXml;
          fOption = kEventWise;
        }
        if(newValues == "root") {
          fFormat = kRoot;
          fOption = kEventWise;
        }
        if(newValues == "hdf5") {
          fFormat = kHdf5;
          fOption = kStepWise;
        }
        GetAnalysisManager(); // call once to make all of the /analysis commands available
      }
      if(command == fOutputOptionCmd) {
        if(newValues == "stepwise") fOption = kStepWise;
        if(newValues == "eventwise") fOption = kEventWise;
      }
      if(command == fRecordAllStepsCmd) {
        fRecordAllSteps = fRecordAllStepsCmd->GetNewBoolValue(newValues);
      }
      if(command == fSilenceOutputCmd) {
        G4bool all = (newValues == "all");
        if(all || newValues == "event") fWEv = false;
        if(all || newValues == "pid") fWPid = false;
        if(all || newValues == "track_step") fWTS = false;
        if(all || newValues == "kinetic_energy") fWKE = false;
        if(all || newValues == "energy_deposition") fWEDep = false;
        if(all || newValues == "position") fWR = false;
        if(all || newValues == "local_position") fWLR = false;
        if(all || newValues == "momentum") fWP = false;
        if(all || newValues == "time") fWT = false;
        if(all || newValues == "volume") fWV = false;
      }
      if(command == fAddOutputCmd) {
        G4bool all = (newValues == "all");
        if(all || newValues == "event") fWEv = true;
        if(all || newValues == "pid") fWPid = true;
        if(all || newValues == "track_step") fWTS = true;
        if(all || newValues == "kinetic_energy") fWKE = true;
        if(all || newValues == "energy_deposition") fWEDep = true;
        if(all || newValues == "position") fWR = true;
        if(all || newValues == "local_position") fWLR = true;
        if(all || newValues == "momentum") fWP = true;
        if(all || newValues == "time") fWT = true;
        if(all || newValues == "volume") fWV = true;
      }
    }

    void ResetVars() {
      fPID.clear();
      fTrackID.clear();
      fParentID.clear();
      fStepNumber.clear();
      fKE.clear();
      fEDep.clear();
      fX.clear();
      fY.clear();
      fZ.clear();
      fLX.clear();
      fLY.clear();
      fLZ.clear();
      fPdX.clear();
      fPdY.clear();
      fPdZ.clear();
      fT.clear();
      fVolID.clear();
      fIRep.clear();
    }

    G4int GetVolID(G4StepPoint* stepPoint) {
      G4VPhysicalVolume* vpv = stepPoint->GetPhysicalVolume();
      G4int id = fVolIDMap[vpv];
      if(id == 0 && fPatternPairs.size() > 0) {
        string name = (vpv == NULL) ? "NULL" : vpv->GetName();
        for(auto& pp : fPatternPairs) {
          if(regex_match(name, pp.first)) {
            string replaced = regex_replace(name,pp.first,pp.second);
	    cout << "Setting ID for " << name << " to " << replaced << endl;
            int id_new = stoi(replaced);
            if (id_new == 0) {
              cout << "Volume " << name << ": Can't use ID = 0" << endl;
            } 
            else id = id_new;
            break;
          }
        }
        fVolIDMap[vpv] = id;
      }
      return id;
    }

    void PushData(const G4Step* step, G4bool usePreStep=false, G4bool zeroEdep=false) {
      // g4simple output convention:
      // Each two rows form a pre-post step point pair.
      // In G4 one is always "in" the vol of the prestep point in G4
      // However in g4simple the volID along with the Edep of the step get
      // recorded along with the poststep point info.
      // This means that boundary crossings are recorded in g4simple output at
      // the first step AFTER hitting the boundary.
      G4StepPoint* stepPoint = step->GetPreStepPoint();
      fVolID.push_back(GetVolID(stepPoint));
      if(!usePreStep) stepPoint = step->GetPostStepPoint();
      fPID.push_back(step->GetTrack()->GetParticleDefinition()->GetPDGEncoding());
      fTrackID.push_back(step->GetTrack()->GetTrackID());
      fParentID.push_back(step->GetTrack()->GetParentID());
      fStepNumber.push_back(step->GetTrack()->GetCurrentStepNumber() - int(usePreStep));
      fKE.push_back(stepPoint->GetKineticEnergy());
      if(usePreStep || zeroEdep) fEDep.push_back(0);
      else fEDep.push_back(step->GetTotalEnergyDeposit());
      G4ThreeVector pos = stepPoint->GetPosition();
      fX.push_back(pos.x());
      fY.push_back(pos.y());
      fZ.push_back(pos.z());
      G4TouchableHandle vol = stepPoint->GetTouchableHandle();
      G4ThreeVector lPos = vol->GetHistory()->GetTopTransform().TransformPoint(pos);
      fLX.push_back(lPos.x());
      fLY.push_back(lPos.y());
      fLZ.push_back(lPos.z());
      G4ThreeVector momDir = stepPoint->GetMomentumDirection();
      fPdX.push_back(momDir.x());
      fPdY.push_back(momDir.y());
      fPdZ.push_back(momDir.z());
      fT.push_back(stepPoint->GetGlobalTime());
      fIRep.push_back(vol->GetReplicaNumber());

      if(fOption == kStepWise) WriteRow();
    }

    void WriteRow() {
      G4VAnalysisManager* man = GetAnalysisManager();
      int iCol = 0;
      if(fWEv) man->FillNtupleIColumn(iCol++, fNEvents);
      if(fWEv) man->FillNtupleIColumn(iCol++, fEventNumber);
      if(fOption == kStepWise) {
        size_t i = fPID.size()-1;
        if(fWPid) man->FillNtupleIColumn(iCol++, fPID[i]);
        if(fWTS) man->FillNtupleIColumn(iCol++, fTrackID[i]);
        if(fWTS) man->FillNtupleIColumn(iCol++, fParentID[i]);
        if(fWTS) man->FillNtupleIColumn(iCol++, fStepNumber[i]);
        if(fWKE) man->FillNtupleDColumn(iCol++, fKE[i]);
        if(fWEDep) man->FillNtupleDColumn(iCol++, fEDep[i]);
        if(fWR) man->FillNtupleDColumn(iCol++, fX[i]);
        if(fWR) man->FillNtupleDColumn(iCol++, fY[i]);
        if(fWR) man->FillNtupleDColumn(iCol++, fZ[i]);
        if(fWLR) man->FillNtupleDColumn(iCol++, fLX[i]);
        if(fWLR) man->FillNtupleDColumn(iCol++, fLY[i]);
        if(fWLR) man->FillNtupleDColumn(iCol++, fLZ[i]);
        if(fWP) man->FillNtupleDColumn(iCol++, fPdX[i]);
        if(fWP) man->FillNtupleDColumn(iCol++, fPdY[i]);
        if(fWP) man->FillNtupleDColumn(iCol++, fPdZ[i]);
        if(fWT) man->FillNtupleDColumn(iCol++, fT[i]);
        if(fWV) man->FillNtupleIColumn(iCol++, fVolID[i]);
        if(fWV) man->FillNtupleIColumn(iCol++, fIRep[i]);
      }
      // for event-wise, manager copies data from vectors over
      // automatically in the next line
      man->AddNtupleRow();
    }

    void UserSteppingAction(const G4Step *step) {
      // This is the main function where we decide what to pull out and write
      // to an output file
      G4VAnalysisManager* man = GetAnalysisManager();

      // Open up a file if one is not open already
      if(!man->IsOpenFile()) {
        // need to create the ntuple before opening the file in order to avoid
        // writing error in csv, xml, and hdf5
        man->CreateNtuple("g4sntuple", "steps data");
        if(fWEv) man->CreateNtupleIColumn("nEvents");
        if(fWEv) man->CreateNtupleIColumn("event");
        if(fOption == kEventWise) {
          if(fWPid) man->CreateNtupleIColumn("pid", fPID);
          if(fWTS) man->CreateNtupleIColumn("trackID", fTrackID);
          if(fWTS) man->CreateNtupleIColumn("parentID", fParentID);
          if(fWTS) man->CreateNtupleIColumn("step", fStepNumber);
          if(fWKE) man->CreateNtupleDColumn("KE", fKE);
          if(fWEDep) man->CreateNtupleDColumn("Edep", fEDep);
          if(fWR) man->CreateNtupleDColumn("x", fX);
          if(fWR) man->CreateNtupleDColumn("y", fY);
          if(fWR) man->CreateNtupleDColumn("z", fZ);
          if(fWLR) man->CreateNtupleDColumn("lx", fLX);
          if(fWLR) man->CreateNtupleDColumn("ly", fLY);
          if(fWLR) man->CreateNtupleDColumn("lz", fLZ);
          if(fWP) man->CreateNtupleDColumn("pdx", fPdX);
          if(fWP) man->CreateNtupleDColumn("pdy", fPdY);
          if(fWP) man->CreateNtupleDColumn("pdz", fPdZ);
          if(fWT) man->CreateNtupleDColumn("t", fT);
          if(fWV) man->CreateNtupleIColumn("volID", fVolID);
          if(fWV) man->CreateNtupleIColumn("iRep", fIRep);
        }
        else if(fOption == kStepWise) {
          if(fWPid) man->CreateNtupleIColumn("pid");
          if(fWTS) man->CreateNtupleIColumn("trackID");
          if(fWTS) man->CreateNtupleIColumn("parentID");
          if(fWTS) man->CreateNtupleIColumn("step");
          if(fWKE) man->CreateNtupleDColumn("KE");
          if(fWEDep) man->CreateNtupleDColumn("Edep");
          if(fWR) man->CreateNtupleDColumn("x");
          if(fWR) man->CreateNtupleDColumn("y");
          if(fWR) man->CreateNtupleDColumn("z");
          if(fWLR) man->CreateNtupleDColumn("lx");
          if(fWLR) man->CreateNtupleDColumn("ly");
          if(fWLR) man->CreateNtupleDColumn("lz");
          if(fWP) man->CreateNtupleDColumn("pdx");
          if(fWP) man->CreateNtupleDColumn("pdy");
          if(fWP) man->CreateNtupleDColumn("pdz");
          if(fWT) man->CreateNtupleDColumn("t");
          if(fWV) man->CreateNtupleIColumn("volID");
          if(fWV) man->CreateNtupleIColumn("iRep");
        }
        else {
          cout << "ERROR: Unknown output option " << fOption << endl;
          return;
        }
        man->FinishNtuple();

        // look for filename set by macro command: /analysis/setFileName [name]
	if(man->GetFileName() == "") man->SetFileName("g4simpleout");
        cout << "Opening file " << man->GetFileName() << endl;
        man->OpenFile();

        ResetVars();
        fNEvents = G4RunManager::GetRunManager()->GetCurrentRun()->GetNumberOfEventToBeProcessed();
        fVolIDMap.clear();
      }

      // Get the event number for recording
      fEventNumber = G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID();
      static G4int lastEventID = fEventNumber;
      if(fEventNumber != lastEventID) {
        if(fOption == kEventWise && fPID.size()>0) WriteRow();
        ResetVars();
        lastEventID = fEventNumber;
      }

      // If writing out all steps, just write and return.
      G4bool usePreStep = true;
      if(fRecordAllSteps) {
        if(step->GetTrack()->GetCurrentStepNumber() == 1) {
          PushData(step, usePreStep);
        }
        PushData(step);
        return;
      }

      // Not writing out all steps:
      // First record primary event info from pre-step of first step of first track
      if(step->GetTrack()->GetTrackID() == 1 && step->GetTrack()->GetCurrentStepNumber() == 1) {
        PushData(step, usePreStep);
      }

      // Below here: writing out only steps in sensitive volumes (volID != 0)
      G4int preID = GetVolID(step->GetPreStepPoint());
      G4int postID = GetVolID(step->GetPostStepPoint());

      // Record step data if in a sensitive volume and Edep > 0
      if(preID != 0 && step->GetTotalEnergyDeposit() > 0) {
        // Record pre-step data for the first step of a E-depositing track in a
        // sens vol. Note: if trackID == 1, we already recorded it
        if(step->GetTrack()->GetCurrentStepNumber() == 1 && step->GetTrack()->GetTrackID() > 1) {
          PushData(step, usePreStep);
        }
        // Record post-step data for all sens vol steps
        PushData(step);
        return; // don't need to re-write poststep below if we already wrote it out
      }

      // Record the step point when an energy-depositing particle first enters a
      // sensitive volume: it's the post-step-point of the step where the phys
      // vol pointer changes.
      // Have to do this last because we might have already written it out
      // during the last step of the previous volume if it was also sensitive.
      G4bool zeroEdep = true;
      if(step->GetPreStepPoint()->GetPhysicalVolume() != step->GetPostStepPoint()->GetPhysicalVolume()) {
        if(postID != 0 && step->GetTotalEnergyDeposit() > 0) {
          PushData(step, usePreStep=false, zeroEdep);
        }
      }
    }

};


class G4SimplePrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
  public:
    void GeneratePrimaries(G4Event* event) { fParticleGun.GeneratePrimaryVertex(event); } 
  private:
    G4GeneralParticleSource fParticleGun;
};


class G4SimpleDetectorConstruction : public G4VUserDetectorConstruction
{ 
  public:
    G4SimpleDetectorConstruction(G4VPhysicalVolume *world = 0) { fWorld = world; }
    virtual G4VPhysicalVolume* Construct() { return fWorld; }
  private:
    G4VPhysicalVolume *fWorld;
};


class G4SimpleRunManager : public G4RunManager, public G4UImessenger
{
  private:
    G4UIdirectory* fDirectory;
    G4UIcmdWithAString* fPhysListCmd;
    G4UIcommand* fDetectorCmd;
    G4UIcommand* fTGDetectorCmd;
    G4UIcmdWithABool* fRandomSeedCmd;
    G4UIcmdWithAString* fListVolsCmd;
    G4UIcommand* fSetStepLimitCmd;

  public:
    G4SimpleRunManager() {
      fDirectory = new G4UIdirectory("/g4simple/");
      fDirectory->SetGuidance("Parameters for g4simple MC");

      fPhysListCmd = new G4UIcmdWithAString("/g4simple/setReferencePhysList", this);
      fPhysListCmd->SetGuidance("Set reference physics list to be used");

      fDetectorCmd = new G4UIcommand("/g4simple/setDetectorGDML", this);
      fDetectorCmd->SetParameter(new G4UIparameter("filename", 's', false));
      G4UIparameter* validatePar = new G4UIparameter("validate", 'b', true);
      validatePar->SetDefaultValue("true");
      fDetectorCmd->SetParameter(validatePar);
      fDetectorCmd->SetGuidance("Provide GDML filename specifying the detector construction");

      fTGDetectorCmd = new G4UIcommand("/g4simple/setDetectorTGFile", this);
      fTGDetectorCmd->SetParameter(new G4UIparameter("filename", 's', false));
      fTGDetectorCmd->SetGuidance("Provide text filename specifying the detector construction");

      fRandomSeedCmd = new G4UIcmdWithABool("/g4simple/setRandomSeed", this);
      fRandomSeedCmd->SetParameterName("useURandom", true);
      fRandomSeedCmd->SetDefaultValue(false);
      fRandomSeedCmd->SetGuidance("Seed random number generator with a read from /dev/random");
      fRandomSeedCmd->SetGuidance("Set useURandom to true to read instead from /dev/urandom (faster but less random)");

      fListVolsCmd = new G4UIcmdWithAString("/g4simple/listPhysVols", this);
      fListVolsCmd->SetParameterName("pattern", true);
      fListVolsCmd->SetGuidance("List name of all instantiated physical volumes");
      fListVolsCmd->SetGuidance("Optionally supply a regex pattern to only list matching volume names");
      fListVolsCmd->AvailableForStates(G4State_Idle, G4State_GeomClosed, G4State_EventProc);
      
      fSetStepLimitCmd = new G4UIcommand("/g4simple/setStepLimit", this);
      fSetStepLimitCmd->SetParameter(new G4UIparameter("stepLimitWithUnit", 's', false));
      fSetStepLimitCmd->SetParameter(new G4UIparameter("volNameRegex", 's', true));
      fSetStepLimitCmd->SetGuidance("Set maximum allowed step length with unit for volumes matching the provided regex (or all volumes if none is provided). Example: 1.0*um");
    }

    ~G4SimpleRunManager() {
      delete fDirectory;
      delete fPhysListCmd;
      delete fDetectorCmd;
      delete fTGDetectorCmd;
      delete fRandomSeedCmd;
      delete fListVolsCmd;
    }

    void SetNewValue(G4UIcommand *command, G4String newValues) {
      if(command == fPhysListCmd) {
        SetUserInitialization((new G4PhysListFactory)->GetReferencePhysList(newValues));
        SetUserAction(new G4SimplePrimaryGeneratorAction); // must come after phys list
        SetUserAction(new G4SimpleSteppingAction); // must come after phys list
      }
      else if(command == fDetectorCmd) {
        istringstream iss(newValues);
        string filename;
        string validate;
        iss >> filename >> validate;
        G4GDMLParser parser;
        parser.Read(filename, validate == "1" || validate == "true" || validate == "True");
        SetUserInitialization(new G4SimpleDetectorConstruction(parser.GetWorldVolume()));
      }
      else if(command == fTGDetectorCmd) {
        new G4tgrMessenger;
        G4tgbVolumeMgr* volmgr = G4tgbVolumeMgr::GetInstance();
        volmgr->AddTextFile(newValues);
        SetUserInitialization(new G4SimpleDetectorConstruction(volmgr->ReadAndConstructDetector()));
      }
      else if(command == fRandomSeedCmd) {
        bool useURandom = fRandomSeedCmd->GetNewBoolValue(newValues);
        string path = useURandom ?  "/dev/urandom" : "/dev/random";

        ifstream devrandom(path.c_str());
        if (!devrandom.good()) {
          cout << "setRandomSeed: couldn't open " << path << ". Your seed is not set." << endl;
          return;
        }

        long seed[2];
        devrandom.read((char*)(seed), sizeof(long));
        devrandom.read((char*)(seed+1), sizeof(long));

        CLHEP::HepRandom::setTheSeeds(seed, 2);
        cout << "CLHEP::HepRandom seeds set to: " << seed[0] << ' ' << seed[1] << endl;
        devrandom.close();
      }
      else if(command == fListVolsCmd) {
        regex pattern(newValues);
        bool doMatching = (newValues != "");
        G4PhysicalVolumeStore* volumeStore = G4PhysicalVolumeStore::GetInstance();
        cout << "Physical volumes";
        if(doMatching) cout << " matching pattern " << newValues;
        cout << ":" << endl;
        for(size_t i=0; i<volumeStore->size(); i++) {
          string name = volumeStore->at(i)->GetName();
	  int iRep = volumeStore->at(i)->GetCopyNo();
          if(!doMatching || regex_match(name, pattern)) cout << name << ' ' << iRep << endl;
        }
      }

      else if (command == fSetStepLimitCmd) {
        istringstream iss(newValues);
        G4String valueWithUnit, volNameRegex;
        // Read the value and unit as separate strings, then combine them
        G4double value;
        G4String unit;
        iss >> value >> unit;
        valueWithUnit = G4String(std::to_string(value)) + unit;
        // Check if there's argument for the volume name regex
        std::getline(iss >> std::ws, volNameRegex);
        // Apply the step limit using the parsed values
        ApplyStepLimit(valueWithUnit, volNameRegex);
      }
    }

    void ApplyStepLimit(const G4String& lengthAndUnit, const G4String& volNameRegex) {
      // Split the string into value and unit
      istringstream iss(lengthAndUnit);
      G4double value;
      G4String unit;
      iss >> value >> unit;
      // Convert the unit to a multiplier
      G4double unitMultiplier = G4UnitDefinition::GetValueOf(unit);
      // Convert the input value to mm using the unit multiplier
      G4double g4_step_max = value * unitMultiplier;
      // Apply the step limit to volumes
      G4PhysicalVolumeStore* volumeStore = G4PhysicalVolumeStore::GetInstance();
      std::regex pattern(volNameRegex);
      for (auto* vol : *volumeStore) {
        if (std::regex_match(vol->GetName(), pattern)) {
            G4LogicalVolume* logicalVolume = vol->GetLogicalVolume();
            if (!logicalVolume->GetUserLimits()) {
                logicalVolume->SetUserLimits(new G4UserLimits());
            }
            logicalVolume->GetUserLimits()->SetMaxAllowedStep(g4_step_max);
            G4cout << "Set step limit of " << lengthAndUnit << " for volume " << vol->GetName() << G4endl;
        }
      }
    }
};



int main(int argc, char** argv)
{
  if(argc > 2) {
    cout << "Usage: " << argv[0] << " [macro]" << endl;
    return 1;
  }

  G4SimpleRunManager* runManager = new G4SimpleRunManager;
  G4VisManager* visManager = new G4VisExecutive;
  visManager->Initialize();

  if(argc == 1) (new G4UIterminal(new G4UItcsh))->SessionStart();
  else G4UImanager::GetUIpointer()->ApplyCommand(G4String("/control/execute ")+argv[1]);

  delete visManager;
  delete runManager;
  return 0;
}

