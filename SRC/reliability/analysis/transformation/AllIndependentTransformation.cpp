/* ****************************************************************** **
**    OpenSees - Open System for Earthquake Engineering Simulation    **
**          Pacific Earthquake Engineering Research Center            **
**                                                                    **
**                                                                    **
** (C) Copyright 2001, The Regents of the University of California    **
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
** Reliability module developed by:                                   **
**   Terje Haukaas (haukaas@ce.berkeley.edu)                          **
**   Armen Der Kiureghian (adk@ce.berkeley.edu)                       **
**                                                                    **
** ****************************************************************** */
                                                                        
// $Revision: 1.1 $
// $Date: 2008-02-29 19:43:52 $
// $Source: /usr/local/cvs/OpenSees/SRC/reliability/analysis/telm/AllIndependentTransformation.cpp,v $

#include <ProbabilityTransformation.h>
#include <AllIndependentTransformation.h>
#include <RandomVariable.h>
#include <CorrelationCoefficient.h>
#include <NormalRV.h>
#include <Vector.h>
#include <Matrix.h>
#include <MatrixOperations.h>
#include <math.h>
#include <string.h>


AllIndependentTransformation::AllIndependentTransformation(ReliabilityDomain *passedReliabilityDomain,
											 int passedPrintFlag)
:ProbabilityTransformation()
{
	theReliabilityDomain = passedReliabilityDomain;
	printFlag = passedPrintFlag;


	// Find and set problem size (number of random variables)
	nrv = theReliabilityDomain->getNumberOfRandomVariables();

	x=0;
	xtemp=0;
	u=0;
	z=0;
	jacobian_x_u=0;
	jacobian_u_x=0;
	jacobian_z_x=0;
	lowerCholesky=0;
	inverseLowerCholesky = 0;
	correlationMatrix=0;
	DzDmean=0;
	DzDstdv=0;

	// Create/initialize vectors and matrices
	x = new Vector(nrv);
	xtemp  = new Vector(nrv);
	u = new Vector(nrv);
	z = new Vector(nrv);
	jacobian_x_u = new Matrix(nrv,nrv);
	jacobian_u_x = new Matrix(nrv,nrv);
	jacobian_z_x = new Matrix(nrv,nrv);
	lowerCholesky = new Matrix(nrv,nrv);
	inverseLowerCholesky = new Matrix(nrv,nrv);
	correlationMatrix = new Matrix(nrv,nrv);

	for(int i=0; i<nrv; i++) (*correlationMatrix)(i,i)=1.0;
	for(int i=0; i<nrv; i++) (*lowerCholesky)(i,i)=1.0;
	for(int i=0; i<nrv; i++) (*inverseLowerCholesky)(i,i)=1.0;

}

AllIndependentTransformation::~AllIndependentTransformation()
{
	if (correlationMatrix != 0){delete correlationMatrix;correlationMatrix=0;}
	if (lowerCholesky != 0){delete lowerCholesky;lowerCholesky=0;}
	if (inverseLowerCholesky != 0){delete inverseLowerCholesky;inverseLowerCholesky=0;}
	if (jacobian_x_u != 0){delete jacobian_x_u;jacobian_x_u=0;}
	if (jacobian_u_x != 0){delete jacobian_u_x;jacobian_u_x=0;}
	if (jacobian_z_x != 0){delete jacobian_z_x;jacobian_z_x=0;}
	if (x != 0){delete x;x=0;}
	if (xtemp != 0){delete xtemp;xtemp=0;}
	if (u != 0){delete u;u=0;}
	if (z != 0){delete z;z=0;}
	if( DzDmean  != 0 ){delete DzDmean ;DzDmean=0;}
	if( DzDstdv  != 0 ){delete DzDstdv ;DzDstdv=0;}
}




int 
AllIndependentTransformation::transform_x_to_u(const Vector &x, Vector &u)
{
//	opserr << " input something112 \n";
	u = x_to_z(x);
	return 0;
}

