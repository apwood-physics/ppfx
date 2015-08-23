#include "MakeReweight.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace NeutrinoFluxReweight{

  MakeReweight* MakeReweight::instance = 0;
  MakeReweight::MakeReweight(){
  }
 void MakeReweight::SetOptions(std::string fileIn){
    fileOptions = fileIn;
    ParseOptions();
    Initialize();
  }

  void MakeReweight::ParseOptions(){
    //Parsing the file input:
    using boost::property_tree::ptree;
    ptree top;
    read_xml(fileOptions.c_str(),top,2); // option 2 removes comment strings
    ptree& options = top.get_child("inputs.Settings");

    mippCorrOption = options.get<std::string>("MIPPCorrOption");
    Nuniverses = options.get<int>("NumberOfUniverses");
    
  }
  void MakeReweight::Initialize(){
    
    //Getting MIPP binning and the correlation parameters:
    CentralValuesAndUncertainties* cvu = CentralValuesAndUncertainties::getInstance();;
    MIPPNumiYieldsBins*  myb =  MIPPNumiYieldsBins::getInstance(); 
    ThinTargetBins*  thinbin =  ThinTargetBins::getInstance(); 
    MIPPNumiMC*  mymc =  MIPPNumiMC::getInstance(); 
    const char* ppfxDir = getenv("PPFX_DIR");
    
    std::cout<<"Initializing correlation parameters"<<std::endl;
    cvu->readFromXML(Form("%s/uncertainties/Parameters_%s.xml",ppfxDir,mippCorrOption.c_str()));
    
    std::cout<<"Initializing bin data conventions"<<std::endl;
    myb->pip_data_from_xml(Form("%s/data/BINS/MIPPNumiData_PIP_Bins.xml",ppfxDir));
    myb->pim_data_from_xml(Form("%s/data/BINS/MIPPNumiData_PIM_Bins.xml",ppfxDir));
    myb->k_pi_data_from_xml(Form("%s/data/BINS/MIPPNumiData_K_PI_Bins.xml",ppfxDir));
    thinbin->pC_pi_from_xml(Form("%s/data/BINS/ThinTarget_pC_pi_Bins.xml",ppfxDir));
    thinbin->barton_pC_pi_from_xml(Form("%s/data/BINS/ThinTargetBarton_pC_pi_Bins.xml",ppfxDir));
    thinbin->pC_k_from_xml(Form("%s/data/BINS/ThinTargetLowxF_pC_k_Bins.xml",ppfxDir));
    thinbin->mipp_pC_k_pi_from_xml(Form("%s/data/BINS/ThinTarget_K_PI_Bins.xml",ppfxDir));
    thinbin->meson_incident_from_xml(Form("%s/data/BINS/ThinTarget_MesonIncident.xml",ppfxDir));
    thinbin->pC_p_from_xml(Form("%s/data/BINS/ThinTarget_pC_p_Bins.xml",ppfxDir));
    thinbin->pC_n_from_xml(Form("%s/data/BINS/ThinTarget_pC_n_Bins.xml",ppfxDir));
    thinbin->material_scaling_from_xml(Form("%s/data/BINS/ThinTarget_material_scaling_Bins.xml",ppfxDir));
    
    std::cout<<"Initializing MC values"<<std::endl;
    mymc->pip_mc_from_xml(Form("%s/data/MIPP/MIPPNuMI_MC_PIP.xml",ppfxDir));
    mymc->pim_mc_from_xml(Form("%s/data/MIPP/MIPPNuMI_MC_PIM.xml",ppfxDir));
    mymc->kap_mc_from_xml(Form("%s/data/MIPP/MIPPNuMI_MC_KAP.xml",ppfxDir));
    mymc->kam_mc_from_xml(Form("%s/data/MIPP/MIPPNuMI_MC_KAM.xml",ppfxDir));

    //Reweighter drivers:
    vec_rws.reserve(Nuniverses);
    std::cout<<"Initializing reweight drivers for "<<Nuniverses<<" universes"<<std::endl;
    
    const int base_universe=1000000;
    cvPars.reserve(Nuniverses+1);
    univPars.reserve(Nuniverses+1);
    for(int ii=0;ii<Nuniverses;ii++){
      std::cout<<"Loading parameters for universe: "<<ii<<std::endl;
      cvPars.push_back(cvu->getCVPars());
      univPars.push_back(cvu->calculateParsForUniverse(ii+base_universe));
      vec_rws.push_back(new ReweightDriver(ii,cvPars[ii],univPars[ii],fileOptions));
      vec_wgts.push_back(1.0);
    }
    
    //Central Value driver:
    //by convention, we use universe -1 to hold the cv. It is in agreement with CentralValueAndUncertainties  
    const int cv_id = -1;
    std::cout<<"Loading parameters for cv"<<std::endl;
    cvPars.push_back(cvu->getCVPars());
    univPars.push_back(cvu->calculateParsForUniverse(cv_id));
    cv_rw = new ReweightDriver(cv_id,cvPars[Nuniverses],univPars[Nuniverses],fileOptions); 

    cv_wgt = 1.0;

    std::cout<<"Done configuring universes"<<std::endl;

  }
  
  std::vector<double> MakeReweight::GetTotalWeights(){
    return vec_wgts;
  }
  /////
  void MakeReweight::calculateWeights(nu_g4numi* nu, const char* tgtcfg, const char* horncfg){
    InteractionChainData* inter_chain = new InteractionChainData(nu,tgtcfg,horncfg);
    doTheJob(inter_chain);
  }
  /////
  void MakeReweight::calculateWeights(bsim::Dk2Nu* nu, bsim::DkMeta* meta){  
    InteractionChainData* inter_chain = new InteractionChainData(nu,meta);
    doTheJob(inter_chain);
  }
  /////
  void MakeReweight::doTheJob(InteractionChainData* icd){
  
    //universe calculation:
    map_rew_wgts.clear();
    for(int ii=0;ii<Nuniverses;ii++){
      vec_wgts[ii] = vec_rws[ii]->calculateWeight(*icd);

      map_rew_wgts["MIPPNumiPionYields"].push_back(vec_rws[ii]->mipp_pion_wgt);
      map_rew_wgts["MIPPNumiKaonYields"].push_back(vec_rws[ii]->mipp_kaon_wgt);
      
      map_rew_wgts["TargetAttenuation"].push_back(vec_rws[ii]->att_wgt);            
      map_rew_wgts["AbsorptionIC"].push_back(vec_rws[ii]->abs_ic_wgt);
      map_rew_wgts["AbsorptionDPIP"].push_back(vec_rws[ii]->abs_dpip_wgt);
      map_rew_wgts["AbsorptionDVOL"].push_back(vec_rws[ii]->abs_dvol_wgt);
      map_rew_wgts["AbsorptionNucleon"].push_back(vec_rws[ii]->abs_nucleon_wgt);
      map_rew_wgts["AbsorptionOther"].push_back(vec_rws[ii]->abs_other_wgt);
      map_rew_wgts["TotalAbsorption"].push_back(vec_rws[ii]->tot_abs_wgt);
      
      map_rew_wgts["ThinTargetpCPion"].push_back(vec_rws[ii]->pC_pi_wgt);
      map_rew_wgts["ThinTargetpCKaon"].push_back(vec_rws[ii]->pC_k_wgt);
      map_rew_wgts["ThinTargetnCPion"].push_back(vec_rws[ii]->nC_pi_wgt);
      map_rew_wgts["ThinTargetpCNucleon"].push_back(vec_rws[ii]->pC_nu_wgt);
      map_rew_wgts["ThinTargetMesonIncident"].push_back(vec_rws[ii]->meson_inc_wgt);
      map_rew_wgts["ThinTargetnucleonA"].push_back(vec_rws[ii]->nuA_wgt);
      map_rew_wgts["Other"].push_back(vec_rws[ii]->other_wgt);

      //now usedin hp weights:
      map_rew_wgts["NA49"].push_back(vec_rws[ii]->na49_wgt);
      map_rew_wgts["MIPPThinTarget"].push_back(vec_rws[ii]->mipp_thin_wgt);
      //      map_rew_wgts["TheoryThinTarget"].push_back(vec_rws[ii]->theory_wgt);
      

      //Special Leo wgts for study:
      map_rew_wgts["Aux_pcpi_na49"].push_back(vec_rws[ii]->aux_pcpi_na49);
      map_rew_wgts["Aux_pcpi_bart"].push_back(vec_rws[ii]->aux_pcpi_bart);
      map_rew_wgts["Aux_pck_na49"].push_back(vec_rws[ii]->aux_pck_na49);
      map_rew_wgts["Aux_pck_mipp"].push_back(vec_rws[ii]->aux_pck_mipp);
      map_rew_wgts["Aux_pck_k0"].push_back(vec_rws[ii]->aux_pck_k0);
      map_rew_wgts["Aux_pcnu_neu"].push_back(vec_rws[ii]->aux_pcnu_neu);
      map_rew_wgts["Aux_pcnu_prt"].push_back(vec_rws[ii]->aux_pcnu_prt);
    }
    
    //cv calculation:
    cv_wgt = cv_rw->calculateWeight(*icd);
    
  }
  ////
  std::vector<double> MakeReweight::GetWeights(std::string nameReweighter){

    std::vector<double> tmp_vec;
    std::map<std::string,std::vector<double> >::iterator it = map_rew_wgts.begin();
    it = map_rew_wgts.find(nameReweighter);
    if(it!=map_rew_wgts.end())tmp_vec = it->second;
    
    return tmp_vec;
   
  }
  ////
  double MakeReweight::GetCVWeight(){   
    return cv_wgt;
  }
  ///
  int  MakeReweight::GetNumberOfUniversesUsed(){
    return Nuniverses;
  }
  MakeReweight::~MakeReweight(){
    
  }
  ///
  MakeReweight* MakeReweight::getInstance(){
    if (instance == 0) instance = new MakeReweight;
    return instance;
  }
}
