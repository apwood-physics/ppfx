

#include "CentralValuesAndUncertainties.h"
#include <set>
#include <string>
#include <iostream>
#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "TRandom3.h"

//using namespace std;

namespace NeutrinoFluxReweight{ 
  
  CentralValuesAndUncertainties* CentralValuesAndUncertainties::instance = 0;

  CentralValuesAndUncertainties::CentralValuesAndUncertainties(){
    //! Setting the seed to zero. 
    r3=new TRandom3(0);
    baseSeed = 0;
  }
  
  void CentralValuesAndUncertainties::readFromXML(const char* filename){
    using boost::property_tree::ptree;
    ptree top;
    
    read_xml(filename,top,2); // option 2 removes comment strings
    
    //! Reading the uncorrelated parameters:
    ptree& uncorrelated = top.get_child("pars.uncorrelated");
    ptree::iterator it = uncorrelated.begin();
    for(; it!=uncorrelated.end(); ++it){
      // it->first is the name
      // it->second is the child property tree
      double cv=it->second.get<double>("cv");
      double err=it->second.get<double>("err");
      Parameter p(it->first,cv);
      CentralValuesAndUncertainties::addUncorrelated(p,err);
    }

    //! Reading the uncorrelated list:
    ptree& uncorrelated_list = top.get_child("pars.uncorrelated_list");
    it = uncorrelated_list.begin();
    for(; it!=uncorrelated_list.end(); ++it){
      // it->first is the name
      // it->second is the child property tree

      std::string cvs_string=it->second.get<std::string>("cvs");
      std::string errs_string=it->second.get<std::string>("errs");
      double cv=0;
      int ii=0;
      std::string name;
      std::stringstream ss(cvs_string);
      std::vector<Parameter> tmp_par;
      while(ss >> cv){
	std::stringstream sID;
	sID << ii;
	std::string nameID = sID.str();
	name = it->first + "_" + nameID;
	Parameter p(name,cv);
	tmp_par.push_back(p);
	ii++;
      };
      
      double err=0;
      std::stringstream sserr(errs_string);
      ii=0;
      while(sserr >> err){
	CentralValuesAndUncertainties::addUncorrelated(tmp_par[ii],err);
	ii++;
      }     
      
    }
    
    //! Reading the correlated parameters:
    ptree& correlated = top.get_child("pars.correlated");
    it = correlated.begin();
    for(; it!=correlated.end(); ++it){
      // it->first is the name
      // it->second is the child property tree
      std::string cvs_string=it->second.get<std::string>("cvs");
      std::string covmx_string=it->second.get<std::string>("covmx");

      //filling the Parameter table:
      std::stringstream ss(cvs_string);
      double cv=0;
      int ii=0;
      std::string name;
      ParameterTable ptable;
      while(ss >> cv){
	std::stringstream sID;
	sID << ii;
	std::string nameID = sID.str();
	name = it->first + "_" +nameID;
	Parameter p(name,cv);
	ptable.setParameter(p);
	ii++;
      };
      TMatrixD mcov(ii,ii);
      //filling the matrix:
      std::stringstream ssmx(covmx_string);
      double err;
      int idx = 0;
      while(ssmx >> err){
	mcov(idx/ii,idx%ii) = err;
	idx++;
      }
      CentralValuesAndUncertainties::addCorrelated(ptable,mcov);
    }

    //Auxiliar parameter.
    //cv=1.0 and error 100%:
    Parameter par_aux("aux_parameter",1.0);
    CentralValuesAndUncertainties::addUncorrelated(par_aux,1.0);

  }
  
  void CentralValuesAndUncertainties::addUncorrelated(Parameter& cv_par, double uncertainty){
    uncorrelated_pars.setParameter(cv_par);
    uncorrelated_errors[cv_par.first] = uncertainty;
  }
  
  //  void CentralValuesAndUncertainties::addCorrelated(ParameterTable& cv_pars, MatrixClass& cov_mx){
  void CentralValuesAndUncertainties::addCorrelated(ParameterTable& cv_pars, TMatrixD& cov_mx){
    correlated_par_tables.push_back(cv_pars);
    covariance_matrices.push_back(cov_mx);
  } 
  