Vector
AllIndependentTransformation::x_to_z(const Vector &px)
{
	RandomVariable *theRV;
	static NormalRV aStandardNormalRV(1, 0.0, 1.0);
//	opserr << " input something113 \n";
//	Vector z(nrv);
//	opserr << " input something114 \n";
	for ( int i=0 ; i<nrv ; i++ )
	{
		theRV = theReliabilityDomain->getRandomVariablePtr(i+1);
		if (strcmp(theRV->getType(),"NORMAL")==0) {
			double mju = theRV->getMean();
			double sigma = theRV->getStdv();
			(*z)(i) =   ( px(i) - mju ) / sigma;
		}
		else if (strcmp(theRV->getType(),"LOGNORMAL")==0) {
			Vector paramTemp = theRV->getParameters();
            double lambda = paramTemp(0);
			double zeta = paramTemp(1);
			if (zeta < 0.0) { /// interpret this as a negative lognormal random variable
				(*z)(i) = -( log ( fabs(px(i)) ) - lambda ) / zeta;
			}
			else {
				(*z)(i) = ( log ( px(i) ) - lambda ) / zeta;
			}
		}
		else {
			(*z)(i) = aStandardNormalRV.getInverseCDFvalue(theRV->getCDFvalue(px(i)));
		}
	}
//	opserr << " input something114 \n";
	return (*z);
}



Vector 
AllIndependentTransformation::meanSensitivityOf_x_to_u(const Vector &px, int rvNumber)
{
//	Vector z = x_to_z(px);
	(*z) = x_to_z(px);

	if(DzDmean != 0 ){
		delete DzDmean;
		DzDmean = 0;
	}
	DzDmean = new Vector(nrv);
//	Vector DzDmean(x.Size());
	static NormalRV aStandardNormalRV(1,0.0,1.0); 
	RandomVariable *theRV = theReliabilityDomain->getRandomVariablePtr(rvNumber);
	if (strcmp(theRV->getType(),"NORMAL")==0) {
		double sigma = theRV->getStdv();
		(*DzDmean)(rvNumber-1) = -1.0 / sigma;
	}
	else if (strcmp(theRV->getType(),"LOGNORMAL")==0) {
		Vector paramTemp = theRV->getParameters();
		double mean = fabs(theRV->getMean()); // more here for negative lognormal?
		double stdv = theRV->getStdv();

		double a = mean*mean+stdv*stdv;
		(*DzDmean)(rvNumber-1) = 0.5*(-2.0*mean*mean*log(a)+4.0*mean*mean*log(mean)
			-3.0*stdv*stdv*log(a)+4.0*stdv*stdv*log(mean)
			+2.0*stdv*stdv*log(fabs(px(rvNumber-1))))
			/(pow(log(a)-2.0*log(mean),1.5)*mean*a);
	}
	else if (strcmp(theRV->getType(),"UNIFORM")==0) {
		double pz = 0.39894228048*exp(-(*z)(rvNumber-1)*(*z)(rvNumber-1)/2.0);
        Vector paramTemp = theRV->getParameters();
        double a = paramTemp(0);
        double b = paramTemp(1);
		(*DzDmean)(rvNumber-1) = -1.0/(pz*(b-a));
	}
	else {
		opserr << "WARNING: Cannot compute reliability sensitivity results for " << endln
			<< " type of random variable number " << rvNumber << endln;
		(*DzDmean)(rvNumber-1) = 0.0; //aStandardNormalRV.getInverseCDFvalue(theRV->getCDFMeanSensitivity(x(rvNumber-1)));
	}

	// Return the final result (the four factors)
	return (*DzDmean);
}


