#include <iostream>
#include <vector>
#include "G4RunManager.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4PVPlacement.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4GeneralParticleSource.hh"
#include "G4UIterminal.hh"
#include "G4UItcsh.hh"
#include "G4UImanager.hh"
#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4PhysListFactory.hh"
#include "G4Transform3D.hh"
#include "G4AssemblyVolume.hh"
#include "G4VisExecutive.hh"
#include "G4UserSteppingAction.hh"
#include "G4Track.hh"
#include "G4EventManager.hh"
#include "G4TrackingManager.hh"
#include "G4RunManager.hh"
#include "G4Run.hh"
#include "G4Neutron.hh"
#include "G4VisAttributes.hh"
#include "G4Color.hh"
#include "G4UIdirectory.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "TTree.h"
#include "TFile.h"
#include "TString.h"
#include "TUUID.h"

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
    G4double fDetectorE; // sum of all edeps for this event

    // target exiters
    vector<G4int> fTEPIDs; // target exiter particle IDs
    vector<G4double> fTEEnergies;
    vector<G4double> fTEMomentumCosTheta;

    // detector enterers
    vector<G4int> fDEPIDs; // detector interactors particle IDs
    vector<G4double> fDEEnergies;

  public:
    G4SimpleSteppingAction() : fFile(NULL), fTree(NULL), fNEvents(0) { 
      ResetVars(); 
      fFileNameCmd = new G4UIcmdWithAString("/g4simple/setFileName", this);
      fFileNameCmd->SetGuidance("Set file name");
    }
    ~G4SimpleSteppingAction() { 
      if(fFile) { 
        if(fDetectorE>0 || fTEPIDs.size()>0 || fDEPIDs.size()>0) fTree->Fill();
        fTree->Write(); 
        fFile->Close(); 
      } 
      delete fFileNameCmd;
    } 

    void SetNewValue(G4UIcommand *command, G4String newValues) {
      if(command == fFileNameCmd) fFileName = newValues;
    }

    void ResetVars() {
      fDetectorE = 0;
      fTEPIDs.clear();
      fTEEnergies.clear();
      fTEMomentumCosTheta.clear();
      fDEPIDs.clear();
      fDEEnergies.clear();
    }

    void UserSteppingAction(const G4Step *step) {
      if(fFile == NULL) {
        fNEvents = G4RunManager::GetRunManager()->GetCurrentRun()->GetNumberOfEventToBeProcessed();
	if(fFileName == "") {
          G4int muE_GeV_AsInt = (G4int) rint(step->GetPreStepPoint()->GetKineticEnergy()/GeV);
          fFile = TFile::Open(TString::Format("nuproblm_%dGeV.root", muE_GeV_AsInt), "recreate");
	}
	else fFile = TFile::Open(fFileName.c_str(), "recreate");
        fTree = new TTree("tree", "tree");
        fTree->Branch("nEvents", &fNEvents, "N/I");
        fTree->Branch("detectorE_MeV", &fDetectorE, "E/D");
        fTree->Branch("tePID", &fTEPIDs);
        fTree->Branch("teE", &fTEEnergies);
        fTree->Branch("tePCosQ", &fTEMomentumCosTheta);
        fTree->Branch("dePID", &fDEPIDs);
        fTree->Branch("deE", &fDEEnergies);
        ResetVars();
      }
      else {
        G4int eventID = G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID();
        static G4int lastEventID = eventID;
        if(eventID != lastEventID) {
          if(fDetectorE>0 || fTEPIDs.size()>0 || fDEPIDs.size()>0) fTree->Fill();
          ResetVars();
          lastEventID = eventID;
        }
      }

      G4double energyDep = step->GetTotalEnergyDeposit();
      fDetectorE += energyDep/MeV;

      G4VPhysicalVolume* preStepVol = step->GetPreStepPoint()->GetPhysicalVolume();
      G4VPhysicalVolume* postStepVol = step->GetPostStepPoint()->GetPhysicalVolume();
      if(preStepVol != postStepVol) {
        static G4VPhysicalVolume* world = NULL;
        static G4VPhysicalVolume* target = NULL;
        static G4VPhysicalVolume* detector = NULL;
        if(preStepVol != world && preStepVol != target && preStepVol != detector) {
          if(world == NULL && preStepVol->GetName() == "world") world = preStepVol;
          else if(target == NULL && preStepVol->GetName() == "target") target = preStepVol;
          else if(detector == NULL && preStepVol->GetName() == "detector") detector = preStepVol;
        }
        if(postStepVol != world && postStepVol != target && postStepVol != detector) {
          if(world == NULL && postStepVol->GetName() == "world") world = postStepVol;
          else if(target == NULL && postStepVol->GetName() == "target") target = postStepVol;
          else if(detector == NULL && postStepVol->GetName() == "detector") detector = postStepVol;
        }

        // target exiters
        if(preStepVol == target && postStepVol == world) {
          fTEPIDs.push_back(step->GetTrack()->GetParticleDefinition()->GetPDGEncoding());
          fTEEnergies.push_back(step->GetTrack()->GetKineticEnergy()/MeV);
          fTEMomentumCosTheta.push_back(step->GetTrack()->GetMomentumDirection().cosTheta());
        }
        // detector enterers
        else if(preStepVol == world && postStepVol == detector) {
          fDEPIDs.push_back(step->GetTrack()->GetParticleDefinition()->GetPDGEncoding());
          fDEEnergies.push_back(step->GetTrack()->GetKineticEnergy()/MeV);
        }
      }
    }

};



