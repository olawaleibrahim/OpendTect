/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : K. Tingdahl
 * DATE     : Oct 1999
-*/

static const char* rcsID mUsedVar = "$Id$";

#include "welllogattrib.h"

#include "attribdataholder.h"
#include "attribdesc.h"
#include "attribfactory.h"
#include "attribparam.h"
#include "survinfo.h"
#include "welldata.h"
#include "wellextractdata.h"
#include "wellman.h"
#include "wellreader.h"

#include <math.h>


namespace Attrib
{

mAttrDefCreateInstance(WellLog)

void WellLog::initClass()
{
    mAttrStartInitClass

    desc->addParam( new StringParam(keyStr()) );
    desc->addParam( new StringParam(logName()) );
    EnumParam* scaletype = new EnumParam( upscaleType() );
    scaletype->setEnums( Stats::UpscaleTypeDef() );
    scaletype->setDefaultValue( (int)Stats::Average );
    desc->addParam( scaletype );

    desc->setNrOutputs( Seis::UnknowData, 1 );
    desc->setLocality( Desc::SingleTrace );
    mAttrEndInitClass
}


WellLog::WellLog( Desc& ds )
    : Provider(ds)
    , logvals_(0)
{
    if ( !isOK() ) return;

    logname_ = desc_.getValParam(logName())->getStringValue( 0 );
    wellid_ = MultiID( desc_.getValParam(keyStr())->getStringValue(0) );
    mGetEnum( upscaletype_, upscaleType() );
}


WellLog::~WellLog()
{
    delete logvals_;
}


bool WellLog::getInputOutput( int input, TypeSet<int>& res ) const
{
    return Provider::getInputOutput( input, res );
}


bool WellLog::getInputData( const BinID& relpos, int zintv )
{
    return true;
}


bool WellLog::allowParallelComputation() const
{ return true; }


void WellLog::prepareForComputeData()
{
    RefMan<Well::Data> wd = new Well::Data;
    if ( Well::MGR().isLoaded(wellid_) )
    {
	wd = Well::MGR().get( wellid_ );
	if ( !wd )
	{
	    errmsg_ = mToUiStringTodo( Well::MGR().errMsg() );
	    return;
	}
    }
    else
    {
	Well::Reader wrdr( wellid_, *wd );
	const bool hastrack = wrdr.getTrack();
	const bool hasd2t = wrdr.getD2T();
	const bool haslog = wrdr.getLog( logname_ );
	if ( !hastrack || ( SI().zIsTime() && !hasd2t ) || !haslog )
	{
	    errmsg_ = uiStrings::phrCannotRead( !hastrack
				    ? uiStrings::sTrack()
				    : !haslog ? uiStrings::sWellLog()
					      : tr("time-depth model") );
	    if ( wd->name() )
		errmsg_.append( " for well ").append( wd->name() );

	    return;
	}
    }

    Well::ExtractParams pars;
    pars.setFixedRange( SI().zRange(true), SI().zDomain().isTime() );
    pars.zstep_ = SI().zStep();
    pars.extractzintime_ = SI().zIsTime();
    pars.snapZRangeToSurvey( !is2D() );
    pars.samppol_ = (Stats::UpscaleType)upscaletype_;

    BufferStringSet lognms; lognms.add( logname_ );
    Well::LogSampler logsamp( *wd, pars, lognms );
    if ( !logsamp.executeParallel(false) )
	return;

    StepInterval<float> zrg = logsamp.zRange();
    zrg.step = SI().zStep();
    arrzrg_ = SI().zRange( false );
    const int arrsz = arrzrg_.nrSteps()+1;
    logvals_ = new Array1DImpl<float>( arrsz );

    for ( int idx=0; idx<arrsz; idx++ )
    {
	const float depth = arrzrg_.atIndex( idx );
	float val = mUdf(float);
	if ( zrg.includes(depth,true) )
	{
	    const int idz = zrg.getIndex( depth );
	    if ( idz >=0 && idz < logsamp.nrZSamples() )
		val = logsamp.getLogVal( 0, idz );
	}

	logvals_->set( idx, val );
    }
}


bool WellLog::computeData( const DataHolder& output, const BinID& relpos,
			   int z0, int nrsamples, int threadid ) const
{
    if ( output.isEmpty() || !logvals_ )
	return false;

    const float step = !mIsZero(refstep_,mDefEps) ? refstep_ : SI().zStep();
    for ( int idx=0; idx<nrsamples; idx++ )
    {
	const float realz = (z0+idx)*step;
	const int arridx = arrzrg_.getIndex( realz );
	setOutputValue( output, 0, idx, z0, logvals_->get(arridx) );
    }

    return true;
}

} // namespace Attrib