Vector 
AllIndependentTransformation::stdvSensitivityOf_x_to_u(const Vector &px, int rvNumber)
{
//	Vector z = x_to_z(x);
	(*z) = x_to_z(px);

	// 3) DzDmean and DzDstdv = a vector of zeros and then:
	if(DzDstdv != 0 ){
		delete DzDstdv;
		DzDstdv = 0;
	}
	DzDstdv = new Vector(nrv);
//	Vector DzDstdv(x.Size());
	static NormalRV aStandardNormalRV(1,0.0,1.0); 
	RandomVariable *theRV = theReliabilityDomain->getRandomVariablePtr(rvNumber);
	if (strcmp(theRV->getType(),"NORMAL")==0) {
		double mu = theRV->getMean();
		double sigma = theRV->getStdv();
		(*DzDstdv)(rvNumber-1) = - (px(rvNumber-1)-mu) / (sigma*sigma);
	}
	else if (strcmp(theRV->getType(),"LOGNORMAL")==0) {
		double mean = fabs(theRV->getMean()); // more here for negative lognormal?
		double stdv = theRV->getStdv();

		double a = mean*mean+stdv*stdv;
		(*DzDstdv)(rvNumber-1) = 0.5*stdv*(log(a)-2.0*log(fabs(px(rvNumber-1))))
			/(pow(log(a)-2.0*log(mean),1.5)*a);
		
//		double z = ( log ( fabs(x(rvNumber-1)) ) - lambda ) / zeta; // more here for negative lognormal?
//		double e = (stdv/mean)*(stdv/mean);
//		double d = 1 + e;
//		double f = 1.0 / (mean*d*zeta);
//		DzDstdv(rvNumber-1) = stdv*((1.0-z/zeta)*f/mean);
	}
	else if (strcmp(theRV->getType(),"UNIFORM")==0) {
		double pz = 0.39894228048*exp(-(*z)(rvNumber-1)*(*z)(rvNumber-1)/2.0);
		Vector paramTemp = theRV->getParameters();
        double a = paramTemp(0);
        double b = paramTemp(1);
		double DzDmean = -1.0/(pz*(b-a));
		double e = -DzDmean/(b-a);
		(*DzDstdv)(rvNumber-1) = 1.732050807*(a+b-2.0*(*x)(rvNumber-1))*e;
	}
	else {
		opserr << "WARNING: Cannot compute reliability sensitivity results for " << endln
			<< " type of random variable number " << rvNumber << endln;
		(*DzDstdv)(rvNumber-1) = 0.0; 
        //aStandardNormalRV.getInverseCDFvalue(theRV->getCDFStdvSensitivity(x(rvNumber-1)));
	}
	// Return the final result (the four factors)
	return *DzDstdv;
}


int 
AllIndependentTransformation::transform_u_to_x(const Vector &u, Vector &x)
{
	x = z_to_x(u);
	return 0;
}

/*
int 
AllIndependentTransformation::transform_u_to_x_andComputeJacobian()
{
	(*x) = z_to_x(*u);


	(*jacobian_u_x) = getJacobian_z_x((*x),(*u));

    for(int i=0; i<nrv; i++) 
	(*jacobian_x_u)(i,i) = 1.0/(*jacobian_u_x)(i,i);

	return 0;
}
*/

int
AllIndependentTransformation::getJacobian_x_to_u(const Vector &x, Matrix &Jxu)
{
	(*jacobian_u_x) = getJacobian_z_x(x,(*u));

    for(int i=0; i<nrv; i++) 
	(*jacobian_x_u)(i,i) = 1.0/(*jacobian_u_x)(i,i);

	return 0;
}



int
AllIndependentTransformation::getJacobian_u_to_x(const Vector &pu, Matrix &Jux)
{
	*x = z_to_x(pu);

	(*jacobian_u_x) = getJacobian_z_x((*x),*u);

	return 0;
}

Matrix
AllIndependentTransformation::getJacobian_z_x(const Vector &px, const Vector &pz)
{	
	RandomVariable *theRV;
	static NormalRV aStandardNormalRV(1, 0.0, 1.0);
//	Matrix jacobianMatrix_z_x(nrv,nrv);
	for ( int i=0 ; i<nrv ; i++ )
	{
		theRV = theReliabilityDomain->getRandomVariablePtr(i+1);
		if (strcmp(theRV->getType(),"NORMAL")==0) {
			double sigma = theRV->getStdv();
			(*jacobian_z_x)(i,i) = 1.0 / sigma;
		}
		else if (strcmp(theRV->getType(),"LOGNORMAL")==0) {
            Vector paramTemp = theRV->getParameters();
            double zeta = fabs(paramTemp(1));
			if (px(i) == 0.0) {
				opserr << "NatafProbabilityTransformation::getJacobian_z_x() -- Error: value " << endln
				    << "of lognormal random variable is zero in original space. " << endln;
			}
			(*jacobian_z_x)(i,i) = 1.0 / ( zeta * px(i)  );
		}
		else {
			double pdf = aStandardNormalRV.getPDFvalue(pz(i));
			if (pdf == 0.0) {
				opserr << "ERROR: NatafProbabilityTransformation::getJacobian_z_x() -- " << endln
					<< " the PDF value is zero, probably due to too large step. " << endln;
			}
			(*jacobian_z_x)(i,i) = theRV->getPDFvalue(px(i)) / pdf;
			if ((*jacobian_z_x)(i,i)==0.0) {
			}
		}
	}

	return *jacobian_z_x;
}




