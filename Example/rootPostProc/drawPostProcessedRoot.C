{
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(0);
  TFile* file = TFile::Open("processed.root");
  TTree* tree = (TTree*) file->Get("procTree");
  TH1D* hE = new TH1D("hE", "Energy", 4100, 0.00, 4100);
  hE->SetLineColor(kRed);
  hE->SetXTitle("energy [keV]");
  tree->Draw("energy*1000. >> hE");
  hE->SetMinimum(0.1);
  c1->SetLogy();
}

