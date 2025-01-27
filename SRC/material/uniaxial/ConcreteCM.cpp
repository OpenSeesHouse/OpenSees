// Code written/implemented by:	Kristijan Kolozvari (kkolozvari@fullerton.edu)
//								California State University, Fullerton 
//								Kutay Orakcal
//								Bogazici University, Istanbul, Turkey
//								John Wallace
//								University of California, Los Angeles
//
// Created: 07/2015
//
// Description: This file contains the class implementation for 
// uniaxialMaterial ConcreteCM, which is a uniaxial hysteretic 
// constitutive model for concrete developed by Chang and Mander(1994).
// This model is a refined, rule - based, generalized, and non - dimensional 
// constitutive model that allows calibration of the monotonic and hysteretic 
// material modeling parameters, and can simulate the hysteretic behavior of 
// confined and unconfined, ordinary and high - strength concrete, in both 
// cyclic compression and tension. The model addresses important behavioral 
// features, such as continuous hysteretic behavior under cyclic compression 
// and tension, progressive stiffness degradation associated with smooth 
// unloading and reloading curves at increasing strain values, and gradual 
// crack closure effects. 
//
// References:
// 1) Chang, G.A. and Mander, J.B. (1994), “Seismic Energy Based Fatigue Damage 
// Analysis of Bridge Columns: Part I – Evaluation of Seismic Capacity”, NCEER 
// Technical Report No. NCEER-94-0006, State University of New York, Buffalo.
// 2) Kutay Orakcal (2004), "Nonlinear Modeling and Analysis of Slender Reinforced 
// Concrete Walls", PhD Dissertation, Department of Civil and Environmental Engineering, 
// University of California, Los Angeles.
//
// Source: /usr/local/cvs/OpenSees/SRC/material/uniaxial/ConcreteCM.cpp
//
// Rev: 1


#include <ConcreteCM.h>
#include <Vector.h>
#include <Matrix.h>
#include <Channel.h>
#include <Information.h>
#include <Parameter.h>
#include <string.h>

#include <MaterialResponse.h>
#include <math.h>
#include <float.h>

#include <elementAPI.h>

#ifndef fmin
#define fmin(a,b) ( ((a)<(b))?(a):(b) )
#endif

#ifndef fmax
#define fmax(a,b) ( ((a)>(b))?(a):(b) )
#endif
// Read input parameters and build the material
void *OPS_ConcreteCM(void)
{

  // Pointer to a uniaxial material that will be returned                       
  UniaxialMaterial *theMaterial = 0;
  
  int numArgs = OPS_GetNumRemainingInputArgs();

  // Parse the script for material parameters
  if (numArgs != 10 && numArgs !=  11 && numArgs != 12) {
    opserr << "Incorrect # args Want: uniaxialMaterial ConcreteCM tag? fpcc? epcc? Ec? rc? xcrn? ft? et? rt? xcrp? <-GapClose gap?>" << endln;
    return 0;
  }
  
  int iData[1];
  double dData[9];
  
  int numData = 1;
  if (OPS_GetIntInput(numData, iData) != 0) {
    opserr << "WARNING invalid tag for uniaxialMaterial ConcreteCM ConcreteCM" << endln;
    return 0;
  }
  
  numData = 9;
  if (OPS_GetDoubleInput(numData, dData) != 0) {
    opserr << "Invalid data for uniaxialMaterial ConcreteCM ConcreteCM " << iData[0] << endln;
    return 0;
  }
  
  if (numArgs == 10) {
    
    theMaterial = new ConcreteCM(iData[0], dData[0], dData[1], dData[2], dData[3], dData[4], dData[5], dData[6], dData[7], dData[8]);
    
  } else if (numArgs == 11) {
    
    numData = 1;
    int mon;
    if (OPS_GetIntInput(numData, &mon) != 0) {
      opserr << "Invalid $mon parameter for uniaxialMaterial ConcreteCM with tag  " << iData[0] << endln;
      return 0;
    }
    
    if (mon != 0 && mon != 1) {
      opserr << "Invalid $mon parameter for uniaxialMaterial ConcreteCM with tag  " << iData[0] << endln;
      return 0;
    }
    
    theMaterial = new ConcreteCM(iData[0], dData[0], dData[1], dData[2], dData[3], dData[4], dData[5], dData[6], dData[7], dData[8], mon);
    
  } else {

    int gap;
    numData = 1;
    
    const char *str = OPS_GetString();
    // OPS_GetStringCopy(&str);
    if (strcmp(str, "-GapClose") == 0) {
      if (OPS_GetIntInput(numData, &gap) != 0) {
		opserr << "Invalid $gap parameter for uniaxialMaterial ConcreteCM with tag  " << iData[0] << endln;
		return 0;
      }
    } else {
      opserr << "Invalid input parameter for uniaxialMaterial ConcreteCM with tag  " << iData[0] << ", want: -GapClose"<< endln;
      return 0;
    }
    
    // delete [] str;
    
    if (gap != 0 && gap != 1) {
      opserr << "Invalid $gap parameter for uniaxialMaterial ConcreteCM with tag  " << iData[0] << endln;
      return 0;
    }
    
    int check = 0; // dummy variable
    theMaterial = new ConcreteCM(iData[0], dData[0], dData[1], dData[2], dData[3], dData[4], dData[5], dData[6], dData[7], dData[8], gap, check);
  }
  
  return theMaterial;
}

// Typical Constructor: mon=0 (default), Gap=0 (default)
ConcreteCM::ConcreteCM
(int tag, double FPCC, double EPCC, double EC, double RC, double XCRN, double FT, double ET, double RT, double XCRP)
:UniaxialMaterial(tag, MAT_TAG_ConcreteCM),
fpcc(FPCC), epcc(EPCC), Ec(EC), rc(RC), xcrn(XCRN), ft(FT), et(ET), rt(RT), xcrp(XCRP), mon(0), Gap(0),// input
Ceunn(0.0), Cfunn(0.0), Ceunp(0.0), Cfunp(0.0), Cer(0.0), Cfr(0.0), Cer0n(0.0), // history and state variables
Cfr0n(0.0), Cer0p(0.0), Cfr0p(0.0), Ce0(0.0), Cea(0.0), Ceb(0.0), Ced(0.0), Cinc(0.0), 
Crule(0.0), Cstrain(0.0), Cstress(0.0), Ctangent(0.0) 

{

	// Set trial values
	this->revertToLastCommit();

	// AddingSensitivity:BEGIN /////////////////////////////////////
	parameterID = 0;
	SHVs = 0;
	// AddingSensitivity:END //////////////////////////////////////

}

// Constructor for monotonic stress-strain relationship only:  mon=1 (invoked in FSAM only), Gap=0 (no impact since monotonic)
ConcreteCM::ConcreteCM
(int tag, double FPCC, double EPCC, double EC, double RC, double XCRN, double FT, double ET, double RT, double XCRP, int MON)
:UniaxialMaterial(tag, MAT_TAG_ConcreteCM),
fpcc(FPCC), epcc(EPCC), Ec(EC), rc(RC), xcrn(XCRN), ft(FT), et(ET), rt(RT), xcrp(XCRP), mon(MON), Gap(0),// input
Ceunn(0.0), Cfunn(0.0), Ceunp(0.0), Cfunp(0.0), Cer(0.0), Cfr(0.0), Cer0n(0.0), // history and state variables
Cfr0n(0.0), Cer0p(0.0), Cfr0p(0.0), Ce0(0.0), Cea(0.0), Ceb(0.0), Ced(0.0), Cinc(0.0), 
Crule(0.0), Cstrain(0.0), Cstress(0.0), Ctangent(0.0) 

{

	// Set trial values
	this->revertToLastCommit();

	// AddingSensitivity:BEGIN /////////////////////////////////////
	parameterID = 0;
	SHVs = 0;
	// AddingSensitivity:END ///////////////////////////////////////

}

// Constructor for optional gradual gap closure: mon=0 (default), Gap=0 or 1 (optional user-generated input, see Eplpf member function)
ConcreteCM::ConcreteCM
(int tag, double FPCC, double EPCC, double EC, double RC, double XCRN, double FT, double ET, double RT, double XCRP, int GAP, int DUMMY)
:UniaxialMaterial(tag, MAT_TAG_ConcreteCM),
fpcc(FPCC), epcc(EPCC), Ec(EC), rc(RC), xcrn(XCRN), ft(FT), et(ET), rt(RT), xcrp(XCRP), mon(0), Gap(GAP),// input
Ceunn(0.0), Cfunn(0.0), Ceunp(0.0), Cfunp(0.0), Cer(0.0), Cfr(0.0), Cer0n(0.0), // history and state variables
Cfr0n(0.0), Cer0p(0.0), Cfr0p(0.0), Ce0(0.0), Cea(0.0), Ceb(0.0), Ced(0.0), Cinc(0.0),
Crule(0.0), Cstrain(0.0), Cstress(0.0), Ctangent(0.0)

{

	// Set trial values
	this->revertToLastCommit();

	// AddingSensitivity:BEGIN /////////////////////////////////////
	parameterID = 0;
	SHVs = 0;
	// AddingSensitivity:END //////////////////////////////////////

}

ConcreteCM::ConcreteCM():UniaxialMaterial(0, MAT_TAG_ConcreteCM),
fpcc(0.0), epcc(0.0), Ec(0.0), rc(0.0), xcrn(0.0), ft(0.0), et(0.0), rt(0.0), xcrp(0.0), mon(0),Gap(0),// input
Ceunn(0.0), Cfunn(0.0), Ceunp(0.0), Cfunp(0.0), Cer(0.0), Cfr(0.0), Cer0n(0.0), // history and state variables
Cfr0n(0.0), Cer0p(0.0), Cfr0p(0.0), Ce0(0.0), Cea(0.0), Ceb(0.0), Ced(0.0), Cinc(0.0), 
Crule(0.0), Cstrain(0.0), Cstress(0.0), Ctangent(0.0) 

{
	// Set trial values
	this->revertToLastCommit();

	// AddingSensitivity:BEGIN /////////////////////////////////////
	parameterID = 0;
	SHVs = 0;
	// AddingSensitivity:END //////////////////////////////////////
}

ConcreteCM::ConcreteCM(int tag)
	:UniaxialMaterial(tag, MAT_TAG_ConcreteCM)
{
	// Set trial values
	this->revertToLastCommit();

	// AddingSensitivity:BEGIN /////////////////////////////////////
	parameterID = 0;
	SHVs = 0;
	// AddingSensitivity:END //////////////////////////////////////
}

ConcreteCM::~ConcreteCM ()
{
	// Does nothing
}

int ConcreteCM::setTrialStrain (double strain, double strainRate)

