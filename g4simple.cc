#include <iostream>
#include <vector>

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

#include "TTree.h"
#include "TFile.h"


using namespace std;
using namespace CLHEP;


class G4SimpleSteppingAction : public G4UserSteppingAction, public G4UImessenger
{
  protected:
    G4UIcmdWithAString* fFileNameCmd;
    std::string fFileName;
    TFile* fFile;
    TTree* fTree;
 
    G4int fNEvents;

    vector<G4int> fPID; 
    vector<G4int> fTrackID;
    vector<G4int> fParentID;
    vector<G4double> fStepNumber;
    vector<G4double> fKE;
    vector<G4double> fEDep;
    vector<G4double> fX;
    vector<G4double> fY;
    vector<G4double> fZ;

  public:
    G4SimpleSteppingAction() : fFile(NULL), fTree(NULL), fNEvents(0) { 
      ResetVars(); 
      fFileNameCmd = new G4UIcmdWithAString("/g4simple/setOutputFileName", this);
      fFileNameCmd->SetGuidance("Set file name");
    }
    ~G4SimpleSteppingAction() { 
      if(fFile) { 
        if(fPID.size()>0) fTree->Fill();
        fTree->Write(); 
        fFile->Close(); 
      } 
      delete fFileNameCmd;
    } 

    void SetNewValue(G4UIcommand *command, G4String newValues) {
      if(command == fFileNameCmd) fFileName = newValues;
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
    }

    void UserSteppingAction(const G4Step *step) {
      if(fFile == NULL) {
	if(fFileName == "") fFile = TFile::Open("g4simpleout.root", "recreate");
	else fFile = TFile::Open(fFileName.c_str(), "recreate");
        fTree = new TTree("tree", "tree");
        fTree->Branch("pid", &fPID);
        fTree->Branch("trackID", &fTrackID);
        fTree->Branch("parentID", &fParentID);
        fTree->Branch("step", &fStepNumber);
        fTree->Branch("KE", &fKE);
        fTree->Branch("Edep", &fEDep);
        fTree->Branch("x", &fX);
        fTree->Branch("y", &fY);
        fTree->Branch("z", &fZ);
        fTree->Branch("nEvents", &fNEvents, "N/I");
        ResetVars();
        fNEvents = G4RunManager::GetRunManager()->GetCurrentRun()->GetNumberOfEventToBeProcessed();
      }
      else {
        G4int eventID = G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID();
        static G4int lastEventID = eventID;
        if(eventID != lastEventID) {
          if(fPID.size()>0) fTree->Fill();
          ResetVars();
          lastEventID = eventID;
        }
      }

      fPID.push_back(step->GetTrack()->GetParticleDefinition()->GetPDGEncoding());
      fTrackID.push_back(step->GetTrack()->GetTrackID());
      fParentID.push_back(step->GetTrack()->GetParentID());
      fStepNumber.push_back(step->GetTrack()->GetCurrentStepNumber());
      fKE.push_back(step->GetTrack()->GetKineticEnergy());
      fEDep.push_back(step->GetTotalEnergyDeposit());
      fX.push_back(step->GetPostStepPoint()->GetPosition().x());
      fY.push_back(step->GetPostStepPoint()->GetPosition().y());
      fZ.push_back(step->GetPostStepPoint()->GetPosition().z());
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
    G4UIcmdWithAString* fDetectorCmd;
    G4UIcmdWithABool* fRandomSeedCmd;

  public:
    G4SimpleRunManager() {
      fDirectory = new G4UIdirectory("/g4simple/");
      fDirectory->SetGuidance("Parameters for g4simple MC");

      fPhysListCmd = new G4UIcmdWithAString("/g4simple/setReferencePhysList", this);
      fPhysListCmd->SetGuidance("Set reference physics list to be used");
      //fPhysListCmd->SetCandidates("Shielding ShieldingNoRDM QGSP_BERT_HP");

      fDetectorCmd = new G4UIcmdWithAString("/g4simple/setDetectorGDML", this);
      fDetectorCmd->SetGuidance("Provide GDML filename specifying the detector construction");

      fRandomSeedCmd = new G4UIcmdWithABool("/g4simple/setRandomSeed", this);
      fRandomSeedCmd->SetParameterName("useURandom", true);
      fRandomSeedCmd->SetDefaultValue(false);
      fRandomSeedCmd->SetGuidance("Seed random number generator with a read from /dev/random");
      fRandomSeedCmd->SetGuidance("Set useURandom to true to read instead from /dev/urandom (faster but less random)");
    }

    ~G4SimpleRunManager() {
      delete fDirectory;
      delete fPhysListCmd;
      delete fDetectorCmd;
      delete fRandomSeedCmd;
    }

    void SetNewValue(G4UIcommand *command, G4String newValues) {
      if(command == fPhysListCmd) {
        SetUserInitialization((new G4PhysListFactory)->GetReferencePhysList(newValues));
        SetUserAction(new G4SimplePrimaryGeneratorAction); // must come after phys list
        SetUserAction(new G4SimpleSteppingAction); // must come after phys list
      }
      else if(command == fDetectorCmd) {
        G4GDMLParser parser;
        parser.Read(newValues);
        SetUserInitialization(new G4SimpleDetectorConstruction(parser.GetWorldVolume()));
      }
      else if(command == fDetectorCmd) {
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

