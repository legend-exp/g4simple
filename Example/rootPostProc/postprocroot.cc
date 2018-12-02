#include <iostream>
#include "TFile.h"
#include "TTree.h"
#include "TTreeReader.h"
#include "TTreeReaderValue.h"
#include "TRandom.h"

using namespace std;

int main(int argc, char** argv)
{
  double pctResAt1MeV = 0.15;

  if(argc != 2) {
    cout << "Usage: postprocroot [filename.root]" << endl;
    return 1;
  }

  TFile* file = TFile::Open(argv[1]);
  if(file == NULL) return 1;

  TTree* tree = (TTree*) file->Get("g4sntuple");
  if(tree == NULL) {
    cout << "Couldn't find g4stree in file" << endl;
    return 1;
  }

  TTreeReader treeReader(tree);
  TTreeReaderValue< vector<double> > Edep(treeReader, "Edep");
  TTreeReaderValue< vector<int> > volID(treeReader, "volID");
  TTreeReaderValue< vector<int> > iRep(treeReader, "iRep");

  TFile* outFile = TFile::Open("processed.root", "recreate");
  TTree* outTree = new TTree("procTree", "procTree");

  vector<double> energy;
  outTree->Branch("energy", &energy);
  vector<int> detID;
  outTree->Branch("detID", &detID);
  vector<int> volIDOut;
  outTree->Branch("volID", &volIDOut);
  
  while(treeReader.Next()) {
    energy.clear();
    detID.clear();
    volIDOut.clear();
    map<pair<int,int>,double> hits;
    for(size_t i=0; i < Edep->size(); i++) {
      if(volID->at(i) != 1) continue;
      hits[pair<int,int>(volID->at(i),iRep->at(i))] += Edep->at(i);
    }
    for(auto& hit : hits) {
      if(hit.second == 0) continue;
      volIDOut.push_back(hit.first.first);
      detID.push_back(hit.first.second);
      double e0 = hit.second;
      double sigma = pctResAt1MeV/100.*sqrt(e0);
      energy.push_back(gRandom->Gaus(e0, sigma));
    }
    if(energy.size() > 0) outTree->Fill();
  }

  outTree->Write();
  outFile->Close();

  return 0;
}