Vector
AllIndependentTransformation::z_to_x(const Vector &pz)
{
	RandomVariable *theRV;
	static NormalRV aStandardNormalRV(1, 0.0, 1.0);
//	Vector x(nrv);
	for ( int i=0 ; i<nrv ; i++ )
	{
		theRV = theReliabilityDomain->getRandomVariablePtr(i+1);
		if (strcmp(theRV->getType(),"NORMAL")==0) {
			double mju = theRV->getMean();
			double sigma = theRV->getStdv();
			(*xtemp)(i) = pz(i) * sigma + mju;
		}
		else if (strcmp(theRV->getType(),"LOGNORMAL")==0) {
            Vector paramTemp = theRV->getParameters();
            double lambda = paramTemp(0);
			double zeta = paramTemp(1);
			if (zeta < 0.0) { // interpret this as negative lognormal random variable
				(*xtemp)(i) = -exp ( -pz(i) * zeta + lambda );
			}
			else {
				(*xtemp)(i) = exp ( pz(i) * zeta + lambda );
			}
		}
		else {
			(*xtemp)(i) = theRV->getInverseCDFvalue(  aStandardNormalRV.getCDFvalue(pz(i))  );
		}
	}
	return *xtemp;
}

void AllIndependentTransformation::setReliabilityDomain(ReliabilityDomain* theRelDom)
{
	theReliabilityDomain = theRelDom;
	nrv = theReliabilityDomain->getNumberOfRandomVariables();

	if( x != 0 ){
		delete x;
		x=0;
	}
	if( xtemp != 0 ){
		delete xtemp;
		xtemp=0;
	}
	if( u != 0 ){
		delete u;
		u=0;
	}
	if( z != 0 ){
		delete z;
		z=0;
	}
	if( jacobian_x_u  != 0 ){
		delete jacobian_x_u;
		jacobian_x_u = 0;
	}
	if( jacobian_u_x  != 0 ){
		delete jacobian_u_x;
		jacobian_u_x = 0;
	}
	if( jacobian_z_x  != 0 ){
		delete jacobian_z_x;
		jacobian_z_x = 0;
	}
	if( lowerCholesky  != 0 ){
		delete lowerCholesky;
		lowerCholesky = 0;
	}
	if( inverseLowerCholesky  != 0 ){
		delete inverseLowerCholesky ;
		inverseLowerCholesky  = 0;
	}
	if( correlationMatrix  != 0 ){
		delete correlationMatrix ;
		correlationMatrix  = 0;
	}
	if( DzDmean  != 0 ){
		delete DzDmean ;
		DzDmean  = 0;
	}
	if( DzDstdv  != 0 ){
		delete DzDstdv ;
		DzDstdv  = 0;
	}

	// Create/initialize vectors and matrices
	x = new Vector(nrv);
	xtemp = new Vector(nrv);
	u = new Vector(nrv);
	z = new Vector(nrv);
	jacobian_x_u = new Matrix(nrv,nrv);
	jacobian_u_x = new Matrix(nrv,nrv);
	jacobian_z_x = new Matrix(nrv,nrv);
	lowerCholesky = new Matrix(nrv,nrv);
	inverseLowerCholesky = new Matrix(nrv,nrv);
	correlationMatrix = new Matrix(nrv,nrv);

	for(int i=0; i<nrv; i++) (*correlationMatrix)(i,i)=1.0;
	for(int i=0; i<nrv; i++) (*lowerCholesky)(i,i)=1.0;
	for(int i=0; i<nrv; i++) (*inverseLowerCholesky)(i,i)=1.0;

}



