#ifndef __AnaEvent_hh__
#define __AnaEvent_hh__

#include <iostream>
#include <vector>

class AnaEvent
{
    public:
        AnaEvent(long int evid)
        {
            m_evid     = evid;
            m_sample   = -99;
            m_bin      = -99;
            m_pmtid = -99;
            m_mpmtid = -99;
            m_cosths = -999.0;
            m_phis = -999.0;
            m_costh = -999.0;
            m_costhm = -999.0;
            m_phim = -999.0;
            m_omega = -999.0;
            m_dz = 0;
            m_z0 = 0;
            m_R   = -999.0;
            m_nPE   = -999.0;
            //m_nPE_tail = 0.0;
            m_nPE_indirect = 0.0;
            m_nPE_indirect_err = 0.0;
            m_timetof   = -999.0;
            m_eff = 1.0;
            m_wght     = 1.0;
            m_wghtMC   = 1.0;
        }

        //Set/Get methods
        inline void SetSampleType(const short val){ m_sample = val; }
        inline short GetSampleType() const { return m_sample; }

        inline void SetSampleBin(const int val){ m_bin = val; }
        inline int GetSampleBin() const { return m_bin; }

        inline void SetPMTID(const int val){ m_pmtid = val; }
        inline int GetPMTID() const { return m_pmtid; }

        inline void SetmPMTID(const int val){ m_mpmtid = val; }
        inline int GetmPMTID() const { return m_mpmtid; }

        inline long int GetEvId() const { return m_evid; }

        inline void SetCosths(double val) {m_cosths = val;}
        inline double GetCosths() const { return m_cosths; }

        inline void SetPhis(double val) {m_phis = val;}
        inline double GetPhis() const { return m_phis; }

        inline void SetCosth(double val){ m_costh = val; }
        inline double GetCosth() const { return m_costh; }

        inline void SetCosthm(double val){ m_costhm = val; }
        inline double GetCosthm() const { return m_costhm; }

        inline void SetPhim(double val){ m_phim = val; }
        inline double GetPhim() const { return m_phim; }

        inline void SetOmega(double val){ m_omega = val; }
        inline double GetOmega() const { return m_omega; }

        inline void SetDz(double val){ m_dz = val; }
        inline double GetDz() const { return m_dz; }

        inline void SetZ0(double val){ m_z0 = val; }
        inline double GetZ0() const { return m_z0; }

        inline void SetR(double val){ m_R = val; }
        inline double GetR() const { return m_R; }

        inline void SetPE(double val){ m_nPE = val; }
        inline double GetPE() const { return m_nPE; }

        //inline void SetTailPE(double val){ m_nPE_tail = val; }
        //inline double GetTailPE() const { return m_nPE_tail; }

        inline void SetPEIndirect(double val){ m_nPE_indirect = val; }
        inline double GetPEIndirect() const { return m_nPE_indirect; }

        inline void SetPEIndirectErr(double val){ m_nPE_indirect_err = val; }
        inline double GetPEIndirectErr() const { return m_nPE_indirect_err; }

        inline void SetTimetof(double val){ m_timetof = val; }
        inline double GetTimetof() const { return m_timetof; }

        inline void SetEff(double val){ m_eff = val; }
        inline double GetEff() const { return m_eff; }

        inline void SetTimetofNom(std::vector<double> val){ m_timetof_nom = val; }
        inline std::vector<double> GetTimetofNom() const { return m_timetof_nom; }

        inline void SetTimetofNomSig2(std::vector<double> val){ m_timetof_nom_sig2 = val; }
        inline std::vector<double> GetTimetofNomSig2() const { return m_timetof_nom_sig2; }

        inline void SetTimetofPred(std::vector<double> val){ m_timetof_pred = val; }
        inline std::vector<double> GetTimetofPred() const { return m_timetof_pred; }

        inline void SetEvWght(double val){ m_wght  = val; }
        inline double GetEvWght() const { return m_wght; }
        inline void AddEvWght(double val){ m_wght *= val; }
        inline void SetEvWghtMC(double val){ m_wghtMC  = val; }
        inline double GetEvWghtMC() const { return m_wghtMC; }

