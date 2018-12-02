{
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(0);
  TFile* file = TFile::Open("processed.root");
  TTree* tree = (TTree*) file->Get("procTree");
  TH1D* hE = new TH1D("hE", "Energy", 220, 0.00, 1.100);
  hE->SetLineColor(kRed);
  hE->SetXTitle("energy [MeV]");
  tree->Draw("energy >> hE", "detID==1");
  tree->Draw("energy", "detID==2", "SAME");
  hE->SetMinimum(0.1);
  c1->SetLogy();

  TLegend* legend = new TLegend(0.15, 0.7, 0.3, 0.85);
  legend->SetBorderSize(0);
  legend->AddEntry(hE, "detector 1", "L");
  legend->AddEntry(tree, "detector 2", "L");
  legend->Draw();
}

