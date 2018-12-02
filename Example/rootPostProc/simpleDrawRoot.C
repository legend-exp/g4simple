// Example simple postprocessing in root: plot spectra of total Edep (>0) in each TubeVol 
{
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(0);
  TFile* file = TFile::Open("g4simpleout.root");
  TTree* tree = (TTree*) file->Get("g4sntuple");
  TH1D* hE = new TH1D("hE", "Summed Edep", 1100, 0.001, 1.101);
  hE->SetLineColor(kRed);
  hE->SetXTitle("energy [MeV]");
  tree->Draw("Sum$(Edep*(volID==2&&iRep==1)) >> hE");
  tree->Draw("Sum$(Edep*(volID==2&&iRep==2))", "", "SAME");
  hE->SetMinimum(0.1);
  c1->SetLogy();

  TLegend* legend = new TLegend(0.15, 0.7, 0.3, 0.85);
  legend->SetBorderSize(0);
  legend->AddEntry(hE, "detector 1", "L");
  legend->AddEntry(tree, "detector 2", "L");
  legend->Draw();
}
