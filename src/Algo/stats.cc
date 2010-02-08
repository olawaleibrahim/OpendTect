/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Bert Bril
 * DATE     : Sep 2006
-*/

static const char* rcsID = "$Id: stats.cc,v 1.10 2010-02-08 15:33:22 cvsbert Exp $";

#include "statruncalc.h"
#include "statrand.h"
#include "errh.h"
#include "timefun.h"
#include "envvars.h"
#include "settings.h"

DefineNameSpaceEnumNames(Stats,Type,0,"Statistics types")
{
	"Count",
	"Average", "Median", "RMS",
	"StdDev", "Variance", "NormVariance",
	"Min", "Max", "Extreme",
	"Sum", "SquareSum",
	"MostFrequent",
	0
};

int Stats::RandGen::seed_ = 0;

#include <math.h>
#include <stdlib.h>


Stats::RunCalcSetup& Stats::RunCalcSetup::require( Stats::Type t )
{
    if ( t == Stats::Median )
	{ needmed_ = true; return *this; }
    else if ( t == Stats::MostFreq )
	{ needmostfreq_ = true; return *this; }
    else if ( t >= Stats::Min && t <= Stats::Extreme )
	{ needextreme_ = true; return *this; }

    needsums_ = true;
    return *this;
}


int Stats::RunCalcSetup::medianEvenHandling()
{
    static int ret = -2;
    if ( ret != -2 ) return ret;

    if ( GetEnvVarYN("OD_EVEN_MEDIAN_AVERAGE") )
	ret = 0;
    else if ( GetEnvVarYN("OD_EVEN_MEDIAN_LOWMID") )
	ret = -1;
    else
    {
	bool yn = false;
	Settings::common().getYN( "dTect.Even Median.Average", yn );
	if ( yn )
	    ret = 0;
	else
	{
	    Settings::common().getYN( "dTect.Even Median.LowMid", yn );
	    ret = yn ? -1 : 1;
	}
    }

    return ret;
}


double Stats::RandGen::get()
{
#ifdef __win__
    static const int rmax_ = RAND_MAX;
    double ret = rand();
    ret /= rmax_;
    return ret;
#else
    return drand48();
#endif
}


void Stats::RandGen::init( int seed )
{
    if ( seed == 0 )
    {
	if ( seed_ != 0 )
	    return;
	seed = (int)Time_getMilliSeconds();
    }
    seed_ = seed;

#ifdef __win__
    srand( (unsigned)seed_ );
#else
    srand48( (long)seed_ );
#endif
}


double Stats::RandGen::getNormal( double ex, double sd )
{
    static int iset = 0;
    static double gset = 0;
    double fac, r, v1, v2;
    double arg;

    double retval = gset;
    if ( !iset )
    {
        do 
	{
            v1 = 2.0*get() - 1.0;
            v2 = 2.0*get() - 1.0;
            r=v1*v1+v2*v2;
        } while (r >= 1.0 || r <= 0.0);

	arg = (double)(-2.0*log(r)/r);
        fac = Math::Sqrt( arg );
        gset = v1 * fac;
        retval = v2 * fac;
    }

    iset = !iset;
    return retval*sd + ex;
}


int Stats::RandGen::getIndex( int sz )
{
    if ( sz < 2 ) return 0;

    int idx = (int)(sz * get());
    if ( idx < 0 ) idx = 0;
    if ( idx >= sz ) idx = sz-1;

    return idx;
}


od_int64 Stats::RandGen::getIndex( od_int64 sz )
{
    if ( sz < 2 ) return 0;

    od_int64 idx = (od_int64)(sz * get());
    if ( idx < 0 ) idx = 0;
    if ( idx >= sz ) idx = sz-1;

    return idx;
}