class G4SimpleDetectorConstruction : public G4VUserDetectorConstruction, public G4UImessenger
{
  public:
    G4VPhysicalVolume* Construct() {
      G4Material* vacuum = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic", false);
      G4Material* targetMat = G4NistManager::Instance()->FindOrBuildMaterial("G4_Pb", false);
      G4Material* detectorMat = G4NistManager::Instance()->FindOrBuildMaterial("G4_POLYETHYLENE", false);
      /*
      http://www.eljentechnology.com/index.php/joomla-overview/this-is-newest/73-ej-309
      http://www.eljentechnology.com/images/stories/Data_Sheets/Liquid_Scintillators/EJ309%20data%20sheet.pdf
      ATOMIC COMPOSITION
      No. of H Atoms per cm3 5.43 x 10 22
      No. of C Atoms per cm 3 4.35 x 10 22
      H:C. Ratio 1.25
      No. of Electrons per cm 3 3.16 x 10 23
      */

      G4VisAttributes* copperVisAtt = new G4VisAttributes(G4Colour(173./256., 111./256., 105./256.));
      G4VisAttributes* glassVisAtt = new G4VisAttributes(G4Colour(0.5, 0.5, 0.5, 0.3));

      G4Box* worldBox = new G4Box("worldBox", 10.0*m, 10.0*m, 10.0*m);
      G4LogicalVolume* worldL = new G4LogicalVolume(worldBox, vacuum, "worldL", 0, 0, 0);
      worldL->SetVisAttributes(G4VisAttributes::Invisible);

      G4double inch = 2.54*cm;
      G4Box* targetS = new G4Box("targetS", 2.*inch, 2.*inch, 4.*inch);
      G4LogicalVolume* targetL = new G4LogicalVolume(targetS, targetMat, "targetL", 0 ,0, 0);
      targetL->SetVisAttributes(copperVisAtt);
      new G4PVPlacement(G4Transform3D::Identity, targetL, "target", worldL, false, 0);

      G4Tubs* detectorS = new G4Tubs("detectorS", 0, 2.5*inch, 2.5*inch, 0, 2.0*pi);
      G4LogicalVolume* detectorL = new G4LogicalVolume(detectorS, detectorMat, "detectorL", 0 ,0, 0);
      detectorL->SetVisAttributes(glassVisAtt);
      G4Transform3D detectorPosRot = G4RotateY3D(90.*degree)*G4TranslateZ3D(1.5*m);
      new G4PVPlacement(detectorPosRot, detectorL, "detector", worldL, false, 0);

      return new G4PVPlacement(G4Transform3D::Identity, worldL, "world", 0, false, 0);
    }
};



class G4SimplePrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
  public:
    void GeneratePrimaries(G4Event* event) { fParticleGun.GeneratePrimaryVertex(event); } 
  private:
    G4GeneralParticleSource fParticleGun;
};



class G4SimpleRunManager : public G4RunManager, public G4UImessenger
{
  private:
    G4UIdirectory* fDirectory;
    G4UIcmdWithAString* fPhysListCmd;
    G4UIcmdWithoutParameter* fSeedWithUUIDCmd;

  public:
    G4SimpleRunManager() {
      SetUserInitialization(new G4SimpleDetectorConstruction);

      fDirectory = new G4UIdirectory("/g4simple/");
      fDirectory->SetGuidance("Parameters for g4simple MC");

      fPhysListCmd = new G4UIcmdWithAString("/g4simple/setReferencePhysList", this);
      fPhysListCmd->SetGuidance("Set reference physics list to be used");
      //fPhysListCmd->SetCandidates("Shielding ShieldingNoRDM QGSP_BERT_HP");

      fSeedWithUUIDCmd = new G4UIcmdWithoutParameter("/g4simple/seedWithUUID", this);
      fSeedWithUUIDCmd->SetGuidance("Seed random number generator quickly with an almost random number");
      fSeedWithUUIDCmd->SetGuidance("Generated using ROOT's TUUID class");
    }

    ~G4SimpleRunManager() {
      delete fDirectory;
      delete fPhysListCmd;
      delete fSeedWithUUIDCmd;
    }

    void SetNewValue(G4UIcommand *command, G4String newValues) {
      if(command == fPhysListCmd) {
        SetUserInitialization((new G4PhysListFactory)->GetReferencePhysList(newValues));
        SetUserAction(new G4SimpleSteppingAction);
        SetUserAction(new G4SimplePrimaryGeneratorAction);
      }
      else if(command == fSeedWithUUIDCmd) {
        TUUID uuid;
        UInt_t buffer[4];
        uuid.GetUUID((UChar_t*) buffer);
        long seed = (buffer[0] + buffer[1] + buffer[2] + buffer[3]);
        if (seed < 0) seed = -seed;
        CLHEP::HepRandom::setTheSeed(seed);
        cout << "CLHEP::HepRandom seed set to: " << seed << endl;
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