{

	// Set trial strain
	this->revertToLastCommit();
	Tstrain = strain;

	if (mon == 1) { // switch to monotonic material ONLY (invoked in FSAM)

		if (Tstrain < 0.0)   {	        // negative envelope  
			fcEtnf(Tstrain);
			Tinc=-1.0;
		}	  
		else if (Tstrain > 0.0)	{  
			fcEtpf(Tstrain,Ce0);
			Tinc=1.0;
		}
		else	{
			Tstress=0.0;
			Ttangent=Ec;
			Trule=0.0;
			Tinc=0.0;
		}

		Teunn=0.0;	
		Tfunn=0.0;
		Teunp=0.0;
		Tfunp=0.0;
		Ter=0.0;
		Tfr=0.0;
		Ter0n=0.0;
		Tfr0n=0.0;
		Ter0p=0.0;
		Tfr0p=0.0;
		Te0=0.0;
		Tea=0.0;
		Teb=0.0;
		Ted=0.0;

	}

	else if (Cinc==0.0)	{		// monotonic, first data point
		if (Tstrain < 0.0)   {	// negative envelope,	  
			fcEtnf(Tstrain);
			Tinc=-1.0;
		}	  
		else if (Tstrain > 0.0)	{  
			fcEtpf(Tstrain,Ce0);
			Tinc=1.0;
		}
		else	{
			Tstress=0.0;
			Ttangent=Ec;
			Trule=0.0;
			Tinc=0.0;
		}

		Teunn=0.0;	
		Tfunn=0.0;
		Teunp=0.0;
		Tfunp=0.0;
		Ter=0.0;
		Tfr=0.0;
		Ter0n=0.0;
		Tfr0n=0.0;
		Ter0p=0.0;
		Tfr0p=0.0;
		Te0=0.0;
		Tea=0.0;
		Teb=0.0;
		Ted=0.0;
	}

	else	{	//cyclic

		if (Tstrain > Cstrain)	{
			Tinc=1.0;
		}
		else if (Tstrain < Cstrain)	{
			Tinc=-1.0;
		}
		else	{
			Tinc=Cinc;
		}

		Teunn=Ceunn;
		Tfunn=Cfunn;
		Teunp=Ceunp;
		Tfunp=Cfunp;
		Ter=Cer;
		Tfr=Cfr;
		Ter0n=Cer0n;
		Tfr0n=Cfr0n;
		Ter0p=Cer0p;
		Tfr0p=Cfr0p;
		Te0=Ce0;
		Tea=Cea;
		Teb=Ceb;
		Ted=Ced;
		Trule=Crule;

		esplnf(Teunn,Tfunn);
		Eplnf(Teunn);
		Esecnf(Teunn,Tfunn);
		delenf(Teunn);
		delfnf(Teunn,Tfunn);
		fnewnf(Teunn,Tfunn);
		Enewnf(Teunn,Tfunn);
		esrenf(Teunn);
		freErenf(Teunn);
		fnewstnf(Tfunn,delfn,Teunn,Ter0n,espln);
		Enewstnf(fnewstn,Tfr0n,Teunn,Ter0n);
		esrestnf(Teunn,delen,Ter0n,espln);   
		freErestnf(Teunn,Tfunn,Ter0n);

		esplpf(Teunp,Tfunp,Te0,espln);
		Eplpf(Te0,Teunp);
		Esecpf(Te0,Teunp,Tfunp,espln);  
		delepf(Teunp,Te0);
		delfpf(Tfunp,Teunp,Te0); 
		fnewpf(Tfunp,Teunp,Te0);
		Enewpf(Teunp,Tfunp,Te0,espln);
		esrepf(Teunp,Te0);
		freErepf(Teunp,Te0);
		fnewstpf(Tfunp,delfp,Teunp,Ter0p,esplp,Te0);
		Enewstpf(fnewstp,Tfr0p,Teunp,Ter0p);
		esrestpf(Teunp,delep,Ter0p,esplp);
		freErestpf(Teunp,Tfunp,Ter0p,Te0,espln);

		// [1]
		if (Cinc==-1.0)	{

			if (Tstrain > Cstrain)	{	// Start reversal from negative direction to positive direction		

				if (Crule==1.0 || Crule==5.0 || Crule==7.0)	{	// Rules [3,9,8,2,6]

					Teunn=Cstrain;
					Tfunn=Cstress;

					e0eunpfunpf(Te0,Teunp,Tfunp,Teunn,Tfunn);

					esplnf(Teunn,Tfunn);
					Eplnf(Teunn);				
					fnewpf(Tfunp,Teunp,Te0);
					Enewpf(Teunp,Tfunp,Te0,espln);

					esrepf(Teunp,Te0);
					freErepf(Teunp,Te0);

					if (Tstrain<=espln)	{				// Rule 3
						r3f(Teunn,Tfunn,espln,Epln);
						Trule=3.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}

					else if (Tstrain<=Teunp)	{		// Rule 9
						r9f(espln,Epln,Teunp,fnewp,Enewp);
						Trule=9.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;

					}
					else if (Tstrain<=esrep)	{		// Rule 8

						r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
						Trule=8.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}

					else	{							// Rules 2 and 6
						fcEtpf(Tstrain,Te0);
					}

				}	// if (Crule=1.0) or 5 or 7

				else if (Crule==10.0)	{

					Ter=Cstrain;
					Tfr=Cstress;

					Teb=Ter;
					fb=Tfr;

					ea1112f(Teb,espln,esplp,Teunn,Teunp);

					if (Tea<=espln)	{

						if (Tstrain<=Tea)	{				// Rule 12 targeting for ea on 3

							r3f(Teunn, Tfunn, espln, Epln);

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

							fca=fc;
							Eta=Et;

							esi=Ter;
							fi=Tfr;
							Ei=Ec;
							esf=Tea;

							RAf(esi,fi,Ei,esf,fca,Eta);

							r12f(Ter,Tfr,Tea,fca,Eta,A,R);
							Trule=12.0;

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}

						else if (Tstrain<=espln)	{	// Rule 3

							r3f(Teunn,Tfunn,espln,Epln);
							Trule=3.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else if (Tstrain<=Teunp)	{	// Rule 9
							r9f(espln,Epln,Teunp,fnewp,Enewp);
							Trule=9.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}

						else if (Tstrain<=esrep)	{	// Rule 8						  

							r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
							Trule=8.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else	{						// Rules 2 and 6
							fcEtpf(Tstrain,Te0);
						}
					}

					else if (Tea<=Teunp)	{			// and Tea>espln

						if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 9

							r9f(espln,Epln,Teunp,fnewp,Enewp); 

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

							fca=fc;
							Eta=Et;

							esi=Ter;
							fi=Tfr;
							Ei=Ec;
							esf=Tea;

							RAf(esi,fi,Ei,esf,fca,Eta);

							r12f(Ter,Tfr,Tea,fca,Eta,A,R);
							Trule=12.0;

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;

						}
						else if (Tstrain<=Teunp)	{	// Rule 9

							r9f(espln,Epln,Teunp,fnewp,Enewp);
							Trule=9.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;

						}
						else if (Tstrain<=esrep)	{	// Rule 8						  

							r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
							Trule=8.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else	{						// Rules 2 and 6
							fcEtpf(Tstrain,Te0);
						}
					}

					else if (Tea<=esrep)	{	

						if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 8

							r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

							fca=fc;
							Eta=Et;

							esi=Ter;
							fi=Tfr;
							Ei=Ec;
							esf=Tea;

							RAf(esi,fi,Ei,esf,fca,Eta);

							r12f(Ter,Tfr,Tea,fca,Eta,A,R);
							Trule=12.0;

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}

						else if (Tstrain<=esrep)	{	// Rule 8						  

							r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
							Trule=8.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else	{						// Rules 2 and 6
							fcEtpf(Tstrain,Te0);
						}
					}

					else	{		// (Tea>esrep)

						if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 2 or 6

							fcEtpf(Tea,Te0);

							fca=Tstress;
							Eta=Ttangent;

							esi=Ter;
							fi=Tfr;
							Ei=Ec;
							esf=Tea;

							RAf(esi,fi,Ei,esf,fca,Eta);

							r12f(Ter,Tfr,Tea,fca,Eta,A,R);
							Trule=12.0;

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;					  
						}
						else	{						// Rules 2 and 6
							fcEtpf(Tstrain,Te0);
						}
					}
				} // if (Crule=10.0)

				else if (Crule==11.0)	{

					Ter=Cstrain;
					Tfr=Cstress;

					if (Teb!=Ter0p)	{

						if (Tea<=espln)	{

							if (Tstrain<=Tea)	{	// Rule 12 targeting for ea on 3							  

								r3f(Teunn,Tfunn,espln,Epln);   

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

								fca=fc;
								Eta=Et;
								esi=Ter;
								fi=Tfr;

								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}

							else if (Tstrain<=espln)	{	// Rule 3

								r3f(Teunn,Tfunn,espln,Epln);
								Trule=3.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain<=Teunp)	{	// Rule 9

								r9f(espln,Epln,Teunp,fnewp,Enewp);
								Trule=9.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;		  
							}
							else if (Tstrain<=esrep)	{	// Rule 8						  

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
								Trule=8.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}		  
							else	{						// Rules 2 and 6						  
								fcEtpf(Tstrain,Te0);					  
							}
						}

						else if (Tea<=Teunp)	{			// and Tea>espln

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 9						  

								r9f(espln,Epln,Teunp,fnewp,Enewp); 

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

								fca=fc;
								Eta=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;			  
							}
							else if (Tstrain<=Teunp)	{	// Rule 9						  

								r9f(espln,Epln,Teunp,fnewp,Enewp);
								Trule=9.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;

							}

							else if (Tstrain<=esrep)	{	// Rule 8						  

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
								Trule=8.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 2 and 6			  
								fcEtpf(Tstrain,Te0);		  
							}
						}

						else if (Tea<=esrep)	{	

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 8

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

								fca=fc;
								Eta=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}					  
							else if (Tstrain<=esrep)	{	// Rule 8						  

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
								Trule=8.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}					  
							else	{						// Rules 2 and 6						  
								fcEtpf(Tstrain,Te0);					  
							}				  
						}

						else	{		// (Tea>esrep)

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 2 or 6

								fcEtpf(Tea,Te0);

								fca=Tstress;
								Eta=Ttangent;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}					  
							else	{						// Rules 2 and 6
								fcEtpf(Tstrain,Te0);					  
							}
						}
					}

					else	{	// if (Teb==Ter0p)

						if (Tea<=esrestp)	{

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 88

								r88f(Tea,Te0,Ter0p,Tfr0p,Teunp,fnewstp,Enewstp,esrestp,frestp,Erestp);

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain<esrestp)	{	// Rule 88

								r88f(Tstrain,Te0,Ter0p,Tfr0p,Teunp,fnewstp,Enewstp,esrestp,frestp,Erestp);
								Trule=88.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 2 and 6						  
								fcEtpf(Tstrain,Te0);					  
							}
						}

						else	{	// if (Tea>esrestp)

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 2 or 6

								fcEtpf(Tea,Te0);

								fca=Tstress;
								Eta=Ttangent;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}					  
							else	{						// Rules 2 and 6
								fcEtpf(Tstrain,Te0);					  
							}
						}
					}
				} // if (Crule==11.0)

				else if (Crule==13.0 || Crule==15.0)	{

					Ter=Cstrain;
					Tfr=Cstress;

					if (Crule==13.0)	{
						Tea=Ter;
						fa=Tfr;
						eb1415f(Tea,fa,Esecn);
					}

					else if (Crule==15.0)	{
						Tea=Cea;
						Teb=Ceb;
					}

					if (Tstrain<=Teb)	{					// Rule 14

						r14f(Ter,Tfr,Teb);
						Trule=14.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}
					else if (Tstrain<Teunp)	{			// Rule 66

						r66f(Tstrain,Te0);
						Trule=66.0;

					}
					else	{							// Rule 6
						fcEtpr6f(Tstrain,Te0);
						Trule=6.0; 
					}
				} // if (Crule==13.0) or 15.0

				else if (Crule==4.0)	{

					Ter0p=Cstrain;
					Tfr0p=Cstress;

					Teb=Ter0p;

					fnewstpf(Tfunp,delfp,Teunp,Ter0p,esplp,Te0);
					Enewstpf(fnewstp,Tfr0p,Teunp,Ter0p);

					esrestpf(Teunp,delep,Ter0p,esplp);
					freErestpf(Teunp,Tfunp,Ter0p,Te0,espln);

					if (Tstrain<esrestp)	{				// Rule 88

						r88f(Tstrain,Te0,Ter0p,Tfr0p,Teunp,fnewstp,Enewstp,esrestp,frestp,Erestp);
						Trule=88.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;

					}
					else	{								// Rules 2 and 6

						fcEtpf(Tstrain,Te0);

					}

				} // if (Crule==4.0)

				else if (Crule==77.0)	{					// Reversal from transition 77 [Rules 12,(3,9 or 9),8,2,6 or Rules 3,9,8,2,6] 

					if (Cstrain>=Teunn)	{			

						Ter=Cstrain;
						Tfr=Cstress;

						Teb=Ter;
						Tea=Ter0n;

						if (Tea<=espln)	{				// Reversal from 77 by Rules [12,3,9,8,2,6]	

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 3

								r3f(Teunn,Tfunn,espln,Epln);   

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

								fca=fc;
								Eta=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;						  
							}					   
							else if (Tstrain<=espln)	{	// Rule 3

								r3f(Teunn,Tfunn,espln,Epln);
								Trule=3.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain<=Teunp)	{	// Rule 9

								r9f(espln,Epln,Teunp,fnewp,Enewp);
								Trule=9.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;

							}
							else if (Tstrain<=esrep)	{	// Rule 8						 

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
								Trule=8.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}		  
							else	{						// Rules 2 and 6						  
								fcEtpf(Tstrain,Te0);					  
							}
						}

						else if (Tea<=Teunp)	{			// Reversal from 77 by Rules [12,9,8,2,6]

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 9						  

								r9f(espln,Epln,Teunp,fnewp,Enewp); 

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

								fca=fc;
								Eta=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;			  
							}
							else if (Tstrain<=Teunp)	{	// Rule 9

								r9f(espln,Epln,Teunp,fnewp,Enewp);
								Trule=9.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain<=esrep)	{	// Rule 8						  

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
								Trule=8.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 2 and 6			  
								fcEtpf(Tstrain,Te0);		  
							}
						}

						else if (Tea<=esrep)	{			// Reversal from 77 by Rules [12,8,2,6]

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 8

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

								fca=fc;
								Eta=Et;
								esi=Ter;
								fi=Tfr;

								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}
							else if (Tstrain<=esrep)	{	// Rule 8

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
								Trule=8.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}					  
							else	{						// Rules 2 and 6						  
								fcEtpf(Tstrain,Te0);					  
							}				  
						}

						else	{		// (Tea>esrep)		// Reversal from 88 by Rules [12,2,6]

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 2 or 6

								fcEtpf(Tea,Te0);

								fca=Tstress;
								Eta=Ttangent;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}				
							else	{						// Rules 2 and 6
								fcEtpf(Tstrain,Te0);					  
							}
						}				  
					}	// if (Cstrain>=Teunn)

					else	{	// if (Cstrain<Teunn)		// Reversal from transition 77 by Rules [3,9,8,2,6] 

						Teunn=Cstrain;				  
						Tfunn=Cstress;

						e0eunpfunpf(Te0,Teunp,Tfunp,Teunn,Tfunn);

						esplnf(Teunn,Tfunn);
						Eplnf(Teunn);

						fnewpf(Tfunp,Teunp,Te0);
						Enewpf(Teunp,Tfunp,Te0,espln);

						esrepf(Teunp,Te0);
						freErepf(Teunp,Te0);

						if (Tstrain<=espln)	{				// Rule 3	  

							r3f(Teunn,Tfunn,espln,Epln);
							Trule=3.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}					  
						else if (Tstrain<=Teunp)	{		// Rule 9						  

							r9f(espln,Epln,Teunp,fnewp,Enewp);						  	  
							Trule=9.0;						  	  

							RAf(esi,fi,Ei,esf,ff,Ef);					  	  

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R); 	  

							Tstress=fc; 	  
							Ttangent=Et;		  	  
						}
						else if (Tstrain<=esrep)	{		// Rule 8						  	  

							r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);  
							Trule=8.0;	  

							RAf(esi,fi,Ei,esf,ff,Ef);	  

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R); 	  

							Tstress=fc;  	  
							Ttangent=Et;						  
						}						  
						else	{							// Rules 2 and 6						  		  
							fcEtpf(Tstrain,Te0);					  	  
						} 				  
					}	// if (Cstrain<Teunn)			  
				}	// if (Crule==77.0)
			} // if (Tstrain>Cstrain)

			//////////////////////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////

			else	{	//	if (Tstrain<=Cstrain)	// Continue going to negative direction

				if (Crule==4.0 || Crule ==10.0 || Crule==7.0 )	{	// or 10.0 or 7.0

					if (Tstrain>=esplp)	{

						r4f(Teunp,Tfunp,esplp,Eplp);
						Trule=4.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}
					else if (Tstrain>=Teunn)	{

						r10f(esplp,Eplp,Teunn,fnewn,Enewn);
						Trule=10.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						// Fix 2
						Esectest = (ff-fi)/(esf-esi);
						if (Et==Esectest) {
							if (Tstrain>=espln) {
								fc=0;
								Et=0;
								Trule=10.0;
							} else {
								Et=Enewn;
								fc=Et*(Tstrain-espln);
								Trule=10.0;
							}
						}
						// Fix 2

						Tstress=fc;
						Ttangent=Et;

					}
					else if (Tstrain>=esren)	{

						r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
						Trule=7.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}

					else	{

						fcEtnf(Tstrain);
					}

				}	// if (Crule==4.0)	// or 10.0 or 7.0

				else if (Crule==1.0 || Crule==5.0)	{	// or 5.0

					fcEtnf(Tstrain);

				}	// if (Crule==1.0)	// or 5.0 or 7.0

				else if (Crule==77.0)	{			// Continue on transition 77 [Rules 77,1,5]

					if (Tstrain>esrestn)	{		// Rule 77

						r77f(Tstrain,Te0,Ter0n,Tfr0n,Teunn,fnewstn,Enewstn,esrestn,frestn,Erestn);
						Trule=77.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}

					else	{						// Rules 1 and 5						  

						fcEtnf(Tstrain);

					}

				}	// if (Crule==77.0)

				else if (Crule==13.0)	{			// Continue on transition 13 [Rules 13,7,1,5]

					if (Tstrain>=Teunn)	{	// Rule 13

						r13f(Ted,Teunn,fnewn,Enewn);
						Trule=13.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						// Fix 1
						Esectest = (ff-fi) / (esf-esi);

						if (Et==Esectest) { 
							if (Tstrain>=espln) { 
								fc=0.0;
								Et=0.0;
								Trule=13.0;
							} else { 
								Et=Enewn;
								fc=Et*(Tstrain-espln);
								Trule=13.0;
							} 
						} 
						// Fix 1

						Tstress=fc;
						Ttangent=Et;
					}
					else if (Tstrain>=esren)	{	// Rule 7

						r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
						Trule=7.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}
					else	{						// Rules 1 and 5

						fcEtnf(Tstrain);					
					}

				}	// if (Crule==13.0)

				else if (Crule==11.0)	{			// Continue on transition 11 [Rules 11,(4,10 or 10),7,1,5 or Rules 11,77,1,5] 

					if (Tea!=Ter0n)	{

						if (Teb>=esplp)	{

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 4

								r4f(Teunp,Tfunp,esplp,Eplp);	

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Teb,esi,fi,esf,ff,Ei,Ef,A,R);

								fcb=fc;
								Etb=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
                            else if (Tstrain>=esplp)	{	// Rule 4

								r4f(Teunp,Tfunp,esplp,Eplp);
								Trule=4.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=Teunn)	{	// Rule 10

								r10f(esplp,Eplp,Teunn,fnewn,Enewn);
								Trule=10.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								// Fix 2 
								Esectest=(ff-fi)/(esf-esi);
								if (Et==Esectest) {
									if (Tstrain>=espln) {
										fc=0;
										Et=0;
										Trule=10.0;
									} else {
										Et=Enewn;
										fc=Et*(Tstrain-espln);
										Trule=10.0;
									}
								}
								// Fix 2

								Tstress=fc;
								Ttangent=Et;
							}

							else if (Tstrain>=esren)	{	// Rule 7						  

								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
								Trule=7.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 1 and 5

								fcEtnf(Tstrain);
							}
						}

						else if (Teb>=Teunn)	{			// and Teb<esplp

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 10

								r10f(esplp,Eplp,Teunn,fnewn,Enewn); 

								esi10=esi; //KK     
								fi10=fi;
								Ei10=Ei;
								esf10=esf;
								ff10=ff;
								Ef10=Ef;

								RAf(esi10,fi10,Ei10,esf10,ff10,Ef10);

								R10=R; //KK     
								A10=A;

								fcEturf(Teb,esi10,fi10,esf10,ff10,Ei10,Ef10,A10,R10);

								fcb=fc; //KK    
								Etb=Et; 

								// Fix 2 
								Esectest10 = (ff10-fi10)/(esf10-esi10);
								if (Etb==Esectest10) {
									if (Teb>=espln) {
										fcb=0;
										Etb=0;
									} else {
										Etb=Enewn;
										fcb=Etb*(Teb-espln);
									}
								}
								// Fix 2

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=Teunn)	{	// Rule 10

								r10f(esplp,Eplp,Teunn,fnewn,Enewn);
								Trule=10.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								// Fix 2  
								Esectest = (ff-fi)/(esf-esi);
								if (Et==Esectest) {
									if (Tstrain>=espln) {
										fc=0;
										Et=0;
										Trule=10.0;
									} else {
										Et=Enewn;
										fc=Et*(Tstrain-espln);
										Trule=10.0;
									}
								}
								// Fix 2

								Tstress=fc;
								Ttangent=Et;;
							}

							else if (Tstrain>=esren)	{	// Rule 7						  
								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
								Trule=7.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 1 and 5

								fcEtnf(Tstrain);
							}
						}

						else if (Teb>=esren)	{	

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 7

								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Teb,esi,fi,esf,ff,Ei,Ef,A,R);

								fcb=fc;
								Etb=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}

							else if (Tstrain>=esren)	{	// Rule 7						  

								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
								Trule=7.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 1 and 5

								fcEtnf(Tstrain);
							}
						}

						else	{		// (Teb<esren)

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 1 or 5

								fcEtnf(Teb);

								fcb=Tstress;
								Etb=Ttangent;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Eta,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;					  
							}
							else	{						// Rules 1 and 5

								fcEtnf(Tstrain);
							}
						}
					}

					else	{	// if (Tea==Ter0n)

						if (Teb>=esrestn)	{

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 77

								r77f(Teb,Te0,Ter0n,Tfr0n,Teunn,fnewstn,Enewstn,esrestn,frestn,Erestn);

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Teb,esi,fi,esf,ff,Ei,Ef,A,R);

								fcb=fc;
								Etb=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>esrestn)	{	// Rule 77

								r77f(Tstrain,Te0,Ter0n,Tfr0n,Teunn,fnewstn,Enewstn,esrestn,frestn,Erestn);
								Trule=77.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 1 and 5						  

								fcEtnf(Tstrain);					  
							}
						}

						else	{	// if (Teb<esrestn)

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 1 or 5

								fcEtnf(Teb);

								fcb=Tstress;
								Etb=Ttangent;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}					  
							else	{						// Rules 1 and 5
								fcEtnf(Tstrain);					  
							}
						}
					}	
				} // if (Crule==11.0)

				else if (Crule==15.0)	{					// Continue on transition 15 [Rules 15,13,7,1,5]

					if (Tstrain>=Tea)	{					// Rule 15 targeting for ea (ed) on 13

						r13f(Ted,Teunn,fnewn,Enewn);

						esi13=esi; //KK
						fi13=fi;
						Ei13=Ei;
						esf13=esf;
						ff13=ff;
						Ef13=Ef;

						RAf(esi13,fi13,Ei13,esf13,ff13,Ef13);

						R13=R; // KK
						A13=A;

						fcEturf(Tea,esi13,fi13,esf13,ff13,Ei13,Ef13,A13,R13);

						fca=fc; // KK
						Eta=Et;

						// Fix 1 
						Esectest13=(ff13-fi13)/(esf13-esi13); // KK

						if (Eta==Esectest13) { 
							if (Tea>=espln) { 
								fca=0;
								Eta=0;
							} else { 
								Eta=Enewn;
								fca=Et*(Tea-espln);
							} 
						} 
						// Fix 1

						esi=Ter;
						fi=Tfr;
						Ei=Ec;
						esf=Tea;

						RAf(esi,fi,Ei,esf,fca,Eta);

						r15f(Ter,Tfr,Tea,fca,Eta,A,R);
						Trule=15.0;

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}

					else if (Tstrain>=Teunn)	{			// Rule 13

						r13f(Ted,Teunn,fnewn,Enewn);
						Trule=13.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						// Fix 1 
						Esectest=(ff-fi)/(esf-esi);

						if (Et==Esectest) { 
							if (Tstrain>=espln) { 
								fc=0;
								Et=0;
								Trule=13.0;

							} else { 

								Et=Enewn;
								fc=Et*(Tstrain-espln);
								Trule=13.0;
							} 
						} 
						// Fix 1

						Tstress=fc;
						Ttangent=Et;

					}
					else if (Tstrain>=esren)	{			// Rule 7

						r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
						Trule=7.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}
					else	{								// Rules 1 and 5

						fcEtnf(Tstrain);					
					}
				}	// if (Crule==15.0)
			}		// if (Tstrain<=Cstrain)
		}			// if (Cinc==-1.0)

		//////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////

		else	{	// if (Cinc==1.0)

			if (Tstrain<Cstrain) 
			{    // Starts reversal from positive direction to negative direction

				if ( fabs(Cstress) == 0.0) 
				{     // Gap Model 

					Teunp=Cstrain;
					Tfunp=Cstress;

					fcEtnf(Teunn);
					Tfunn=Tstress;

					Ter=Cstrain;
					Tfr=Cstress;          

					Ted=Ter;

					fnewnf(Teunn,Tfunn);
					Enewnf(Teunn,Tfunn);

					if (Tstrain>=Teunn)	
					{		// Rule 13

						r13f(Ted,Teunn,fnewn,Enewn);
						Trule=13.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						// Fix 1  
						Esectest=(ff-fi)/(esf-esi);
						if ( Et==Esectest ) 
						{ 
							if ( Tstrain>=espln ) { 
								fc=0;
								Et=0;
								Trule=13.0;
							} else { 
								Et=Enewn;
								fc=Et*(Tstrain-espln);
								Trule=13.0;
							} 
						} 
						// Fix 1

						Tstress=fc;
						Ttangent=Et;
					} 
					else if ( Tstrain>=esren ) 
					{ 
						r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
						Trule=7.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					} 
					else 
					{
						fcEtnf(Tstrain);
					} 
				} 
				else if (Crule==2.0 || Crule==8.0) 
				{   // Starts reversal from non-cracked envelope (Rule 2) or Rule 8 [Rules 4,10,7,1,5]

					Teunp=Cstrain;

					fcEtpf(Teunp,Te0);	// need Tfunp only from fcEtpf
					Tfunp=Tstress;

					Esecpf(Te0,Teunp,Tfunp,espln);

					esplpf(Teunp,Tfunp,Te0,espln);
					Eplpf(Te0,Teunp);         

					if (Tstrain>=esplp) 
					{  
						r4f(Teunp,Tfunp,esplp,Eplp);
						Trule=4.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					} 
					else if ( Tstrain>=Teunn ) 
					{ 
						r10f(esplp,Eplp,Teunn,fnewn,Enewn);
						Trule=10.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						// Fix 2  
						Esectest=(ff-fi)/(esf-esi);
						if (Et==Esectest) {
							if (Tstrain>=espln) {
								fc=0;
								Et=0;
								Trule=10.0;
							} else {
								Et=Enewn;
								fc=Et*(Tstrain-espln);
								Trule=10.0;
							}
						}	
						// Fix 2

						Tstress=fc;
						Ttangent=Et;
					} 
					else if ( Tstrain>=esren ) 
					{ 
						r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
						Trule=7.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					} 
					else 
					{ 
						fcEtnf(Tstrain);
					} 
				}
				else if (Crule==9.0)	{

					Ter=Cstrain;
					Tfr=Cstress;

					Tea=Ter;
					fa=Tfr;

					eb1112f(Tea,espln,esplp,Teunn,Teunp);

					if (Teb>=esplp)	{

						if (Tstrain>=Teb)	{				// Rule 11 targeting for eb on 4

							r4f(Teunp,Tfunp,esplp,Eplp);

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Teb,esi,fi,esf,ff,Ei,Ef,A,R);

							fcb=fc;
							Etb=Et;

							esi=Ter;
							fi=Tfr;
							Ei=Ec;
							esf=Teb;

							RAf(esi,fi,Ei,esf,fcb,Etb);

							r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
							Trule=11.0;

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else if (Tstrain>=esplp)	{	// Rule 4

							r4f(Teunp,Tfunp,esplp,Eplp);
							Trule=4.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else if (Tstrain>=Teunn)	{	// Rule 10

							r10f(esplp,Eplp,Teunn,fnewn,Enewn);
							Trule=10.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							// Fix 2  
							Esectest=(ff-fi)/(esf-esi);

							if (Et==Esectest) {
								if (Tstrain>=espln) {
									fc=0;
									Et=0;
									Trule=10.0;
								} else {
									Et=Enewn;
									fc=Et*(Tstrain-espln);
									Trule=10.0;
								}
							}
							// Fix 2

							Tstress=fc;
							Ttangent=Et;
						}
						else if (Tstrain>=esren)	{	// Rule 7						  

							r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
							Trule=7.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else	{						// Rules 1 and 5
							fcEtnf(Tstrain);
						}
					}

					else if (Teb>=Teunn)	{			// and Teb<esplp

						if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 10

							r10f(esplp,Eplp,Teunn,fnewn,Enewn);

							esi10=esi; //KK 
							fi10=fi;
							Ei10=Ei;
							esf10=esf;
							ff10=ff;
							Ef10=Ef;

							RAf(esi10,fi10,Ei10,esf10,ff10,Ef10);

							R10=R; //KK 
							A10=A;

							fcEturf(Teb,esi10,fi10,esf10,ff10,Ei10,Ef10,A10,R10);

							fcb=fc; 
							Etb=Et; 

							// Fix 2 
							Esectest10=(ff10-fi10)/(esf10-esi10); 
							if (Etb==Esectest10) {
								if (Teb>=espln) {
									fcb=0;
									Etb=0;
								} else {
									Etb=Enewn;
									fcb=Etb*(Teb-espln);
								}
							}
							// Fix 2

							esi=Ter;
							fi=Tfr;
							Ei=Ec;
							esf=Teb;

							RAf(esi,fi,Ei,esf,fcb,Etb);

							r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
							Trule=11.0;

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else if (Tstrain>=Teunn)	{	// Rule 10

							r10f(esplp,Eplp,Teunn,fnewn,Enewn);
							Trule=10.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							// Fix 2  
							Esectest = (ff-fi)/(esf-esi);
							if (Et==Esectest) {
								if (Tstrain>=espln) {
									fc=0;
									Et=0;
									Trule=10.0;
								} else {
									Et=Enewn;
									fc=Et*(Tstrain-espln);
									Trule=10.0;
								}
							}	
							// Fix 2

							Tstress=fc;
							Ttangent=Et;;
						}
						else if (Tstrain>=esren)	{	// Rule 7						  

							r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
							Trule=7.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else	{						// Rules 1 and 5
							fcEtnf(Tstrain);
						}
					}

					else if (Teb>=esren)	{	

						if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 7

							r7f(Teunn,fnewn,Enewn,esren,fren,Eren);

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Teb,esi,fi,esf,ff,Ei,Ef,A,R);

							fcb=fc;
							Etb=Et;

							esi=Ter;
							fi=Tfr;
							Ei=Ec;
							esf=Teb;

							RAf(esi,fi,Ei,esf,fcb,Etb);

							r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
							Trule=11.0;

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else if (Tstrain>=esren)	{	// Rule 7						  

							r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
							Trule=7.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else	{						// Rules 1 and 5
							fcEtnf(Tstrain);
						}
					}

					else	{		// (Teb<esren)

						if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 1 or 5

							fcEtnf(Teb);

							fcb=Tstress;
							Etb=Ttangent;

							esi=Ter;
							fi=Tfr;
							Ei=Ec;
							esf=Teb;

							RAf(esi,fi,Ei,esf,fcb,Etb);

							r11f(Ter,Tfr,Teb,fcb,Eta,A,R);
							Trule=11.0;

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;					  
						}
						else	{						// Rules 1 and 5
							fcEtnf(Tstrain);
						}
					}
				} // if (Crule=9.0)

				else if (Crule==12.0)	{

					Ter=Cstrain;
					Tfr=Cstress;

					if (Tea!=Ter0n)	{

						if (Teb>=esplp)	{

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 4

								r4f(Teunp,Tfunp,esplp,Eplp);	

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Teb,esi,fi,esf,ff,Ei,Ef,A,R);

								fcb=fc;
								Etb=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);

								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=esplp)	{	// Rule 4

								r4f(Teunp,Tfunp,esplp,Eplp);
								Trule=4.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=Teunn)	{	// Rule 10

								r10f(esplp,Eplp,Teunn,fnewn,Enewn);
								Trule=10.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								// Fix 2  
								Esectest=(ff-fi)/(esf-esi);
								if (Et==Esectest) {
									if (Tstrain>=espln) {
										fc=0;
										Et=0;
										Trule=10.0;
									} else {
										Et=Enewn;
										fc=Et*(Tstrain-espln);
										Trule=10.0;
									}
								}
								// Fix 2

								Tstress=fc;
								Ttangent=Et;

							}
							else if (Tstrain>=esren)	{	// Rule 7						  

								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
								Trule=7.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 1 and 5
								fcEtnf(Tstrain);
							}
						}

						else if (Teb>=Teunn)	{			// and Teb<esplp

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 10

								r10f(esplp,Eplp,Teunn,fnewn,Enewn); 

								esi10=esi; //KK
								fi10=fi;
								Ei10=Ei;
								esf10=esf;
								ff10=ff;
								Ef10=Ef;

								RAf(esi10,fi10,Ei10,esf10,ff10,Ef10);

								R10=R; //KK
								A10=A;

								fcEturf(Teb,esi10,fi10,esf10,ff10,Ei10,Ef10,A10,R10);

								fcb=fc;
								Etb=Et;

								// Fix 2 
								Esectest10 = (ff10-fi10)/(esf10-esi10);
								if (Etb==Esectest10) {
									if (Teb>=espln) {
										fcb=0;
										Etb=0;
									} else {
										Etb=Enewn;
										fcb=Etb*(Teb-espln);
									}
								}
								// Fix 2

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=Teunn)	{	// Rule 10

								r10f(esplp,Eplp,Teunn,fnewn,Enewn);
								Trule=10.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								// Fix 2 
								Esectest = (ff-fi)/(esf-esi);
								if (Et==Esectest) {
									if (Tstrain>=espln) {
										fc=0;
										Et=0;
										Trule=10.0;
									} else { 
										Et=Enewn;
										fc=Et*(Tstrain-espln);
										Trule=10.0;
									}
								}
								// Fix 2

								Tstress=fc;
								Ttangent=Et;;
							}
							else if (Tstrain>=esren)	{	// Rule 7						  

								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
								Trule=7.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 1 and 5
								fcEtnf(Tstrain);
							}
						}

						else if (Teb>=esren)	{	

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 7
								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Teb,esi,fi,esf,ff,Ei,Ef,A,R);

								fcb=fc;
								Etb=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=esren)	{	// Rule 7						  

								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
								Trule=7.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 1 and 5
								fcEtnf(Tstrain);
							}
						}

						else	{		// (Teb<esren)

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 1 or 5

								fcEtnf(Teb);

								fcb=Tstress;
								Etb=Ttangent;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Eta,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;					  
							}
							else	{						// Rules 1 and 5
								fcEtnf(Tstrain);
							}
						}
					}

					else	{	// if (Tea==Ter0n)

						if (Teb>=esrestn)	{

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 77

								r77f(Teb,Te0,Ter0n,Tfr0n,Teunn,fnewstn,Enewstn,esrestn,frestn,Erestn);

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Teb,esi,fi,esf,ff,Ei,Ef,A,R);

								fcb=fc;
								Etb=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>esrestn)	{	// Rule 77

								r77f(Tstrain,Te0,Ter0n,Tfr0n,Teunn,fnewstn,Enewstn,esrestn,frestn,Erestn);
								Trule=77.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 1 and 5						  
								fcEtnf(Tstrain);					  
							}
						}

						else	{	// if (Teb<esrestn)

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 1 or 5

								fcEtnf(Teb);

								fcb=Tstress;
								Etb=Ttangent;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}					  
							else	{						// Rules 1 and 5
								fcEtnf(Tstrain);					  
							}
						}
					}	
				} // if (Crule==12.0)

				else if (Crule==14.0)	{			// Reversal from transition 14 [Rules 15,13,7,1,5]

					Ter=Cstrain;
					Tfr=Cstress;

					if (Tstrain>=Tea)	{			// Rule 15 targeting for ea (ed) on 13

						r13f(Ted,Teunn,fnewn,Enewn);

						esi13=esi; // KK
						fi13=fi;
						Ei13=Ei;
						esf13=esf;
						ff13=ff;
						Ef13=Ef;

						RAf(esi13,fi13,Ei13,esf13,ff13,Ef13);

						R13=R; // KK 
						A13=A;

						fcEturf(Tea,esi13,fi13,esf13,ff13,Ei13,Ef13,A13,R13);

						fca=fc;
						Eta=Et;

						// Fix 1 
						Esectest13=(ff13-fi13)/(esf13-esi13);

						if (Eta==Esectest13) { 
							if (Tea>=espln) { 
								fca=0;
								Eta=0;
							} else { 
								Eta=Enewn;
								fca=Et*(Tea-espln);
							} 
						} 
						//Fix 1

						esi=Ter;
						fi=Tfr;
						Ei=Ec;
						esf=Tea;

						RAf(esi,fi,Ei,esf,fca,Eta);

						r15f(Ter,Tfr,Tea,fca,Eta,A,R);
						Trule=15.0;

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}
					else if (Tstrain>=Teunn)	{	// Rule 13

						r13f(Ted,Teunn,fnewn,Enewn);
						Trule=13.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						// Fix 1  
						Esectest=(ff-fi)/(esf-esi);
						if (Et==Esectest) { 
							if (Tstrain>=espln) { 
								fc=0;
								Et=0;
								Trule=13.0;
							} else { 
								Et=Enewn;
								fc=Et*(Tstrain-espln);
								Trule=13.0;
							} 
						} 
						// Fix 1

						Tstress=fc;
						Ttangent=Et;
					}
					else if (Tstrain>=esren)	{	// Rule 7

						r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
						Trule=7.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}
					else	{						// Rules 1 and 5
						fcEtnf(Tstrain);					
					}
				} // if (Crule==14.0)

				else if (Crule==3.0)	{

					Ter0n=Cstrain;
					Tfr0n=Cstress;

					Tea=Ter0n;

					fnewstnf(Tfunn,delfn,Teunn,Ter0n,espln);
					Enewstnf(fnewstn,Tfr0n,Teunn,Ter0n);

					esrestnf(Teunn,delen,Ter0n,espln);   
					freErestnf(Teunn,Tfunn,Ter0n);

					if (Tstrain>esrestn)	{		// Rule 77

						r77f(Tstrain,Te0,Ter0n,Tfr0n,Teunn,fnewstn,Enewstn,esrestn,frestn,Erestn);
						Trule=77.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}
					else	{						// Rules 1 and 5						  
						fcEtnf(Tstrain);					  
					}
				} // if (Crule==3.0)

				else if (Crule==88.0)	{			// Reversal from transition 88 [Rules 11,(4,10 or 10),7,1,5 or Rules 4,10,7,1,5] 

					if (Cstrain<=Teunp)	{			

						Ter=Cstrain;
						Tfr=Cstress;

						Tea=Ter;

						Teb=Ter0p;

						if (Teb>=esplp)	{				// Reversal from 88 by Rules [11,4,10,7,1,5]	

							if (Tstrain>=Teb)	{		// Rule 11 targeting for eb on 4

								r4f(Teunp,Tfunp,esplp,Eplp);	

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Teb,esi,fi,esf,ff,Ei,Ef,A,R);

								fcb=fc;
								Etb=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=esplp)	{	// Rule 4

								r4f(Teunp,Tfunp,esplp,Eplp);
								Trule=4.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=Teunn)	{	// Rule 10

								r10f(esplp,Eplp,Teunn,fnewn,Enewn);
								Trule=10.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								// Fix 2 
								Esectest = (ff-fi)/(esf-esi);
								if (Et==Esectest) {
									if (Tstrain>=espln) {
										fc=0;
										Et=0;
										Trule=10.0;
									} else {
										Et=Enewn;
										fc=Et*(Tstrain-espln);
										Trule=10.0;
									}
								}
								// Fix 2

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=esren)	{	// Rule 7						  

								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
								Trule=7.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 1 and 5
								fcEtnf(Tstrain);
							}
						}

						else if (Teb>=Teunn)	{			// Reversal from 88 by Rules [11,10,7,1,5]

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 10

								r10f(esplp,Eplp,Teunn,fnewn,Enewn); 

								esi10=esi; // KK 
								fi10=fi;
								Ei10=Ei;
								esf10=esf;
								ff10=ff;
								Ef10=Ef;

								RAf(esi10,fi10,Ei10,esf10,ff10,Ef10);

								R10=R; // KK 
								A10=A; 

								fcEturf(Teb,esi10,fi10,esf10,ff10,Ei10,Ef10,A10,R10);

								fcb=fc;
								Etb=Et;

								// Fix 2
								Esectest10 = (ff10-fi10)/(esf10-esi10);
								if (Etb==Esectest10) {
									if (Teb>=espln) {
										fcb=0;
										Etb=0;
									} else {
										Etb=Enewn;
										fcb=Etb*(Teb-espln);
									}
								}
								// Fix 2

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Tea,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=Teunn)	{	// Rule 10

								r10f(esplp,Eplp,Teunn,fnewn,Enewn);
								Trule=10.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								// Fix 2 
								Esectest = (ff-fi)/(esf-esi);
								if (Et==Esectest) {
									if (Tstrain>=espln) {
										fc=0;
										Et=0;
										Trule=10.0;
									} else {
										Et=Enewn;
										fc=Et*(Tstrain-espln);
										Trule=10.0;
									}
								}
								// Fix 2

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=esren)	{	// Rule 7						  

								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
								Trule=7.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 1 and 5
								fcEtnf(Tstrain);
							}
						}

						else if (Teb>=esren)	{			// Reversal from 88 by Rules [11,7,1,5]

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 7

								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Teb,esi,fi,esf,ff,Ei,Ef,A,R);

								fcb=fc;
								Etb=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain>=esren)	{	// Rule 7						  

								r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
								Trule=7.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 1 and 5
								fcEtnf(Tstrain);
							}			  
						}

						else	{		// (Teb<esren)		// Reversal from 88 by Rules [11,1,5]

							if (Tstrain>=Teb)	{			// Rule 11 targeting for eb on 1 or 5

								fcEtnf(Teb);

								fcb=Tstress;
								Etb=Ttangent;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Teb;

								RAf(esi,fi,Ei,esf,fcb,Etb);

								r11f(Ter,Tfr,Teb,fcb,Etb,A,R);
								Trule=11.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}					  
							else	{						// Rules 1 and 5
								fcEtnf(Tstrain);					  
							}
						}				  
					}	// if (Cstrain<=Teunp)

					else	{	// if (Cstrain>Teunp)		// Reversal from transition 88 by Rules [4,10,7,1,5] 

						Teunp=Cstrain;				  
						fcEtpf(Teunp,Te0);
						Tfunp=Tstress;

						esplpf(Teunp,Tfunp,Te0,espln);
						Eplpf(Te0,Teunp);

						if (Tstrain>=esplp)	{

							r4f(Teunp,Tfunp,esplp,Eplp);
							Trule=4.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else if (Tstrain>=Teunn)	{

							r10f(esplp,Eplp,Teunn,fnewn,Enewn);
							Trule=10.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							// Fix 2 
							Esectest = (ff-fi)/(esf-esi);
							if (Et==Esectest) {
								if (Tstrain>=espln) {
									fc=0;
									Et=0;
									Trule=10.0;
								} else {
									Et=Enewn;
									fc=Et*(Tstrain-espln);
									Trule=10.0;
								}
							}
							// Fix 2

							Tstress=fc;
							Ttangent=Et;
						}

						else if (Tstrain>=esren)	{

							r7f(Teunn,fnewn,Enewn,esren,fren,Eren);
							Trule=7.0;

							RAf(esi,fi,Ei,esf,ff,Ef);

							fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

							Tstress=fc;
							Ttangent=Et;
						}
						else	{
							fcEtnf(Tstrain);
						}				  
					}	// if (Cstrain<Teunn)			  
				}	// if (Crule==77.0)
			}	// if (Tstrain<Cstrain)

			//////////////////////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////////////////////

			else	{	// if (Tstrain>=Cstrain)				// Continue going to positive direction	

				if (Crule==3.0 || Crule==9.0 || Crule==8.0)	{	// Continue on transition 3 or 9 or 8 [Rules 3,9,8,2,6]

					if (Tstrain<=espln)	{

						r3f(Teunn,Tfunn,espln,Epln);
						Trule=3.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}
					else if (Tstrain<=Teunp)	{

						r9f(espln,Epln,Teunp,fnewp,Enewp);
						Trule=9.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}
					else if (Tstrain<=esrep)	{

						r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
						Trule=8.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;
					}
					else	{
						fcEtpf(Tstrain,Te0);
					}

				}	// if (Crule==3)	or 9 or 8

				else if (Crule==2.0)	{		// Continue on transition 2 [Rules 2,6]

					fcEtpf(Tstrain,Te0);
				}	// if (Crule==2)

				else if (Crule==6.0)	{		// Continue on transition 6 [Rule 6]

					fcEtpr6f(Tstrain,Te0);
					Trule=6.0; 

				}	// if (Crule==6)

				else if (Crule==88.0)	{		// Continue on transition 88 [Rules 88,2,6]

					if (Tstrain<esrestp)	{	// Rule 88							  

						r88f(Tstrain,Te0,Ter0p,Tfr0p,Teunp,fnewstp,Enewstp,esrestp,frestp,Erestp);
						Trule=88.0;							  

						RAf(esi,fi,Ei,esf,ff,Ef);							  

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);							  

						Tstress=fc;							  
						Ttangent=Et;						  
					}						  
					else	{							// Rules 2 and 6						  
						fcEtpf(Tstrain,Te0);					  						  
					}
				}

				else if (Crule==12.0)	{		// Continue on transition 12 [Rules 12,(3,9 or 9),8,2,6] or [Rules 12,88,2,6]	

					if (Teb!=Ter0p)	{

						if (Tea<=espln)	{

							if (Tstrain<=Tea)	{	// Rule 12 targeting for ea on 3							  

								r3f(Teunn,Tfunn,espln,Epln);   

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

								fca=fc;
								Eta=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}
							else if (Tstrain<=espln)	{	// Rule 3

								r3f(Teunn,Tfunn,espln,Epln);
								Trule=3.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain<=Teunp)	{	// Rule 9

								r9f(espln,Epln,Teunp,fnewp,Enewp);
								Trule=9.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;		  
							}
							else if (Tstrain<=esrep)	{	// Rule 8						  

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
								Trule=8.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}		  
							else	{						// Rules 2 and 6						  
								fcEtpf(Tstrain,Te0);					  
							}
						}

						else if (Tea<=Teunp)	{			// (Tea>espln)

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 9						  

								r9f(espln,Epln,Teunp,fnewp,Enewp); 

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

								fca=fc;
								Eta=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;			  
							}
							else if (Tstrain<=Teunp)	{	// Rule 9						  

								r9f(espln,Epln,Teunp,fnewp,Enewp);
								Trule=9.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain<=esrep)	{	// Rule 8						  

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
								Trule=8.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{						// Rules 2 and 6			  
								fcEtpf(Tstrain,Te0);		  
							}
						}

						else if (Tea<=esrep)	{	

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 8

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

								fca=fc;
								Eta=Et;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}					  
							else if (Tstrain<=esrep)	{	// Rule 8						  

								r8f(Teunp,fnewp,Enewp,esrep,frep,Erep);
								Trule=8.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}					  
							else	{						// Rules 2 and 6						  
								fcEtpf(Tstrain,Te0);					  
							}				  
						}

						else	{		// (Tea>esrep)

							if (Tstrain<=Tea)	{			// Rule 12 targeting for ea on 2 or 6

								fcEtpf(Tea,Te0);

								fca=Tstress;
								Eta=Ttangent;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}					  
							else	{						// Rules 2 and 6
								fcEtpf(Tstrain,Te0);					  
							}
						}
					}

					else	{	// if (Teb==Ter0p)

						if (Tea<=esrestp)	{

							if (Tstrain<=Tea)	{				// Rule 12 targeting for ea on 88

								r88f(Tea,Te0,Ter0p,Tfr0p,Teunp,fnewstp,Enewstp,esrestp,frestp,Erestp);

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tea,esi,fi,esf,ff,Ei,Ef,A,R);

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else if (Tstrain<=esrestp)	{	// Rule 88

								r88f(Tstrain,Te0,Ter0p,Tfr0p,Teunp,fnewstp,Enewstp,esrestp,frestp,Erestp);
								Trule=88.0;

								RAf(esi,fi,Ei,esf,ff,Ef);

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;
							}
							else	{							// Rules 2 and 6						  
								fcEtpf(Tstrain,Te0);					  
							}
						}

						else	{	// if (Tea>esrestp)

							if (Tstrain<=Tea)	{				// Rule 12 targeting for ea on 2 or 6

								fcEtpf(Tea,Te0);

								fca=Tstress;
								Eta=Ttangent;

								esi=Ter;
								fi=Tfr;
								Ei=Ec;
								esf=Tea;

								RAf(esi,fi,Ei,esf,fca,Eta);

								r12f(Ter,Tfr,Tea,fca,Eta,A,R);
								Trule=12.0;

								fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

								Tstress=fc;
								Ttangent=Et;				  
							}					  
							else	{							// Rules 2 and 6
								fcEtpf(Tstrain,Te0);					  
							}
						}
					}
				} // if (Crule==12.0)

				else if (Crule==14.0)	{						// Continue on transition 14 [Rules 14,66,6]

					if (Tstrain<=Teb)	{						// Rule 14

						r14f(Ter,Tfr,Teb);
						Trule=14.0;

						RAf(esi,fi,Ei,esf,ff,Ef);

						fcEturf(Tstrain,esi,fi,esf,ff,Ei,Ef,A,R);

						Tstress=fc;
						Ttangent=Et;

					}
					else if (Tstrain<Teunp)	{				// Rule 66

						r66f(Tstrain,Te0);
						Trule=66.0;

					}
					else	{								// Rule 6

						fcEtpr6f(Tstrain,Te0);
						Trule=6.0; 
					}
				}

				else if (Crule==66)	{						// Continue on transition 66 [Rules 66,6]

					if (Tstrain<Teunp)	{

						r66f(Tstrain,Te0);
						Trule=66.0;
					}
					else	{								// Rule 6
						fcEtpr6f(Tstrain,Te0);
						Trule=6.0; 
					}

				}

			}	// if (Tstrain>=Cstrain)

		}	//	if (Cinc==1.0)

	}	// if monotonic or cyclic 

	return 0;	}


	void ConcreteCM::fcEtnf(double e)
	{
		xn=fabs(e/epcc);
		nn=fabs(Ec*epcc/fpcc);
		yf(xcrn,nn,rc);
		zf(xcrn,nn,rc);
		xsp=fabs(xcrn-y/(nn*z));

		if (xn<=xsp) {
			r1f(xn,nn,rc);
			Trule=1.0;
			//		Ttangent=xsp;
		}
		else {
			r5f(xn,nn,rc);
			Trule=5.0;
			//		Ttangent=xsp;
		}
	}

	void ConcreteCM::fcEtpf(double e, double e0)
	{
		xp=fabs((e-e0)/et);
		np=Ec*et/ft;
		yf(xcrp,np,rt);
		zf(xcrp,np,rt);
		xcrk=fabs(xcrp-y/(np*z));

		if (xp<=xcrk) {	
			r2f(xp,np,rt);
			Trule=2.0;
			//		Ttangent=xcrk; 
		}
		else {
			r6f(xp,np,rt);
			Trule=6.0;
			//		Ttangent=xcrk;  
		}
	}

	void ConcreteCM::fcEtpr6f(double e, double e0)
	{
		xp=fabs((e-e0)/et);
		np=Ec*et/ft;
		r6f(xp,np,rt);
		Trule=6.0;
	}

	void ConcreteCM::yf(double x, double n, double r)
	{
		double D;

		if (r!=1.0) {				
			D = 1.0 + (n - r / (r - 1.0))*x + pow(x, r) / (r - 1.0);
		}
		else {
			D = 1 + (n - 1 + log10(x))*x;;
		}

		y=n*x/D;
	}

	void ConcreteCM::zf(double x, double n, double r)
	{
		double D;
		if (r!=1.0) {
			D=1.0+(n-r/(r-1.0))*x+pow(x,r)/(r-1.0);
		}
		else {
			D=1.0+(n-1.0+log10(x))*x;	//
		}

		z=(1-pow(x,r))/pow(D,2.0);
	}

	void ConcreteCM::esplnf(double eunn, double funn)
	{
		Esecnf(eunn,funn);
		espln=eunn-funn/Esecn;
	}

	void ConcreteCM::Eplnf(double eunn)
	{
		Epln=0.1*Ec*exp(-2.0*fabs(eunn/epcc));
	}	  

	void ConcreteCM::Esecnf(double eunn, double funn)
	{
		Esecn=Ec*((fabs(funn/(Ec*epcc))+0.57)/(fabs(eunn/epcc)+0.57));
	}	  

	void ConcreteCM::delenf(double eunn)
	{
		   delen=eunn/(1.15+2.75*fabs(eunn/epcc));
	}	

	void ConcreteCM::delfnf(double eunn, double funn)
	{
		if (eunn<=epcc/10.0)	{
			 delfn=0.09*funn*pow(fabs(eunn/epcc),0.5);  
		}
		else	{
			   delfn=0.0;
		}
	}

	void ConcreteCM::fnewnf(double eunn, double funn)
	{
		delfnf(eunn,funn);
		fnewn=funn-delfn;
	}

	void ConcreteCM::Enewnf(double eunn, double funn)
	{
		fnewnf(eunn,funn);
		esplnf(eunn,funn);
		Enewn=fmin(Ec,(fnewn/(eunn-espln))); 

		if (eunn == espln) {Enewn =  Ec;} 
	}

	void ConcreteCM::esrenf(double eunn)
	{
		delenf(eunn);
		esren=eunn+delen;
	}

	void ConcreteCM::freErenf(double eunn) 
	{
		esrenf(eunn);

		xn=fabs(esren/epcc);
		nn=fabs(Ec*epcc/fpcc);
		yf(xcrn,nn,rc);     
		zf(xcrn,nn,rc);     
		xsp=fabs(xcrn-y/(nn*z));

		if (xn<=xsp) {
			if (xn<xcrn) {
				yf(xn,nn,rc);
				zf(xn,nn,rc);
				fren=fpcc*y;
				Eren=Ec*z;
			}
			else {
				yf(xcrn,nn,rc);
				zf(xcrn,nn,rc);
				fren=fpcc*(y+nn*z*(xn-xcrn));
				Eren=Ec*z;
			}
		}
		else {
			fren=0.0;
			Eren=0.0;
		}
	}

	void ConcreteCM::fnewstnf(double funn, double delfn, double eunn, double er0n, double espln)
	{
		fnewstn=funn-delfn*((eunn-er0n)/(eunn-espln));
	}

	void ConcreteCM::Enewstnf(double fnewstn, double fr0n, double eunn, double er0n)
	{
		Enewstn=(fnewstn-fr0n)/(eunn-er0n);
	}

	void ConcreteCM::esrestnf(double eunn, double delen, double er0n, double espln)
	{
		esrestn=eunn+delen*(eunn-er0n)/(eunn-espln);
	}

	void ConcreteCM::freErestnf(double eunn, double funn, double er0n)
	{
		delenf(eunn);
		esplnf(eunn,funn);
		esrestnf(eunn,delen,er0n,espln);

		xn=fabs(esrestn/epcc);
		nn=fabs(Ec*epcc/fpcc);
		yf(xcrn,nn,rc);
		zf(xcrn,nn,rc);
		xsp=fabs(xcrn-y/(nn*z));

		if (xn<=xsp) {	
			if (xn<xcrn) {
				yf(xn,nn,rc);
				zf(xn,nn,rc);
				frestn=fpcc*y;
				Erestn=Ec*z;
			}
			else {
				yf(xcrn,nn,rc);
				zf(xcrn,nn,rc);
				frestn=fpcc*(y+nn*z*(xn-xcrn));
				Erestn=Ec*z;
			}
		}
		else {
			frestn=0.0;
			Erestn=0.0;
		}
	}

	void ConcreteCM::esplpf(double eunp, double funp, double e0, double espln)
	{
		Esecpf(e0,eunp,funp,espln);
		esplp=eunp-funp/Esecp;
	}

	void ConcreteCM::Eplpf(double e0, double eunp)
	{
		if (Gap == 1){
			Eplp = Ec / (pow((fabs((eunp - e0) / et)), 1.1) + 1.0);    // Less gradual gap closure (optional)
		} else {		
			Eplp=0.0;   // More gradual gap closure (default)
		}
	}	  

	void ConcreteCM::Esecpf(double e0, double eunp, double funp, double espln)
	{
		 Esecp=Ec*((fabs(funp/(Ec*et))+0.67)/(fabs((eunp-e0)/et)+0.67));

		  if (Esecp<(fabs(funp/(fabs(eunp-espln)))))  { 
    
			 Esecp=fabs(funp/(fabs(eunp-espln)));
	     }
	}	  

	void ConcreteCM::delepf(double eunp, double e0)
	{
		    delep=0.22*fabs(eunp-e0);
	}	

	void ConcreteCM::delfpf(double funp, double eunp, double e0)
	{
		if (eunp>=e0+et/2.0)	{
			   delfp=0.15*funp;  
		}
		else	{
		     delfp=0.0;
		}
	}

	void ConcreteCM::fnewpf(double funp, double eunp, double e0)
	{
		delfpf(funp,eunp,e0);
		fnewp=funp-delfp;
	}

	void ConcreteCM::Enewpf(double eunp, double funp, double e0, double espln)
	{
		fnewpf(funp,eunp,e0);
		esplpf(eunp,funp,e0,espln);
		Enewp=fmin(Ec,(fnewp/(eunp-esplp)));

		if (eunp == esplp) {Enewp =  Ec;}
	}

	void ConcreteCM::esrepf(double eunp, double e0)
	{
		delepf(eunp,e0);
		esrep=eunp+delep;
	}
	
	void ConcreteCM::freErepf(double eunp, double e0) 
	{
		esrepf(eunp,e0);

		xp=fabs((esrep-e0)/et);
		np=Ec*et/ft;
		yf(xcrp,np,rt);
		zf(xcrp,np,rt);
		xcrk=fabs(xcrp-y/(np*z));

		if (xp<=xcrk) {		
			if (xp<xcrp) {
				yf(xp,np,rt);
				zf(xp,np,rt);
				frep=ft*y;
				Erep=Ec*z;
			}
			else {
				yf(xcrp,np,rt);
				zf(xcrp,np,rt);
				frep=ft*(y+np*z*(xp-xcrp));
				Erep=Ec*z;	
			}
		}
		else {
			frep=0.0;
			Erep=0.0;
 
		}
	}

	void ConcreteCM::fnewstpf(double funp, double delfp, double eunp, double er0p, double esplp, double e0)
	{
		fnewstp=funp-delfp*((eunp-er0p)/(eunp-esplp));
	}

	void ConcreteCM::Enewstpf(double fnewstp, double fr0p, double eunp, double er0p)
	{
		Enewstp=(fnewstp-fr0p)/(eunp-er0p);
	}

	void ConcreteCM::esrestpf(double eunp, double delep, double er0p, double esplp)
	{
		esrestp=eunp+delep*(eunp-er0p)/(eunp-esplp);
	}

	void ConcreteCM::freErestpf(double eunp, double funp, double er0p, double e0, double espln)
	{
		delepf(eunp,e0);
		esplpf(eunp,funp,e0,espln);
		esrestpf(eunp,delep,er0p,esplp);

		xp=fabs((esrestp-e0)/et);
		np=Ec*et/ft;
		yf(xcrp,np,rt);
		zf(xcrp,np,rt);
		xcrk=fabs(xcrp-y/(np*z));

		if (xp<=xcrk) {			
			if (xp<xcrp) {
				yf(xp,np,rt);
				zf(xp,np,rt);
				frestp=ft*y;
				Erestp=Ec*z;
			}
			else {
				yf(xcrp,np,rt);
				zf(xcrp,np,rt);
				frestp=ft*(y+np*z*(xp-xcrp));
				Erestp=Ec*z;	
			}
		}
		else {
			frestp=0.0;
			Erestp=0.0;
		}
	}

	void ConcreteCM::e0eunpfunpf(double e0,double eunp, double funp, double eunn, double funn)
	{
		double xun=fabs(eunn/epcc);
		double xup=fabs((eunp-e0)/et);

		double e0ref;
		double eunpref;
		double funpref;

		if (xup<xun)	{
			xup=xun;
			e0ref=0.0;
			eunpref=xup*et;
			fcEtpf(eunpref,e0ref);
			funpref=Tstress;
		}
		else	{
			xup=xup;
			e0ref=e0;
			eunpref=eunp;
			funpref=funp;  
		}

		esplnf(eunn,funn);						
		Eplnf(eunn);							
		Esecpf(e0ref,eunpref,funpref,espln);	

		double dele0=2.0*funpref/(Esecp+Epln);

		Te0=espln+dele0-xup*et;
		Teunp=xup*et+Te0;
		fcEtpf(Teunp,Te0);
		Tfunp=Tstress;

	}


	void ConcreteCM::r1f(double x, double n, double r)
	{
		if (x<xcrn) {
			yf(x,n,r);
			zf(x,n,r);
			Tstress=fpcc*y;
			Ttangent=Ec*z;
		}
		else {
			yf(xcrn,n,r);
			zf(xcrn,n,r);
			Tstress=fpcc*(y+n*z*(x-xcrn));
			Ttangent=Ec*z;
		}
	}


	void ConcreteCM::r5f(double x, double n, double r)
	{
		Tstress=0.0;
		Ttangent=0.0;
	}


	void ConcreteCM::r2f(double x, double n, double r)
	{
		if (x<xcrp) {
			yf(x,n,r);
			zf(x,n,r);
			Tstress=ft*y;
			Ttangent=Ec*z;
		}
		else {
			yf(xcrp,n,r);
			zf(xcrp,n,r);
			Tstress=ft*(y+n*z*(x-xcrp));
			Ttangent=Ec*z;
		}
	}

	void ConcreteCM::r6f(double x, double n, double r)
	{
		Tstress=0.0;
		Ttangent=0.0;
	}

	void ConcreteCM::r3f(double eunn, double funn, double espln, double Epln)
	{
		esi=eunn;
		fi=funn;
		Ei=Ec;
		esf=espln;
		ff=0.0;
		Ef=Epln;
	}

	void ConcreteCM::r9f(double espln, double Epln, double eunp, double fnewp, double Enewp)
	{
		esi=espln;
		fi=0.0;
		Ei=Epln;
		esf=eunp;
		ff=fnewp;
		Ef=Enewp;
	}

	void ConcreteCM::r8f(double eunp, double fnewp, double Enewp, double esrep, double frep, double Erep)
	{
		esi=eunp;
		fi=fnewp;
		Ei=Enewp;
		esf=esrep;
		ff=frep;
		Ef=Erep;
	}

	void ConcreteCM::r4f(double eunp, double funp, double esplp, double Eplp)
	{
		esi=eunp;
		fi=funp;
		Ei=Ec;
		esf=esplp;
		ff=0.0;
		Ef=Eplp;
	}

	void ConcreteCM::r10f(double esplp, double Eplp, double eunn, double fnewn, double Enewn)
	{
		esi=esplp;
		fi=0.0;
		Ei=Eplp;
		esf=eunn;
		ff=fnewn;
		Ef=Enewn;
	}

	void ConcreteCM::r7f(double eunn, double fnewn, double Enewn, double esren, double fren, double Eren)
	{
		esi=eunn;
		fi=fnewn;
		Ei=Enewn;
		esf=esren;
		ff=fren;
		Ef=Eren;
	}

	void ConcreteCM::r12f(double er, double fr, double ea, double fca, double Eta, double A, double R)
	{
		esi=er;
		fi=fr;
		Ei=Ec;
		esf=ea;
		ff=fca;
		Ef=Eta;
		fcEturf(ea,esi,fi,esf,ff,Ei,Ef,A,R);
		ff=fc;
		Ef=Et;
	}

	void ConcreteCM::r11f(double er, double fr, double eb, double fcb, double Etb, double A, double R)
	{
		esi=er;
		fi=fr;
		Ei=Ec;
		esf=eb;
		ff=fcb;
		Ef=Etb;
		fcEturf(eb,esi,fi,esf,ff,Ei,Ef,A,R);
		ff=fc;
		Ef=Et;
	}

	void ConcreteCM::r13f(double ed, double eunn, double fnewn, double Enewn)
	{
		esi=ed;
		fi=0.0;
		Ei=0.0;
		esf=eunn;
		ff=fnewn;
		Ef=Enewn;
	}

	void ConcreteCM::r14f(double er, double fr, double eb)
	{
		esi=er;
		fi=fr;
		Ei=Ec;
		esf=eb;
		ff=0.0;
		Ef=0.0;
	}

	void ConcreteCM::r15f(double er, double fr, double ea, double fca, double Eta, double A, double R)
	{
		esi=er;
		fi=fr;
		Ei=Ec;
		esf=ea;
		ff=fca;
		Ef=Eta;
		fcEturf(ea,esi,fi,esf,ff,Ei,Ef,A,R);
		ff=fc;
		Ef=Et;
	}

	void ConcreteCM::r66f(double e, double e0)
	{
		Tstress=0.0;
		Ttangent=0.0;
	}

	void ConcreteCM::r88f(double e, double e0, double er0p, double fr0p, double eunp, double fnewstp, double Enewstp, double esrestp, double frestp, double Erestp)
	{
		if ((e-e0)>=(er0p-e0) && (e-e0)<=(eunp-e0))	{
			esi=er0p;
			fi=fr0p;
			Ei=Ec;
			esf=eunp;
			ff=fnewstp;
			Ef=Enewstp;
		}
		if ((e-e0)>(eunp-e0) && (e-e0)<(esrestp-e0))	{
			esi=eunp;
			fi=fnewstp;
			Ei=Enewstp;
			esf=esrestp;
			ff=frestp;
			Ef=Erestp; 
		}
	}

	void ConcreteCM::r77f(double e, double e0, double er0n, double fr0n, double eunn, double fnewstn, double Enewstn, double esrestn, double frestn, double Erestn)
	{

		if (e<=er0n && e>=eunn)	{
			esi=er0n;
			fi=fr0n;
			Ei=Ec;
			esf=eunn;
			ff=fnewstn;
			Ef=Enewstn;
		}
		if (e<eunn && e>esrestn)	{
			esi=eunn;
			fi=fnewstn;
			Ei=Enewstn;
			esf=esrestn;
			ff=frestn;
			Ef=Erestn; 
		}
	}

	void ConcreteCM::ea1112f(double eb, double espln, double esplp, double eunn, double eunp)
	{
		Tea=espln+((eunn-eb)/(eunn-esplp))*(eunp-espln);
	}

	void ConcreteCM::eb1112f(double ea, double espln, double esplp, double eunn, double eunp)
	{
		Teb=eunn-((ea-espln)/(eunp-espln))*(eunn-esplp);
	}

	void ConcreteCM::eb1415f(double ea, double fa, double Esecn)
	{
		Teb=ea-fa/Esecn;
	}

	void ConcreteCM::RAf(double esi, double fi, double Ei, double esf, double ff, double Ef)
	{
		double Esec=(ff-fi)/(esf-esi);
		R=(Ef-Esec)/(Esec-Ei);
		double check=pow(fabs(esf-esi),R);
	

		if (check==0.0 || check>1.797e308 || check<-1.797e308  || Esec==Ei)	{
			A=1.0e-300;
		}
		else	{
			A=(Esec-Ei)/pow(fabs(esf-esi),R);
			if (A>1.797e308 || A<-1.797e308)	{
				A=1.0e300;
			}
		}
	}

	void ConcreteCM::fcEturf(double es, double esi, double fi, double esf, double ff, double Ei, double Ef, double A, double R)
	{
		double Esec=(ff-fi)/(esf-esi);

		if (A==1.0e300 || A==0.0)	{
			fc=fi+Esec*(es-esi);
			Et=Esec;
		}
		else if (pow(fabs(es-esi),-R)==0.0 || pow(fabs(es-esi),-R)>1.797e308 || pow(fabs(es-esi),-R)<-1.797e308)	{
			fc=fi+Esec*(es-esi);
			Et=Esec;
		}
		else if (Ei>=Esec && Ef>=Esec)	{
			fc=fi+Esec*(es-esi);
			Et=Esec;
		}
		else if (Ei<=Esec && Ef<=Esec)	{
			fc=fi+Esec*(es-esi);
			Et=Esec;
		}
		else	{
			fc=fi+(es-esi)*(Ei+A*pow(fabs(es-esi),R));
			Et=Ei+A*(R+1)*pow(fabs(es-esi),R);
			if (Et>1.797e308 || Et<-1.797e308)	{
				fc=fi+Esec*(es-esi);
				Et=Esec;
			}
		}
	}

	double ConcreteCM::getStress ()
	{
		return Tstress;
	}

	double ConcreteCM::getStrain ()
	{
		return Tstrain;
	}

	double ConcreteCM::getTangent ()
	{
		return Ttangent;
	}

	int ConcreteCM::commitState ()
	{
		// History variables
		Ceunn=Teunn;
		Cfunn=Tfunn;
		Ceunp=Teunp;
		Cfunp=Tfunp;
		Cer=Ter;
		Cfr=Tfr;
		Cer0n=Ter0n;
		Cfr0n=Tfr0n;
		Cer0p=Ter0p;
		Cfr0p=Tfr0p;
		Ce0=Te0;
		Cea=Tea;
		Ceb=Teb;
		Ced=Ted;
		Cinc=Tinc;
		Crule=Trule;

		// State variables
		Cstrain = Tstrain;
		Cstress = Tstress;
		Ctangent = Ttangent;	

		return 0;
	}

	double ConcreteCM::getCommittedStrain() // KK
    {
		return Cstrain;
    }

	double ConcreteCM::getCommittedStress() // KK
    {
		return Cstress;
    }

	double ConcreteCM::getCommittedCyclicCrackingStrain() // KK
    {
		return Ceunp;
    }

	int ConcreteCM::revertToLastCommit ()
	{
		// Reset trial history variables to last committed state
		Teunn=Ceunn;
		Tfunn=Cfunn;
		Teunp=Ceunp;
		Tfunp=Cfunp;
		Ter=Cer;
		Tfr=Cfr;
		Ter0n=Cer0n;
		Tfr0n=Cfr0n;
		Ter0p=Cer0p;
		Tfr0p=Cfr0p;
		Te0=Ce0;
		Tea=Cea;
		Teb=Ceb;
		Ted=Ced;
		Tinc=Cinc;         
		Trule=Crule;

		// Recompute trial stress and tangent
		Tstrain = Cstrain;
		Tstress = Cstress;
		Ttangent = Ctangent;

		return 0;
	}


	int ConcreteCM::revertToStart ()
	{
		// Initial tangent
		double Ec0 = Ec;

		// History variables
		Ceunn=0.0;
		Cfunn=0.0;
		Ceunp=0.0;
		Cfunp=0.0;
		Cer=0.0;
		Cfr=0.0;
		Cer0n=0.0;
		Cfr0n=0.0;
		Cer0p=0.0;
		Cfr0p=0.0;
		Ce0=0.0;
		Cea=0.0;
		Ceb=0.0;
		Ced=0.0;
		Cinc=0.0;
		Crule=0.0;

		// State variables
		Cstrain = 0.0;
		Cstress = 0.0;
		Ctangent = Ec0;   
			
		// Reset trial variables and state
		this->revertToLastCommit();

		return 0;
	}
	
	UniaxialMaterial* ConcreteCM::getCopy ()
	{

		ConcreteCM* theCopy = new ConcreteCM(this->getTag());
		
		// Input variables
		theCopy->fpcc = fpcc;
		theCopy->epcc = epcc;
		theCopy->Ec = Ec;
		theCopy->rc = rc;
		theCopy->xcrn = xcrn;
		theCopy->ft = ft;
		theCopy->et = et;
		theCopy->rt = rt;
		theCopy->xcrp = xcrp;
		theCopy->mon = mon;
		theCopy->Gap = Gap;
		
		// Converged history variables
		theCopy-> Ceunn=Ceunn;
		theCopy-> Cfunn=Cfunn;
		theCopy-> Ceunp=Ceunp;
		theCopy-> Cfunp=Cfunp;
		theCopy-> Cer=Cer;
		theCopy-> Cfr=Cfr;
		theCopy-> Cer0n=Cer0n;
		theCopy-> Cfr0n=Cfr0n;
		theCopy-> Cer0p=Cer0p;
		theCopy-> Cfr0p=Cfr0p;
		theCopy-> Ce0=Ce0;
		theCopy-> Cea=Cea;
		theCopy-> Ceb=Ceb;
		theCopy-> Ced=Ced;
		theCopy-> Cinc=Cinc;
		theCopy-> Crule=Crule;

		// Converged state variables
		theCopy->Cstrain = Cstrain;
		theCopy->Cstress = Cstress;
		theCopy->Ctangent = Ctangent;

		return theCopy;
	}

	int ConcreteCM::sendSelf (int commitTag, Channel& theChannel)
	{
		int res = 0;
		static Vector data(31);
		data(0) = this->getTag();

		// Material properties
		data(1) = fpcc;
		data(2) = epcc;
		data(3) = Ec;
		data(4) = rc;
		data(5) = xcrn;
		data(6) = ft;
		data(7) = et;
		data(8) = rt;
		data(9) = xcrp;
		data(10) = mon;
		data(11) = Gap;

		// History variables from last converged state
		data(12) = Ceunn;
		data(13) = Cfunn;
		data(14) = Ceunp;
		data(15) = Cfunp;
		data(16) = Cer;
		data(17) = Cfr;
		data(18) = Cer0n;
		data(19) = Cfr0n;
		data(20) = Cer0p;
		data(21) = Cfr0p;
		data(22) = Ce0;
		data(23) = Cea;
		data(24) = Ceb;
		data(25) = Ced;
		data(26) = Cinc;
		data(27) = Crule;

		// State variables from last converged state
		data(28) = Cstrain;
		data(29) = Cstress;
		data(30) = Ctangent;

		// Data is only sent after convergence, so no trial variables
		// need to be sent through data vector

		res = theChannel.sendVector(this->getDbTag(), commitTag, data);
		if (res < 0) 
			opserr << "ConcreteCM::sendSelf() - failed to send data\n";

		return res;
	}

	int ConcreteCM::recvSelf (int commitTag, Channel& theChannel,
		FEM_ObjectBroker& theBroker)
	{
		int res = 0;
		static Vector data(31);
		res = theChannel.recvVector(this->getDbTag(), commitTag, data);

		if (res < 0) {
			opserr << "ConcreteCM::recvSelf() - failed to receive data\n";
			this->setTag(0);      
		}
		else {
			this->setTag(int(data(0)));

			// Material properties 
			fpcc = data(1);
			epcc = data(2);
			Ec = data(3); 
			rc = data(4);
			xcrn = data(5);
			ft = data(6);
			et = data(7);
			rt = data(8);
			xcrp = data(9);
			mon = data(10);
			Gap = data(11);

			// History variables from last converged state
			Ceunn = data(12);
			Cfunn = data(13);
			Ceunp = data(14);
			Cfunp = data(15);
			Cer = data(16);
			Cfr = data(17);
			Cer0n = data(18);
			Cfr0n = data(19);
			Cer0p = data(20);
			Cfr0p = data(21);
			Ce0 = data(22);
			Cea = data(23);
			Ceb = data(24);
			Ced = data(25);
			Cinc = data(26);
			Crule = data(27);

			// State variables from last converged state
			Cstrain = data(28);
			Cstress = data(29);
			Ctangent = data(30);

			// Set trial state variables
			Tstrain = Cstrain;
			Tstress = Cstress;
			Ttangent = Ctangent;
		}

		return res;		//come back to what this means
	}

	// KK
	Response* 
	ConcreteCM::setResponse(const char **argv, int argc,
				 OPS_Stream &theOutput)
	{
	Response *theResponse = 0;

	if (strcmp(argv[0],"getCommittedConcreteStrain") == 0) {
		double data = 0.0;
		theResponse = new MaterialResponse(this, 100, data);
	} else if (strcmp(argv[0],"getCommittedConcreteStress") == 0) {
		double data1 = 0.0;
		theResponse = new MaterialResponse(this, 101, data1);
	} else if (strcmp(argv[0],"getCommittedCyclicCrackingConcreteStrain") == 0) {
		double data2 = 0.0;
		theResponse = new MaterialResponse(this, 102, data2);
	} else if (strcmp(argv[0],"getInputParameters") == 0) {
		Vector data3(11);
		data3.Zero();
		theResponse = new MaterialResponse(this, 103, data3);
	} else
		return this->UniaxialMaterial::setResponse(argv, argc, theOutput);

	return theResponse;
	}
 
	// KK
	int 
	ConcreteCM::getResponse(int responseID, Information &matInfo)
	{
	if (responseID == 100) {
		matInfo.theDouble = this->getCommittedStrain();

	} else if (responseID == 101){
		matInfo.theDouble = this->getCommittedStress();
	
	} else if (responseID == 102){
		matInfo.theDouble = this->getCommittedCyclicCrackingStrain();
	
	} else if (responseID == 103){
		matInfo.setVector(this->getInputParameters()); 

	} else

		return this->UniaxialMaterial::getResponse(responseID, matInfo);

	return 0;
	}

	void ConcreteCM::Print (OPS_Stream& s, int flag)
	{
		s << "ConcreteCM:(strain, stress, tangent) " << Cstrain << " " << Cstress << " " << Ctangent << endln;
	}


	// AddingSensitivity:BEGIN ///////////////////////////////////
	int
		ConcreteCM::setParameter(const char **argv, int argc, Information &info)
	{
		return -1;
	}


	int
		ConcreteCM::updateParameter(int parameterID, Information &info)
	{
		return 0;
	}


	int
		ConcreteCM::activateParameter(int passedParameterID)
	{
		return 0;
	}

	double
		ConcreteCM::getStressSensitivity(int gradNumber, bool conditional)
	{
		return 0;
	}


	int
		ConcreteCM::commitSensitivity(double TstrainSensitivity, int gradNumber, int numGrads)
	{
		return 0;
	}
	// AddingSensitivity:END /////////////////////////////////////////////

	Vector ConcreteCM::getInputParameters(void)
	{
		Vector input_par(11); // size = max number of parameters (assigned + default)

		input_par.Zero();

		input_par(0) = this->getTag(); 
		input_par(1) = fpcc; 
		input_par(2) = epcc; 
		input_par(3) = Ec; 
		input_par(4) = rc; 
		input_par(5) = xcrn; 
		input_par(6) = ft; 
		input_par(7) = et; 
		input_par(8) = rt; 
		input_par(9) = xcrp;
		input_par(10) = Gap;

		return input_par;
	}
