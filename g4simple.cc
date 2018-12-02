#include <iostream>
#include <vector>
#include <string>
#include <regex>

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

    vector<regex> fPatterns;
    vector<int> fPatternIDs;
 
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
    vector<G4double> fT;
    vector<G4int> fVolID;
    vector<G4int> fIRep;

    map<G4VPhysicalVolume*, int> fVolIDMap;

  public:
    G4SimpleSteppingAction() : fNEvents(0), fEventNumber(0) {
      ResetVars(); 

      fVolIDCmd = new G4UIcommand("/g4simple/setVolID", this);
      fVolIDCmd->SetParameter(new G4UIparameter("pattern", 's', false));
      fVolIDCmd->SetParameter(new G4UIparameter("id", 'i', false));
      fVolIDCmd->SetGuidance("Volumes with name matching [pattern] will be given volume ID [id]");

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
        if(fOption == kEventWise && fPID.size()>0) WriteRow(man);
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
        int id;
        iss >> pattern >> id;
        if(id == 0 || id == -1) {
          cout << "Pattern " << pattern << ": Can't use ID = " << id << endl;
        }
        else {
          fPatterns.push_back(regex(pattern));
          fPatternIDs.push_back(id);
        }
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
      fT.clear();
      fVolID.clear();
      fIRep.clear();
    }

    void WriteRow(G4VAnalysisManager* man) {
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
        man->FillNtupleDColumn(row++, fT[i]);
        man->FillNtupleIColumn(row++, fVolID[i]);
        man->FillNtupleIColumn(row++, fIRep[i]);
      }
      // for event-wise, manager copies data from vectors over
      // automatically in the next line
      man->AddNtupleRow();
    }

    void UserSteppingAction(const G4Step *step) {
      G4VAnalysisManager* man = GetAnalysisManager();

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
      else {
        fEventNumber = G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID();
        static G4int lastEventID = fEventNumber;
        if(fEventNumber != lastEventID) {
          if(fOption == kEventWise && fPID.size()>0) WriteRow(man);
          ResetVars();
          lastEventID = fEventNumber;
        }
      }

      // post-step point will always work: only need to use the pre-step point
      // on the first step, for which the pre-step volume is always the same as
      // the post-step volume
      G4VPhysicalVolume* vpv = step->GetPostStepPoint()->GetPhysicalVolume();
      G4int id = fVolIDMap[vpv];
      if(id == 0 && fPatterns.size() > 0) {
        string name = (vpv == NULL) ? "NULL" : vpv->GetName();
        for(size_t i=0; i<fPatterns.size(); i++) {
          if(regex_match(name, fPatterns[i])) {
            id = fPatternIDs[i];
            break;
          }
        }
        if(id == 0 && !fRecordAllSteps) id = -1;
        fVolIDMap[vpv] = id;
      }

      // always record primary event info from pre-step of first step
      // if recording all steps, do this block to record prestep info
      if(fVolID.size() == 0 || (fRecordAllSteps && step->GetTrack()->GetCurrentStepNumber() == 1)) {
        fVolID.push_back(id == -1 ? 0 : id);
        fPID.push_back(step->GetTrack()->GetParticleDefinition()->GetPDGEncoding());
        fTrackID.push_back(step->GetTrack()->GetTrackID());
        fParentID.push_back(step->GetTrack()->GetParentID());
        fStepNumber.push_back(step->GetTrack()->GetCurrentStepNumber());
        fKE.push_back(step->GetPreStepPoint()->GetKineticEnergy());
        fEDep.push_back(0);
        G4ThreeVector pos = step->GetPreStepPoint()->GetPosition();
        G4TouchableHandle vol = step->GetPreStepPoint()->GetTouchableHandle();
        G4ThreeVector lPos = vol->GetHistory()->GetTopTransform().TransformPoint(pos);
        fX.push_back(pos.x());
        fY.push_back(pos.y());
        fZ.push_back(pos.z());
        fLX.push_back(lPos.x());
        fLY.push_back(lPos.y());
        fLZ.push_back(lPos.z());
        fT.push_back(step->GetPreStepPoint()->GetGlobalTime());
        fIRep.push_back(vol->GetReplicaNumber());

        if(fOption == kStepWise) WriteRow(man);
      }

      // If not in a sensitive volume, get out of here.
      if(id == -1) return; 

      // Now record post-step info
      fVolID.push_back(id);
      fPID.push_back(step->GetTrack()->GetParticleDefinition()->GetPDGEncoding());
      fTrackID.push_back(step->GetTrack()->GetTrackID());
      fParentID.push_back(step->GetTrack()->GetParentID());
      fStepNumber.push_back(step->GetTrack()->GetCurrentStepNumber());
      fKE.push_back(step->GetTrack()->GetKineticEnergy());
      fEDep.push_back(step->GetTotalEnergyDeposit());
      G4ThreeVector pos = step->GetPostStepPoint()->GetPosition();
      G4TouchableHandle vol = step->GetPostStepPoint()->GetTouchableHandle();
      G4ThreeVector lPos = vol->GetHistory()->GetTopTransform().TransformPoint(pos);
      fX.push_back(pos.x());
      fY.push_back(pos.y());
      fZ.push_back(pos.z());
      fLX.push_back(lPos.x());
      fLY.push_back(lPos.y());
      fLZ.push_back(lPos.z());
      fT.push_back(step->GetPostStepPoint()->GetGlobalTime());
      fIRep.push_back(vol->GetReplicaNumber());

      if(fOption == kStepWise) WriteRow(man);
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
          if(!doMatching || regex_match(name, pattern)) cout << name << endl;
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

