#pragma once
/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Paul
 * DATE     : Dec 2012
-*/

#include "gendefs.h"
#include "attribprovider.h"
#include "arraynd.h"
#include "mathfunc.h"
#include "commondefs.h"

/*!%CEEMD Attributes

Input:
0		Data

Outputs:
0		CEEMD attributes
*/

namespace Attrib
{

class CEEMD: public Provider
{
public:
    static void		initClass();
			CEEMD(Desc&);

    static const char*	attribName()		{ return "CEEMD"; }
    static const char*	stopimfStr()		{ return "stopimf"; }
    static const char*	stopsiftStr()		{ return "stopsift"; }
    static const char*	maxnrimfStr()		{ return "maxnrimf"; }
    static const char*	maxsiftStr()		{ return "maxsift"; }
    static const char*	emdmethodStr()		{ return "method"; }
    static const char*	attriboutputStr()	{ return "attriboutput"; }
    static const char*	symmetricboundaryStr()	{ return "symmetricboundary"; }
    static const char*	noisepercentageStr()	{ return "noisepercentage"; }
    static const char*	maxnoiseloopStr()	{ return "maxnoiseloop"; }
    static const char*	minstackcountStr()	{ return "minstackcount"; }
    static const char*	outputfreqStr()		{ return "outputfreq"; }
    static const char*	stepoutfreqStr()	{ return "stepoutfreq"; }
    static const char*	outputcompStr()		{ return "outputcomp"; }
    static const char*	usetfpanelStr()		{ return "usetfpanel"; }
    static const char*	transMethodNamesStr(int);
    static const char*	transOutputNamesStr(int);
    void		getCompNames( BufferStringSet& nms ) const;
    bool		prepPriorToOutputSetup();

protected:
			~CEEMD() {}
    static Provider*	createInstance(Desc&);
    static void		updateDefaults(Desc&)	{};
    static void		updateDesc(Desc&);

    bool		allowParallelComputation() const;
    bool		areAllOutputsEnabled() const;
    void		getFirstAndLastOutEnabled(int& first, int& last) const;
    int			maxnrimf_; // Maximum number of intrinsic Mode Functions
    int			maxsift_; // Maximum number of sifting iterations
    float		outputfreq_; // Output frequency
    float		stepoutfreq_; // Step output frequency
    int			outputcomp_; // Output frequency
    float		stopsift_; // stop sifting if st.dev res.-imf < value
    float		stopimf_; // stop decomp. when st.dev imf < value
    float		noisepercentage_; // noise percentage for EEMD and CEEMD
			// boundary extension symmetric or periodic
    bool		symmetricboundary_;
			// use synthetic trace in ceemdtestprogram.h
    bool		usetestdata_;
    bool		usetfpanel_; // if panel is pressed use 0 to Nyquist
    bool		getInputOutput(int input,TypeSet<int>& res) const;
    bool		getInputData(const BinID&,int zintv);
    bool		computeData(const DataHolder&,const BinID& relpos,
			    int z0,int nrsamples,int threadid) const;
    const Interval<float>* reqZMargin(int input,int output) const
			   {return &gate_;}
    const Interval<int>* desZSampMargin(int input,int output) const
			 { return &dessampgate_; }

    Interval<float>	gate_;
    Interval<int>	dessampgate_;
    int			dataidx_;
    int			method_;
    int			attriboutput_;
    int			maxnoiseloop_;
    const DataHolder*	inputdata_;
};

}; // namespace Attrib

