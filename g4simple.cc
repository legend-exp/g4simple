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

  public:
    G4SimpleSteppingAction() : fNEvents(0), fEventNumber(0) {
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
      delete man;
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

    void PushData(G4int id, const G4Step* step, G4bool usePreStep, G4bool zeroEdep=false) {
      fVolID.push_back(id);
      fPID.push_back(step->GetTrack()->GetParticleDefinition()->GetPDGEncoding());
      fTrackID.push_back(step->GetTrack()->GetTrackID());
      fParentID.push_back(step->GetTrack()->GetParentID());
      fStepNumber.push_back(step->GetTrack()->GetCurrentStepNumber() - int(usePreStep));
      G4StepPoint* stepPoint = NULL;
      if(usePreStep) stepPoint = step->GetPreStepPoint();
      else stepPoint = step->GetPostStepPoint();
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
      man->FillNtupleIColumn(0, fNEvents);
      man->FillNtupleIColumn(1, fEventNumber);
      int row = 2;
      if(fOption == kStepWise) {
        size_t i = fPID.size()-1;
        man->FillNtupleIColumn(row++, fPID[i]);
        man->FillNtupleIColumn(row++, fTrackID[i]);
        man->FillNtupleIColumn(row++, fParentID[i]);
        man->FillNtupleIColumn(row++, fStepNumber[i]);
        man->FillNtupleDColumn(row++, fKE[i]);
        man->FillNtupleDColumn(row++, fEDep[i]);
        man->FillNtupleDColumn(row++, fX[i]);
        man->FillNtupleDColumn(row++, fY[i]);
        man->FillNtupleDColumn(row++, fZ[i]);
        man->FillNtupleDColumn(row++, fLX[i]);
        man->FillNtupleDColumn(row++, fLY[i]);
        man->FillNtupleDColumn(row++, fLZ[i]);
        man->FillNtupleDColumn(row++, fPdX[i]);
        man->FillNtupleDColumn(row++, fPdY[i]);
        man->FillNtupleDColumn(row++, fPdZ[i]);
        man->FillNtupleDColumn(row++, fT[i]);
        man->FillNtupleIColumn(row++, fVolID[i]);
        man->FillNtupleIColumn(row++, fIRep[i]);
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
        man->CreateNtupleIColumn("nEvents");
        man->CreateNtupleIColumn("event");
        if(fOption == kEventWise) {
          man->CreateNtupleIColumn("pid", fPID);
          man->CreateNtupleIColumn("trackID", fTrackID);
          man->CreateNtupleIColumn("parentID", fParentID);
          man->CreateNtupleIColumn("step", fStepNumber);
          man->CreateNtupleDColumn("KE", fKE);
          man->CreateNtupleDColumn("Edep", fEDep);
          man->CreateNtupleDColumn("x", fX);
          man->CreateNtupleDColumn("y", fY);
          man->CreateNtupleDColumn("z", fZ);
          man->CreateNtupleDColumn("lx", fLX);
          man->CreateNtupleDColumn("ly", fLY);
          man->CreateNtupleDColumn("lz", fLZ);
          man->CreateNtupleDColumn("pdx", fPdX);
          man->CreateNtupleDColumn("pdy", fPdY);
          man->CreateNtupleDColumn("pdz", fPdZ);
          man->CreateNtupleDColumn("t", fT);
          man->CreateNtupleIColumn("volID", fVolID);
          man->CreateNtupleIColumn("iRep", fIRep);
        }
        else if(fOption == kStepWise) {
          man->CreateNtupleIColumn("pid");
          man->CreateNtupleIColumn("trackID");
          man->CreateNtupleIColumn("parentID");
          man->CreateNtupleIColumn("step");
          man->CreateNtupleDColumn("KE");
          man->CreateNtupleDColumn("Edep");
          man->CreateNtupleDColumn("x");
          man->CreateNtupleDColumn("y");
          man->CreateNtupleDColumn("z");
          man->CreateNtupleDColumn("lx");
          man->CreateNtupleDColumn("ly");
          man->CreateNtupleDColumn("lz");
          man->CreateNtupleDColumn("pdx");
          man->CreateNtupleDColumn("pdy");
          man->CreateNtupleDColumn("pdz");
          man->CreateNtupleDColumn("t");
          man->CreateNtupleIColumn("volID");
          man->CreateNtupleIColumn("iRep");
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

      // Check if we are in a sensitive volume.
      // A track is "in" the volume of the pre-step point.
      G4int id = GetVolID(step->GetPreStepPoint());

      G4bool usePreStep = false;
      if(fRecordAllSteps) {
        if(step->GetTrack()->GetCurrentStepNumber() == 1) {
          PushData(id, step, usePreStep=true);
        }
        PushData(id, step, usePreStep=false);
        return;
      }

      // record primary event info from pre-step of first step of first track
      if(step->GetTrack()->GetTrackID() == 1 && step->GetTrack()->GetCurrentStepNumber() == 1) {
        PushData(id, step, usePreStep=true);
      }

      // Record post-step if in a sensitive volume and Edep > 0
      if(id != 0 && step->GetTotalEnergyDeposit() > 0) {
        PushData(id, step, usePreStep=false);
      }

      // Record first step point when entering a sensitive volume: it's the
      // post-step-point of the step where the phys vol pointer changes.
      // Have to do this last to make sure to write the last step of the
      // previous volume in case it is also sensitive
      if(step->GetPreStepPoint()->GetPhysicalVolume() != step->GetPostStepPoint()->GetPhysicalVolume()) {
        G4int post_id = GetVolID(step->GetPostStepPoint());
        if(post_id != 0) {
          G4bool zeroEdep = true;
          PushData(post_id, step, usePreStep=false, zeroEdep);
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

        long seed;
        devrandom.read((char*)(&seed), sizeof(long));

        // Negative seeds give nasty sequences for some engines. For example,
        // CLHEP's JamesRandom.cc contains a specific check for this. Might 
        // as well make all seeds positive; randomness is not affected (one 
        // bit of randomness goes unused).
        if (seed < 0) seed = -seed;

        CLHEP::HepRandom::setTheSeed(seed);
        cout << "CLHEP::HepRandom seed set to: " << seed << endl;
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

