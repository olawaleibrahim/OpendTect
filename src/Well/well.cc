/*+
 * COPYRIGHT: (C) de Groot-Bril Earth Sciences B.V.
 * AUTHOR   : A.H. Bril
 * DATE     : Aug 2003
-*/

static const char* rcsID = "$Id: well.cc,v 1.1 2003-08-15 11:12:15 bert Exp $";

#include "welldata.h"
#include "welltrack.h"
#include "welllog.h"
#include "welllogset.h"
#include "welld2tmodel.h"
#include "finding.h"


Well::Data::Data( const char* nm )
    	: track_(*new Well::Track)
    	, logs_(*new Well::LogSet)
    	, d2tmodel_(0)
{
}


void Well::Data::setD2TModel( D2TModel* d )
{
    delete d2tmodel_;
    d2tmodel_ = d;
}


Well::LogSet::~LogSet()
{
    deepErase( logs );
}


void Well::LogSet::add( Well::Log* l )
{
    if ( !l ) return;

    logs += l;
    if ( dahintv.start > l->dah(0) ) dahintv.start = l->dah(0);
    if ( dahintv.stop < l->dah(l->nrSamples()-1) )
	dahintv.stop = l->dah(l->nrSamples()-1);
}


Well::Log* Well::LogSet::remove( int idx )
{
    Log* l = logs[idx]; logs -= l;
    ObjectSet<Well::Log> tmp( logs );
    logs.erase(); init();
    for ( int idx=0; idx<tmp.size(); idx++ )
	add( tmp[idx] );
    return l;
}


float Well::Log::getValue( float dh ) const
{
    int idx1;
    if ( findFPPos(dah_,dah_.size(),dh,-1,idx1) )
	return val_[idx1];
    else if ( idx1 < 0 || idx1 == dah_.size()-1 )
	return mUndefValue;

    const int idx2 = idx1 + 1;
    const float d1 = dh - dah_[idx1];
    const float d2 = dah_[idx2] - dh;
    return d1 * val_[idx2] + d2 * val_[idx1] / (d1 + d2);
}


Coord3 Well::Track::getPos( float dh ) const
{
    int idx1;
    if ( findFPPos(dah_,dah_.size(),dh,-1,idx1) )
	return pos_[idx1];
    else if ( idx1 < 0 || idx1 == dah_.size()-1 )
	return Coord3(0,0,0);

    const int idx2 = idx1 + 1;
    const float d1 = dh - dah_[idx1];
    const float d2 = dah_[idx2] - dh;
    const Coord3& c1 = pos_[idx1];
    const Coord3& c2 = pos_[idx2];
    const float f = 1. / (d1 + d2);
    return Coord3( f * (d1 * c2.x + d2 * c1.x), f * (d1 * c2.y + d2 * c1.y),
		   f * (d1 * c2.z + d2 * c1.z) );
}


float Well::D2TModel::getTime( float dh ) const
{
    int idx1;
    if ( findFPPos(dah,dah.size(),dh,-1,idx1) )
	return t[idx1];
    else if ( idx1 < 0 || idx1 == dah.size()-1 )
	return mUndefValue;

    const int idx2 = idx1 + 1;
    const float d1 = dh - dah[idx1];
    const float d2 = dah[idx2] - dh;
    //TODO not time-correct
    return d1 * t[idx2] + d2 * t[idx1] / (d1 + d2);
}
