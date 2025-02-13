#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <TROOT.h>
#include <TApplication.h>
#include <TStyle.h>
#include <TFile.h>
#include <TTree.h>
#include <TCanvas.h>
#include <TChain.h>
#include <TBranch.h>
#include <TH1.h>
#include <TH2.h>
#include <TH3.h>
#include <TMath.h>
#include <TGraph.h>
#include <TVector3.h>
#include <TRandom3.h>

#include "WCSimRootEvent.hh"
#include "WCSimRootGeom.hh"
#include "WCSimRootOptions.hh"

// Mock up digitization code copied from WCSIM
// Currently contains BoxandLine20inchHQE and PMT3inchR14374 options
#include "OPTICALFIT/utils/WCSIMDigitization.hh"

#include "OPTICALFIT/utils/CalcGroupVelocity.hh"
#include "OPTICALFIT/utils/LEDProfile.hh"
#include "OPTICALFIT/utils/AttenuationZ.hh"

using namespace std;

TRandom3* rng;

WCSimRootGeom *geo = 0; 

// Simple example of reading a generated Root file
const int nPMTtypes = 2;
double PMTradius[nPMTtypes];

double CalcSolidAngle(double r, double R, double costh)
{
  double weight = 2*TMath::Pi()*(1-R/sqrt(R*R+r*r));
  //weight *= 1.-0.5*sqrt(1-costh*costh);
  //weight *= 0.5+0.5*costh;

  return weight;
}

void HelpMessage()
{
  std::cout << "USAGE: "
            << "WCSIM_TreeConvert" << "\nOPTIONS:\n"
            << "-f : Input file\n"
            << "-o : Output file\n"
            << "-l : Laser wavelength\n"
            << "-w : Apply diffuser profile reweight\n"
            << "-z : Reweighing attenuation factor with input slope\n"
            << "-p : Set water parameters for attenuation factor reweight (ABWFF,RAYFF)\n"
            << "-b : Use only B&L PMTs\n"
            << "-d : Run with raw Cherenkov hits and perform ad-hoc digitization\n"
            << "-t : Use separated triggers\n"
            << "-v : Verbose\n"
            << "-s : Start event\n"
            << "-e : End event\n"
            << "-r : RNG seed\n";
}