  void CentralValuesAndUncertainties::setBaseSeed(int val){
    baseSeed = val;
  }
  ParameterTable CentralValuesAndUncertainties::calculateParsForUniverse(int universe){
    
    //If universe = -1, then it is the central value:
    double cvfactor = 1.0;
    if(universe==-1)cvfactor = 0.0;
    int univ_seed = baseSeed + universe;
    r3->SetSeed(univ_seed);    
    
    ParameterTable ptable;
    
    //! We are going to use 100% correlated bin-to-bin for systematic errors in thin target data:
    double sigma_pc_pip = r3->Gaus(0.0,1.0);
    double sigma_pc_pim = r3->Gaus(0.0,1.0);
    double sigma_pc_kap = r3->Gaus(0.0,1.0);
    double sigma_pc_kam = r3->Gaus(0.0,1.0);
    double sigma_pc_p   = r3->Gaus(0.0,1.0);
    double sigma_pc_n   = r3->Gaus(0.0,1.0);

    const boost::interprocess::flat_map<std::string, double>& table_uncorr_pars = uncorrelated_pars.getMap();
    boost::interprocess::flat_map<std::string, double>::const_iterator it = table_uncorr_pars.begin();
    for(;it!=table_uncorr_pars.end();++it){
      double sigma = r3->Gaus(0.0,1.0);
      //redefining sigma for 100% correlation:
      if((it->first).find("ThinTarget_pC_pip_sys")<10)sigma = sigma_pc_pip;
      if((it->first).find("ThinTarget_pC_pim_sys")<10)sigma = sigma_pc_pim;
      if((it->first).find("ThinTargetLowxF_pC_kap_sys")<10)sigma = sigma_pc_kap;
      if((it->first).find("ThinTargetLowxF_pC_kam_sys")<10)sigma = sigma_pc_kam;
      if((it->first).find("ThinTarget_pC_p_sys")<10)  sigma = sigma_pc_p;
      if((it->first).find("ThinTarget_pC_n_sys")<10)  sigma = sigma_pc_n;
      double new_val = it->second  + cvfactor*sigma*uncorrelated_errors[it->first];
      Parameter p(it->first,new_val);
      ptable.setParameter(p);      
    }
    
    TDecompChol *decomp;
    
    for(size_t ii=0;ii<covariance_matrices.size();++ii){
      
      decomp=new TDecompChol(covariance_matrices[ii],0.0);
      
      bool isPosDef=decomp->Decompose();
      TMatrixD MxU = decomp->GetU();
      delete decomp;
      TMatrixD MxV(MxU); 
      MxV.Transpose(MxU);
      int nmat = MxV.GetNcols();
      TVectorD vsigma(nmat);
      for(int jj=0;jj<nmat;jj++){
	vsigma[jj]=cvfactor*(r3->Gaus(0.0,1.0));
      }   
      TVectorD vecDShift = MxV*vsigma;
      
      const boost::interprocess::flat_map<std::string, double>& tb = (correlated_par_tables[ii]).getMap();
      boost::interprocess::flat_map<std::string, double>::const_iterator it_tb = tb.begin();

      for(;it_tb != tb.end();++it_tb){
	std::string tmp_name = it_tb->first;
	std::string snID = tmp_name.substr((it_tb->first).rfind("_")+1,(it_tb->first).length());
	std::stringstream ssID(snID);
	int nID;
	ssID >> nID;
	double new_val = it_tb->second + vecDShift[nID];	
	Parameter p(it_tb->first,new_val); 
	if(isPosDef)ptable.setParameter(p);
      }
      
    }
    
    return ptable; 
    
  }  
  
  ParameterTable CentralValuesAndUncertainties::getCVPars(){      

    ParameterTable ptableCV;
    
    const boost::interprocess::flat_map<std::string, double>& table_pars=uncorrelated_pars.getMap();
    boost::interprocess::flat_map<std::string, double>::const_iterator it = table_pars.begin();
    for(;it!=table_pars.end();it++){
      Parameter p(it->first,it->second); 
      ptableCV.setParameter(p);      
    }
    
    //Correlated:
    for(size_t ii=0;ii<covariance_matrices.size();ii++){
      const boost::interprocess::flat_map<std::string, double>& corr_table_pars = correlated_par_tables[ii].getMap();
      it = corr_table_pars.begin();
      for(;it != corr_table_pars.end();it++){
	Parameter p(it->first,it->second); 
	ptableCV.setParameter(p);
      }
      
    }
    
    return ptableCV;
    
  }
  
  CentralValuesAndUncertainties* CentralValuesAndUncertainties::getInstance(){
    if (instance == 0) instance = new CentralValuesAndUncertainties;
    return instance;
  }

}

