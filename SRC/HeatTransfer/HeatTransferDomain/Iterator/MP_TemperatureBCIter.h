/* ****************************************************************** **
**    OpenSees - Open System for Earthquake Engineering Simulation    **
**          Pacific Earthquake Engineering Research Center            **
**                                                                    **
**                                                                    **
** (C) Copyright 1999, The Regents of the University of California    **
** All Rights Reserved.                                               **
**                                                                    **
** Commercial use of this program without express permission of the   **
** University of California, Berkeley, is strictly prohibited.  See   **
** file 'COPYRIGHT'  in main directory for information on usage and   **
** redistribution,  and for a DISCLAIMER OF ALL WARRANTIES.           **
**                                                                    **
** Developed by:                                                      **
**   Frank McKenna (fmckenna@ce.berkeley.edu)                         **
**   Gregory L. Fenves (fenves@ce.berkeley.edu)                       **
**   Filip C. Filippou (filippou@ce.berkeley.edu)                     **
**                                                                    **
** ****************************************************************** */
                                                                        
// $Revision: 1.1.1.1 $
// $Date: 2000-09-15 08:23:18 $
// $Source: /usr/local/cvs/OpenSees/SRC/domain/domain/MP_TemperatureBCIter.h,v $
                                                                        
                                                                        
// File: ~/domain/domain/MP_TemperatureBCIter.h
//
// Written: fmk 
// Created: Fri Sep 20 15:27:47: 1996
// Revision: A
//
// Description: This file contains the class definition for 
// MP_TemperatureBCIter. MP_TemperatureBCIter is an abstract base class.
// An MP_TemperatureBCIter object is an iter for returning the
// multiple point constraints  of an object of class Domain. 
// MP_TemperatureBCIters must be written for each subclass of Domain.

#ifndef MP_TemperatureBCIter_h
#define MP_TemperatureBCIter_h

class MP_TemperatureBC;

class MP_TemperatureBCIter
{
  public:
    MP_TemperatureBCIter() {};
    virtual ~MP_TemperatureBCIter() {};
    
    virtual MP_TemperatureBC* operator()(void) =0;
    
  protected:

  private:

};

#endif