        inline void ResetEvWght(){ m_wght = m_wghtMC; m_timetof_pred = m_timetof_nom; }

        void Print() const
        {
            std::cout << "Event ID    " << m_evid << std::endl
                      << "Sample      " << GetSampleType() << std::endl
                      << "Bin         " << GetSampleBin() << std::endl
                      << "PMT id      " << GetPMTID() << std::endl
                      << "cosths      " << GetCosths() << std::endl
                      << "phis        " << GetPhis() << std::endl
                      << "costh       " << GetCosth() << std::endl
                      << "costhm      " << GetCosthm() << std::endl
                      << "phim        " << GetPhim() << std::endl
                      << "omega       " << GetOmega() << std::endl
                      << "R           " << GetR() << std::endl
                      << "nPE         " << GetPE() << std::endl
                      << "timetof     " << GetTimetof() << std::endl
                      << "Weight      " << GetEvWght() << std::endl
                      << "Weight MC   " << GetEvWghtMC() << std::endl;
        }

        double GetEventVar(const std::string& var) const
        {
            if(var == "R")
                return m_R;
            else if(var == "costh")
                return m_costh;
            else if(var == "costhm")
                return m_costhm;
            else if(var == "cosths")
                return m_cosths;
            else if(var == "phis")
                return m_phis;
            else if(var == "phim")
                return m_phim;
            else if(var == "timetof")
                return m_timetof;
            else if(var == "nPE")
                return m_nPE;
            //else if(var == "nPE_tail")
            //    return m_nPE_tail;
            else if(var == "m_nPE_indirect")
                return m_nPE_indirect;
            else if(var == "m_nPE_indirect_err")
                return m_nPE_indirect_err;
            else if(var == "sample")
                return m_sample;
            else if(var == "omega")
                return m_omega;
            else if(var == "dz")
                return m_dz;
            else if(var == "z0")
                return m_z0;
            else if(var == "PMT_id")
                return m_pmtid;
            else if(var == "mPMT_id")
                return m_mpmtid;
            else if(var == "Eff")
                return m_eff;
            else
            {
                std::cout<<" Error! Variable "<<var<<" not available in AnaEvent"<<std::endl;
                return -1;
            }
        }


        inline const std::vector<double>& GetRecoVar() const { return reco_var; }
        inline void SetRecoVar(std::vector<double> vec) { reco_var = vec; }
        
        inline void ResetParList() { par_list.clear(); }
        inline void AddPar(int val) { par_list.push_back(val); }
        inline const std::vector<int>& GetParList() const { return par_list; }

    private:
        long int m_evid;   //unique event id
        short m_sample;    //sample type (aka cutBranch)
        int m_bin;       //reco bin for the sample binning
        int m_pmtid;       //pmt unique id
        int m_mpmtid;      //pmt unique id inside a mPMT
        double m_cosths;   //PMT angle relative to source
        double m_phis;     //PMT phi angle relative to source
        double m_costh;    //photon incident angle relative to PMT
        double m_costhm;   //photon incident theta angle relative to central PMT ( = costh for B&L PMT )
        double m_phim;     //photon incident phi angle relative to central PMT, only for mPMT
        double m_omega;    //solid angle subtended by PMT
        double m_dz;       // z-pos relative to source
        double m_z0;       // diffuser z-pos
        double m_R;        //distance to source
        double m_nPE;      //number of PE
        //double m_nPE_tail; //number of PE at the tail
        double m_nPE_indirect; //indirect PE prediction
        double m_nPE_indirect_err; //err^2
        double m_timetof;  //hittime-tof
        double m_eff;      // PMT relative efficiency
        double m_wght;     //event weight
        double m_wghtMC;   //event weight from original MC
        std::vector<double> m_timetof_nom;      // template nominal prediction
        std::vector<double> m_timetof_nom_sig2; // template nominal prediction sigma^2
        std::vector<double> m_timetof_pred;     // template prediction modified by parameter spline
        std::vector<int> par_list;
        std::vector<double> reco_var;

};

#endif