int main(int argc, char **argv){
  
  char * filename=NULL;
  char * outfilename=NULL;
  bool verbose=false;
  bool hybrid = true;
  double cvacuum = 3e8 ;//speed of light, in m/s.
  double nindex = 1.373;//refraction index of water
  bool plotDigitized = true; //using digitized hits
  bool separatedTriggers=false; //Assume two independent triggers, one for mPMT, one for B&L
  bool diffuserProfile = false; //Reweigh PMT hits by source angle
  bool zreweight = false; //Reweigh PMT hits by a z-dependence in attenuation length
  double slopeA = 0;
  double abwff = 1.3;
  double rayff = 0.75;

  double wavelength = 400; //wavelength in nm

  int nPMTpermPMT=19;

  int startEvent=0;
  int endEvent=0;
  int seed = 0;
  char c;
  while( (c = getopt(argc,argv,"f:o:b:s:e:l:r:z:p:hdtvw")) != -1 ){//input in c the argument (-f etc...) and in optarg the next argument. When the above test becomes -1, it means it fails to find a new argument.
    switch(c){
      case 'f':
        filename = optarg;
        break;
      case 'd':
        plotDigitized = false; //using raw hits
        break;
      case 'b':
        hybrid = false; // no mPMT
        break;
      case 't':
        separatedTriggers = true;
        break;
      case 'v':
        verbose = true;
        break;
      case 'o':
	      outfilename = optarg;
	      break;
      case 's':
	      startEvent = std::stoi(optarg);
        if (startEvent<0) startEvent = 0; 
	      break;
      case 'e':
	      endEvent = std::stoi(optarg);
	      break;
      case 'r':
	      seed = std::stoi(optarg);
        if (seed<0) seed=0;
        std::cout<<"Set RNG seed = "<<seed<<std::endl;
	      break;
      case 'l':
        wavelength = std::stod(optarg);
        if (wavelength<0) {
          std::cout<<"Wavelength < 0, using default = 400 nm"<<std::endl;
          wavelength = 400;
        }
	      break;
      case 'w':
        diffuserProfile = true;
        break;
      case 'z':
        slopeA = std::stod(optarg);
        if (fabs(slopeA)>1.e-9)
        {
          zreweight = true;
          std::cout<<"Reweighing attenuation factor with linear z-dependence, slope = "<<slopeA<<std::endl;
        }
        break;
      case 'p':
        {
          std::string wp = optarg;
          std::stringstream ss(wp);
          int count = 0;
          for(std::string s; std::getline(ss, s, ',');)
          {
            double val = std::stod(s);
            if (val>0)
            {
              if (count==0) abwff = val;
              else if (count==1) rayff = val;
            }
            count++;
          }
          std::cout<<"Setting new water paramters, ABWFF = "<<abwff<<", RAYFF = "<<rayff<<std::endl;
          break;
        }
      case 'h':
        HelpMessage();
      default:
        return 0;
    }
  }
  
  // Open the file
  if (filename==NULL){
    std::cout << "Error, no input file: " << std::endl;
    HelpMessage();
    return -1;
  }
  TFile *file = TFile::Open(filename);
  if (!file->IsOpen()){
    std::cout << "Error, could not open input file: " << filename << std::endl;
    return -1;
  }
  
  double vg = CalcGroupVelocity(wavelength);
  std::cout<<"Using wavelength = "<<wavelength<<" nm, group velocity = "<<vg<<" m/s, n = "<<cvacuum/vg<<std::endl;
  vg /= 1.e9; // convert to m/ns

  rng = new TRandom3(seed);
  gRandom = rng;

  // Get the a pointer to the tree from the file
  TTree *tree = (TTree*)file->Get("wcsimT");

  // Get the number of events
  int nevent = ((int)tree->GetEntries());//std::min(((int)tree->GetEntries()),100000);
  if(endEvent>0 && endEvent<=nevent) nevent = endEvent;
  if(verbose) printf("nevent %d\n",nevent);
  
  // Create a WCSimRootEvent to put stuff from the tree in

  WCSimRootEvent* wcsimrootsuperevent = new WCSimRootEvent();
  WCSimRootEvent* wcsimrootsuperevent2 = new WCSimRootEvent();

  // Set the branch address for reading from the tree
  TBranch *branch = tree->GetBranch("wcsimrootevent");
  branch->SetAddress(&wcsimrootsuperevent);
  // Force deletion to prevent memory leak 
  tree->GetBranch("wcsimrootevent")->SetAutoDelete(kTRUE);

  TBranch *branch2;
  if(hybrid){
    branch2 = tree->GetBranch("wcsimrootevent2");
    branch2->SetAddress(&wcsimrootsuperevent2);
  // Force deletion to prevent memory leak 
    tree->GetBranch("wcsimrootevent2")->SetAutoDelete(kTRUE);
  }

  // Geometry tree - only need 1 "event"
  TTree *geotree = (TTree*)file->Get("wcsimGeoT");
  geotree->SetBranchAddress("wcsimrootgeom", &geo);
  if(verbose) std::cout << "Geotree has " << geotree->GetEntries() << " entries" << std::endl;
  if (geotree->GetEntries() == 0) {
      exit(9);
  }
  geotree->GetEntry(0);
  PMTradius[0]=geo->GetWCPMTRadius();
  PMTradius[1]=geo->GetWCPMTRadius(true);
  //std::cout<<"geo->GetWCCylLength() = "<<geo->GetWCCylLength()<<std::endl;

  // Options tree - only need 1 "event"
  TTree *opttree = (TTree*)file->Get("wcsimRootOptionsT");
  WCSimRootOptions *opt = 0; 
  opttree->SetBranchAddress("wcsimrootoptions", &opt);
  if(verbose) std::cout << "Optree has " << opttree->GetEntries() << " entries" << std::endl;
  if (opttree->GetEntries() == 0) {
    exit(9);
  }
  opttree->GetEntry(0);
  opt->Print();

  // start with the main "subevent", as it contains most of the info
  // and always exists.
  WCSimRootTrigger* wcsimrootevent;
  WCSimRootTrigger* wcsimrootevent2;

  if(outfilename==NULL) outfilename = (char*)"out.root";
  
  TFile * outfile = new TFile(outfilename,"RECREATE");
  std::cout<<"File "<<outfilename<<" is open for writing"<<std::endl;

  double nHits, nPE, timetof;
  double weight;
  int nReflec, nRaySct, nMieSct;
  double photonStartTime;
  double nPE_digi, timetof_digi;
  int PMT_id;
  // TTree for storing the hit information. One for B&L PMT, one for mPMT
  TTree* hitRate_pmtType0 = new TTree("hitRate_pmtType0","hitRate_pmtType0");
  hitRate_pmtType0->Branch("nHits",&nHits); // dummy variable, always equal to 1
  hitRate_pmtType0->Branch("nPE",&nPE); // number of PE
  hitRate_pmtType0->Branch("timetof",&timetof); // hittime-tof
  hitRate_pmtType0->Branch("PMT_id",&PMT_id);
  hitRate_pmtType0->Branch("weight",&weight);
  // Branches below only filled for raw hits
  if (!plotDigitized)
  {
    hitRate_pmtType0->Branch("nReflec",&nReflec); // Number of reflection experienced by a photon before reaching the sensitive detector
    hitRate_pmtType0->Branch("nRaySct",&nRaySct); // Number of Rayleigh scattering
    hitRate_pmtType0->Branch("nMieSct",&nMieSct); // Number of Mie scattering
    hitRate_pmtType0->Branch("photonStartTime",&photonStartTime); // True photon start time
    hitRate_pmtType0->Branch("nPE_digi",&nPE_digi); // nPE after ad-hoc digitization
    hitRate_pmtType0->Branch("timetof_digi",&timetof_digi); // hittime-tof after ad-hoc digitization
  }
  TTree* hitRate_pmtType1 = new TTree("hitRate_pmtType1","hitRate_pmtType1");
  hitRate_pmtType1->Branch("nHits",&nHits);
  hitRate_pmtType1->Branch("nPE",&nPE);
  hitRate_pmtType1->Branch("timetof",&timetof);
  hitRate_pmtType1->Branch("PMT_id",&PMT_id);
  hitRate_pmtType1->Branch("weight",&weight);
  if (!plotDigitized)
  {
    hitRate_pmtType1->Branch("nReflec",&nReflec);
    hitRate_pmtType1->Branch("nRaySct",&nRaySct);
    hitRate_pmtType1->Branch("nMieSct",&nMieSct);
    hitRate_pmtType1->Branch("photonStartTime",&photonStartTime);
    hitRate_pmtType1->Branch("nPE_digi",&nPE_digi);
    hitRate_pmtType1->Branch("timetof_digi",&timetof_digi);
  }

  // Load diffuser position into memory
  double vtxpos[3];
  tree->GetEntry(0);
  wcsimrootevent = wcsimrootsuperevent->GetTrigger(0);
  for (int i=0;i<3;i++) vtxpos[i]=wcsimrootevent->GetVtx(i);

  // Reweight class for source angular profile
  LEDProfile* led;
  if (diffuserProfile) led = new LEDProfile();

  // Reweight class for z-dependence attenuation length
  AttenuationZ* attenZ;
  if (zreweight) attenZ = new AttenuationZ(wavelength,vtxpos[2],slopeA,abwff,rayff);

  // Save the PMT geometry information relative to source
  double dist, costh, cosths, phis, omega, phim, costhm, dz;
  int mPMT_id;
  TTree* pmt_type0 = new TTree("pmt_type0","pmt_type0");
  pmt_type0->Branch("R",&dist);          // distance to source
  pmt_type0->Branch("costh",&costh);     // photon incident angle relative to PMT
  pmt_type0->Branch("cosths",&cosths);   // PMT costheta angle relative to source
  pmt_type0->Branch("phis",&phis);       // PMT phi angle relative to source
  pmt_type0->Branch("costhm",&costhm);   // costhm = costh
  pmt_type0->Branch("phim",&phim);       // dummy
  pmt_type0->Branch("omega",&omega);     // solid angle subtended by PMT
  pmt_type0->Branch("dz",&dz);           // z-pos relative to source
  pmt_type0->Branch("PMT_id",&PMT_id);   // unique PMT id
  pmt_type0->Branch("mPMT_id",&mPMT_id); // dummy 
  pmt_type0->Branch("weight",&weight);   // weight from e.g. LED profile
  TTree* pmt_type1 = new TTree("pmt_type1","pmt_type1");
  pmt_type1->Branch("R",&dist);
  pmt_type1->Branch("costh",&costh);
  pmt_type1->Branch("cosths",&cosths);
  pmt_type1->Branch("phis",&phis);
  pmt_type1->Branch("costhm",&costhm);   // photon incident theta angle relative to central PMT 
  pmt_type1->Branch("phim",&phim);       // photon incident phi angle relative to central PMT 
  pmt_type1->Branch("omega",&omega);
  pmt_type1->Branch("dz",&dz);
  pmt_type1->Branch("PMT_id",&PMT_id);
  pmt_type1->Branch("mPMT_id",&mPMT_id); //sub-ID of PMT inside a mPMT module
                                         // 0 -11 : outermost ring
                                         // 12 - 17: middle ring
                                         // 18: central PMT
  pmt_type1->Branch("weight",&weight); 

  // Assume the "source" is the UK injection system, either diffuser or collimator
  // LI source direction is always perpendicular to the wall
  double vDirSource[3];
  double vSource_localXaxis[3];
  double vSource_localYaxis[3];
  double endcapZ = geo->GetWCCylLength()/2.*0.9; // cut value to determine whether the diffuser is in the barrel or endcap
  // Barrel injector
  if (abs(vtxpos[2])<endcapZ) {
    vDirSource[0]=vtxpos[0];
    vDirSource[1]=vtxpos[1];
    double norm = sqrt(vtxpos[0]*vtxpos[0]+vtxpos[1]*vtxpos[1]);
    vDirSource[0]/=-norm;
    vDirSource[1]/=-norm;
    vDirSource[2]=0;
    vSource_localXaxis[0]=0;vSource_localXaxis[1]=0;vSource_localXaxis[2]=1;
    vSource_localYaxis[0]=-vtxpos[1]/norm;vSource_localYaxis[1]=vtxpos[0]/norm;vSource_localYaxis[2]=0;
  } 
  else // endcap injector
  {
    vDirSource[0]=0;
    vDirSource[1]=0;
    if (vtxpos[2]>endcapZ) 
    {
      vDirSource[2]=-1;
      vSource_localXaxis[0]=1;vSource_localXaxis[1]=0;vSource_localXaxis[2]=0;
      vSource_localYaxis[0]=0;vSource_localYaxis[1]=-1;vSource_localYaxis[2]=0;
    }
    else 
    {
      vDirSource[2]=1;
      vSource_localXaxis[0]=1;vSource_localXaxis[1]=0;vSource_localXaxis[2]=0;
      vSource_localYaxis[0]=0;vSource_localYaxis[1]=1;vSource_localYaxis[2]=0;
    }
  }

  int nPMTs_type0=geo->GetWCNumPMT();
  int nPMTs_type1=0; if (hybrid) nPMTs_type1=geo->GetWCNumPMT(true);
  // reweight factor per PMT
  std::vector<double> ledweight_type0(nPMTs_type0,-1);
  std::vector<double> ledweight_type1(nPMTs_type1,-1);
  std::vector<double> atten_weight0(nPMTs_type0,-1);
  std::vector<double> atten_weight1(nPMTs_type1,-1);
  for (int pmtType=0;pmtType<nPMTtypes;pmtType++) 
  {
    int nPMTs_type = pmtType==0 ? nPMTs_type0 : nPMTs_type1;
    for (int i=0;i<nPMTs_type;i++) 
    {
      WCSimRootPMT pmt;
      if (pmtType==0) pmt = geo->GetPMT(i,false);
      else pmt = geo->GetPMT(i,true);
      PMT_id = i;
      if (pmtType == 0) mPMT_id = 0;
      else mPMT_id = PMT_id%nPMTpermPMT; // assume the PMT_id is ordered properly for mPMT
      double PMTpos[3];
      double PMTdir[3];                   
      for(int j=0;j<3;j++){
        PMTpos[j] = pmt.GetPosition(j);
        PMTdir[j] = pmt.GetOrientation(j);
      }
      double particleRelativePMTpos[3];
      for(int j=0;j<3;j++) particleRelativePMTpos[j] = PMTpos[j] - vtxpos[j];
      double vDir[3];double vOrientation[3];
      for(int j=0;j<3;j++){
        vDir[j] = particleRelativePMTpos[j];
        vOrientation[j] = PMTdir[j];
      }
      double Norm = TMath::Sqrt(vDir[0]*vDir[0]+vDir[1]*vDir[1]+vDir[2]*vDir[2]);
      double NormOrientation = TMath::Sqrt(vOrientation[0]*vOrientation[0]+vOrientation[1]*vOrientation[1]+vOrientation[2]*vOrientation[2]);
      for(int j=0;j<3;j++){
        vDir[j] /= Norm;
        vOrientation[j] /= NormOrientation;
      }
      dist = Norm;
      costh = -(vDir[0]*vOrientation[0]+vDir[1]*vOrientation[1]+vDir[2]*vOrientation[2]);
      cosths = vDir[0]*vDirSource[0]+vDir[1]*vDirSource[1]+vDir[2]*vDirSource[2];
      double localx = vDir[0]*vSource_localXaxis[0]+vDir[1]*vSource_localXaxis[1]+vDir[2]*vSource_localXaxis[2];
      double localy = vDir[0]*vSource_localYaxis[0]+vDir[1]*vSource_localYaxis[1]+vDir[2]*vSource_localYaxis[2];
      phis = atan2(localy,localx);
      weight = 1;
      if (diffuserProfile)
      {
        double wgt = led->GetLEDWeight(cosths,phis);
        weight *= wgt;
        if (pmtType==0) ledweight_type0[i]=wgt;
        if (pmtType==1) ledweight_type1[i]=wgt;
      }
      double pmtradius = pmtType==0 ? PMTradius[0] : PMTradius[1]; 
      omega = CalcSolidAngle(pmtradius,dist,costh);
      costhm = costh;
      phim = 0;
      // mPMT specific
      if (pmtType==1 && mPMT_id!=nPMTpermPMT-1)
      { 
        // Calculate photon incident cos(theta) and phi angle relative to central PMT 
        int idx_centralpmt = int(PMT_id/nPMTpermPMT)*nPMTpermPMT + nPMTpermPMT-1; // locate the central PMT
        double PMTpos_central[3], PMTdir_central[3];
        WCSimRootPMT pmt_central = geo->GetPMT(idx_centralpmt,true);
        for(int j=0;j<3;j++){
          PMTpos_central[j] = pmt_central.GetPosition(j)-PMTpos[j]; // central PMT position relative to current PMT
          PMTdir_central[j] = pmt_central.GetOrientation(j);
        }
        costhm = -(vDir[0]*PMTdir_central[0]+vDir[1]*PMTdir_central[1]+vDir[2]*PMTdir_central[2]);
        TVector3 v_central(PMTpos_central);
        TVector3 v_dir(vDir);
        TVector3 v_orientation(vOrientation);
        // Use cross product to extract the perpendicular component
        TVector3 v_dir1 = v_orientation.Cross(v_central);
        TVector3 v_dir2 = v_orientation.Cross(-v_dir);
        // Use dot cross to calculate the phi angle
        phim = v_dir1.Angle(v_dir2);
      }
      dz = PMTpos[2] - vtxpos[2];
      if (zreweight)
      {
        double wgt = attenZ->GetAttenuationZWeight(dist,dz);
        weight *= wgt;
        if (pmtType==0) atten_weight0[i]=wgt;
        if (pmtType==1) atten_weight1[i]=wgt;
      }
      if (pmtType==0) pmt_type0->Fill();
      if (pmtType==1) pmt_type1->Fill();
    }
  }
  outfile->cd();
  pmt_type0->Write();
  pmt_type1->Write();

  // Ad-hoc digitizer, PMT specific 
  BoxandLine20inchHQE_Digitizer* BnLDigitizer = new BoxandLine20inchHQE_Digitizer();
  PMT3inchR14374_Digitizer* mPMTDigitizer = new PMT3inchR14374_Digitizer();

  // Now loop over events
  for (int ev=startEvent; ev<nevent; ev++)
  {
    // Read the event from the tree into the WCSimRootEvent instance
    tree->GetEntry(ev);

    wcsimrootevent = wcsimrootsuperevent->GetTrigger(0);
    if(hybrid) wcsimrootevent2 = wcsimrootsuperevent2->GetTrigger(0);
    //wcsimrootevent2 = wcsimrootsuperevent2->GetTrigger(0);
    if(verbose){
      printf("********************************************************");
      printf("Evt, date %d %d\n", wcsimrootevent->GetHeader()->GetEvtNum(),
	     wcsimrootevent->GetHeader()->GetDate());
      printf("Mode %d\n", wcsimrootevent->GetMode());
      printf("Number of subevents %d\n",
	     wcsimrootsuperevent->GetNumberOfSubEvents());
      
      printf("Vtxvol %d\n", wcsimrootevent->GetVtxvol());
      printf("Vtx %f %f %f\n", wcsimrootevent->GetVtx(0),
	     wcsimrootevent->GetVtx(1),wcsimrootevent->GetVtx(2));
    }

    if(verbose){
      printf("Jmu %d\n", wcsimrootevent->GetJmu());
      printf("Npar %d\n", wcsimrootevent->GetNpar());
      printf("Ntrack %d\n", wcsimrootevent->GetNtrack());
    }

    std::vector<double> triggerInfo;
    triggerInfo.clear();
    triggerInfo = wcsimrootevent->GetTriggerInfo();

    std::vector<double> triggerInfo2;
    triggerInfo2.clear();
    if(hybrid) triggerInfo2 = wcsimrootevent2->GetTriggerInfo();


    if(verbose){
      for(unsigned int v=0;v<triggerInfo.size();v++){
	      cout << "Trigger entry #" << v << ", info = " << triggerInfo[v] << endl;
      }
      if(hybrid){
        for(unsigned int v=0;v<triggerInfo2.size();v++){
          cout << "Trigger2 entry #" << v << ", info = " << triggerInfo2[v] << endl;
        }
      }
    }

    double triggerShift[nPMTtypes];
    double triggerTime[nPMTtypes];
    for(int pmtType=0;pmtType<nPMTtypes;pmtType++){
      triggerShift[pmtType]=0;
      triggerTime[pmtType]=0;
      if(triggerInfo.size()>=3){
        if(pmtType==0){
          triggerShift[pmtType] = triggerInfo[1];
          triggerTime[pmtType] = triggerInfo[2];
        }
      }
      if(triggerInfo2.size()>=3){
        if(pmtType==1 && hybrid){
          triggerShift[pmtType] = triggerInfo2[1];
          triggerTime[pmtType] = triggerInfo2[2];
        }
      }
    }    
  

    int ncherenkovhits     = wcsimrootevent->GetNcherenkovhits();
    int ncherenkovdigihits = wcsimrootevent->GetNcherenkovdigihits(); 
    int ncherenkovhits2 = 0; if(hybrid) ncherenkovhits2 = wcsimrootevent2->GetNcherenkovhits();
    int ncherenkovdigihits2 = 0;if(hybrid) ncherenkovdigihits2 = wcsimrootevent2->GetNcherenkovdigihits(); 
    
    if(verbose){
      printf("node id: %i\n", ev);
      printf("Ncherenkovhits %d\n",     ncherenkovhits);
      printf("Ncherenkovdigihits %d\n", ncherenkovdigihits);
      printf("Ncherenkovhits2 %d\n",     ncherenkovhits2);
      printf("Ncherenkovdigihits2 %d\n", ncherenkovdigihits2);
      cout << "RAW HITS:" << endl;
    }

    if(!plotDigitized) for(int pmtType=0;pmtType<nPMTtypes;pmtType++){
      if(separatedTriggers){
        if(triggerInfo2.size()!=0 && pmtType==0) continue;
        if(triggerInfo.size()!=0 && pmtType==1) continue;
      }
      if(verbose) cout << "PMT Type = " << pmtType << endl;
 
      // Grab the big arrays of times and parent IDs
      
      TClonesArray *timeArray;//An array of pointers on CherenkovHitsTimes.
      if(pmtType==0) timeArray = wcsimrootevent->GetCherenkovHitTimes();
      else timeArray = wcsimrootevent2->GetCherenkovHitTimes();
      
      //double particleRelativePMTpos[3];
      double totalPe = 0;
      //int totalHit = 0;

      int nhits;
      if(pmtType == 0) nhits = ncherenkovhits;
      else nhits = ncherenkovhits2;

      // Get the number of Cherenkov hits
      // Loop over sub events
      // Loop through elements in the TClonesArray of WCSimRootCherenkovHits
      for (int i=0; i< nhits ; i++)
      {
        //if(verbose) cout << "Hit #" << i << endl;

        WCSimRootCherenkovHit *wcsimrootcherenkovhit;
        if(pmtType==0) wcsimrootcherenkovhit = (WCSimRootCherenkovHit*) (wcsimrootevent->GetCherenkovHits())->At(i);
        else wcsimrootcherenkovhit = (WCSimRootCherenkovHit*) (wcsimrootevent2->GetCherenkovHits())->At(i);
        
        int tubeNumber     = wcsimrootcherenkovhit->GetTubeID();
        int timeArrayIndex = wcsimrootcherenkovhit->GetTotalPe(0);
        int peForTube      = wcsimrootcherenkovhit->GetTotalPe(1);

        WCSimRootPMT pmt;
        if(pmtType == 0) pmt = geo->GetPMT(tubeNumber-1,false);
        else pmt  = geo->GetPMT(tubeNumber-1,true);

        PMT_id = tubeNumber-1;

        double PMTpos[3];
        //double PMTdir[3];
        for(int j=0;j<3;j++){
          PMTpos[j] = pmt.GetPosition(j);
          //PMTdir[j] = pmt.GetOrientation(j);
        }
        
        ////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////
        //double radius = TMath::Sqrt(PMTpos[0]*PMTpos[0] + PMTpos[1]*PMTpos[1]);
        //double phiAngle;
        //if(PMTpos[1] >= 0) phiAngle = TMath::ACos(PMTpos[0]/radius);
        //else phiAngle = TMath::Pi() + (TMath::Pi() - TMath::ACos(PMTpos[0]/radius));
        
        // Use vertex positions for LI events
        // for(int j=0;j<3;j++) particleRelativePMTpos[j] = PMTpos[j] - vtxpos[j];
          
        double vDir[3]; //double vOrientation[3];
        for(int j=0;j<3;j++){
          vDir[j] = PMTpos[j] - vtxpos[j];
          //vOrientation[j] = PMTdir[j];
        }
        double Norm = TMath::Sqrt(vDir[0]*vDir[0]+vDir[1]*vDir[1]+vDir[2]*vDir[2]);
        double tof = Norm*1e-2/(vg);
        //if(verbose) cout << "Time of flight = " << tof << endl;
        //double NormOrientation = TMath::Sqrt(vOrientation[0]*vOrientation[0]+vOrientation[1]*vOrientation[1]+vOrientation[2]*vOrientation[2]);
        //for(int j=0;j<3;j++){
          //vDir[j] /= Norm;
          //vOrientation[j] /= NormOrientation;
        //}
        
        WCSimRootCherenkovHitTime * HitTime = (WCSimRootCherenkovHitTime*) timeArray->At(timeArrayIndex);//Takes the first hit of the array as the timing, It should be the earliest hit
        //WCSimRootCherenkovHitTime HitTime = (WCSimRootCherenkovHitTime) timeArray->At(j);		  
        double time = HitTime->GetTruetime();

        photonStartTime = HitTime->GetPhotonStartTime();
        nReflec = 0;
        nRaySct = 0;
        nMieSct = 0;
        for (int idx = timeArrayIndex; idx<timeArrayIndex+peForTube; idx++) {
          WCSimRootCherenkovHitTime * cht = (WCSimRootCherenkovHitTime*) timeArray->At(idx);

          // only works well for peForTube = 1
          // if peForTube > 1, you don't know whether reflection and scattering happens at the same time for a single photon
          if (cht->GetReflection()>0) nReflec++;
          if (cht->GetRayScattering()>0) nRaySct++;
          if (cht->GetMieScattering()>0) nMieSct++;
        }
        ////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////
        
        timetof = time-tof;
        nHits = 1; nPE = peForTube; 

        // A simple way to mimic the digitization procedure, but triggering is not taken into account
        nPE_digi = nPE;
        timetof_digi = timetof;
        if (pmtType==0) BnLDigitizer->Digitize(nPE_digi,timetof_digi);
        if (pmtType==1) mPMTDigitizer->Digitize(nPE_digi,timetof_digi);

        weight = 1;
        if (diffuserProfile) 
        {
          weight *= pmtType==0 ? ledweight_type0[PMT_id] : ledweight_type1[PMT_id];   
        }
        if (zreweight)
        {
          weight *= pmtType==0 ? atten_weight0[PMT_id] : atten_weight1[PMT_id];   
        }
        nPE *= weight;  
        nPE_digi *= weight;  

        if (pmtType==0) hitRate_pmtType0->Fill();
        if (pmtType==1) hitRate_pmtType1->Fill();

      } // End of loop over Cherenkov hits
      if(verbose) cout << "Total Pe : " << totalPe << endl;
    }


    // Get the number of digitized hits
    // Loop over sub events
    if(verbose) cout << "DIGITIZED HITS:" << endl;

    if(plotDigitized) for(int pmtType=0;pmtType<nPMTtypes;pmtType++){
      if(separatedTriggers){
        if(triggerInfo2.size()!=0 && pmtType==0) continue;
        if(triggerInfo.size()!=0 && pmtType==1) continue;
      }
      if(verbose) cout << "PMT Type = " << pmtType << endl;
      // Grab the big arrays of times and parent IDs
      /*
      // Not used
      TClonesArray *timeArray;
      if(pmtType==0) timeArray = wcsimrootevent->GetCherenkovHitTimes();
      else timeArray = wcsimrootevent2->GetCherenkovHitTimes();
      */
      //double particleRelativePMTpos[3];
      double totalPe = 0;
      //int totalHit = 0;

      int nhits;
      if(pmtType == 0) nhits = ncherenkovdigihits;
      else nhits = ncherenkovdigihits2;
      
      // Loop through elements in the TClonesArray of WCSimRootCherenkovHits
      for (int i=0; i< nhits ; i++)
      { 
        TObject *Hit;
        if(pmtType==0) Hit = (wcsimrootevent->GetCherenkovDigiHits())->At(i);
        else Hit = (wcsimrootevent2->GetCherenkovDigiHits())->At(i);

        WCSimRootCherenkovDigiHit *wcsimrootcherenkovdigihit = 
          dynamic_cast<WCSimRootCherenkovDigiHit*>(Hit);
        
        int tubeNumber     = wcsimrootcherenkovdigihit->GetTubeId();
        double peForTube      = wcsimrootcherenkovdigihit->GetQ();

        //int tubeNumber     = wcsimrootcherenkovhit->GetTubeID();
        //int timeArrayIndex = wcsimrootcherenkovhit->GetTotalPe(0);
        //int peForTube      = wcsimrootcherenkovhit->GetTotalPe(1);
        WCSimRootPMT pmt;
        if(pmtType == 0) pmt = geo->GetPMT(tubeNumber-1,false);
        else pmt  = geo->GetPMT(tubeNumber-1,true); 

        PMT_id = (tubeNumber-1.);
        
        double PMTpos[3];
        //double PMTdir[3];                   
        for(int j=0;j<3;j++){
          PMTpos[j] = pmt.GetPosition(j);
          //PMTdir[j] = pmt.GetOrientation(j);
        }
        
        
        ////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////
        //double radius = TMath::Sqrt(PMTpos[0]*PMTpos[0] + PMTpos[1]*PMTpos[1]);
        //double phiAngle;
        //if(PMTpos[1] >= 0) phiAngle = TMath::ACos(PMTpos[0]/radius);
        //else phiAngle = TMath::Pi() + (TMath::Pi() - TMath::ACos(PMTpos[0]/radius));

        // Use vertex positions for LI events
        //for(int j=0;j<3;j++) particleRelativePMTpos[j] = PMTpos[j] - vtxpos[j];
        
        double vDir[3]; //double vOrientation[3];
        for(int j=0;j<3;j++){
          vDir[j] = PMTpos[j] - vtxpos[j];
          //vOrientation[j] = PMTdir[j];	  
        }
        double Norm = TMath::Sqrt(vDir[0]*vDir[0]+vDir[1]*vDir[1]+vDir[2]*vDir[2]);
        double tof = Norm*1e-2/(vg);
        //double NormOrientation = TMath::Sqrt(vOrientation[0]*vOrientation[0]+vOrientation[1]*vOrientation[1]+vOrientation[2]*vOrientation[2]);
        //for(int j=0;j<3;j++){
        //  vDir[j] /= Norm;
        //  vOrientation[j] /= NormOrientation;
        //}

        double time = wcsimrootcherenkovdigihit->GetT();
      
        ////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////

        timetof = time-tof+triggerTime[pmtType]-triggerShift[pmtType];

        nHits = 1; nPE = peForTube; 
        weight = 1;
        if (diffuserProfile) 
        {
          weight *= pmtType==0 ? ledweight_type0[PMT_id] : ledweight_type1[PMT_id];   
        }
        if (zreweight)
        {
          weight *= pmtType==0 ? atten_weight0[PMT_id] : atten_weight1[PMT_id];   
        }
        nPE *= weight;  
        if (pmtType==0) hitRate_pmtType0->Fill();
        if (pmtType==1) hitRate_pmtType1->Fill();


      } // End of loop over Cherenkov hits
      if(verbose) cout << "Total Pe : " << totalPe << std::endl; //", total hit : " << totalHit << endl;
    }

    // reinitialize super event between loops.
    wcsimrootsuperevent->ReInitialize();
    if(hybrid) wcsimrootsuperevent2->ReInitialize();
    
  } // End of loop over events

  outfile->cd();
  hitRate_pmtType0->Write();
  hitRate_pmtType1->Write();

  outfile->Close();
  
  return 0;
 }
