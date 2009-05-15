/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Bruno
 Date:		Jan 2009
________________________________________________________________________

-*/
static const char* rcsID = "$Id: welltied2tmodelmanager.cc,v 1.2 2009-05-15 12:42:48 cvsbruno Exp $";

#include "welltied2tmodelmanager.h"

#include "ioman.h"
#include "iostrm.h"
#include "filegen.h"
#include "filepath.h"
#include "strmprov.h"
#include "welldata.h"
#include "welllog.h"
#include "wellman.h"
#include "welllogset.h"
#include "wellwriter.h"
#include "welld2tmodel.h"
#include "welltiesetup.h"
#include "welltiegeocalculator.h"

WellTieD2TModelManager::WellTieD2TModelManager( Well::Data& d, 
						const  WellTieSetup& s, 
					        WellTieGeoCalculator& gc )
	: wd_(d)
	, geocalc_(gc)
	, prvd2t_(0)
	, emptyoninit_(false)
	, wtsetup_(s)		     
{
    if ( !wd_.d2TModel() || wd_.d2TModel()->size()<=2 )
    {
	emptyoninit_ = true;
	wd_.setD2TModel( new Well::D2TModel );
    }
    orgd2t_ = new Well::D2TModel( *wd_.d2TModel() );

    if ( wd_.checkShotModel() || wd_.d2TModel()->size()<=2 )
	setFromVelLog();
} 


WellTieD2TModelManager::~WellTieD2TModelManager()
{
    delete prvd2t_;
    delete orgd2t_;
}


Well::D2TModel& WellTieD2TModelManager::d2T()
{
    return *wd_.d2TModel();
}


void WellTieD2TModelManager::setFromVelLog( bool docln )
{setAsCurrent( geocalc_.getModelFromVelLog(docln) );}


void WellTieD2TModelManager::setFromData( const Array1DImpl<float>& time,
					   const Array1DImpl<float>& dpt )
{setAsCurrent( geocalc_.getModelFromVelLogData( time, dpt) );}



void WellTieD2TModelManager::shiftModel( float shift)
{
    TypeSet<float> dah, time;

    Well::D2TModel& d2t = d2T();
    //copy old d2t
    for (int idx = 0; idx<d2t.size(); idx++)
    {
	time += d2t.value( idx );
	dah  += d2t.dah( idx );
    }

    //replace by shifted one
    d2t.erase();
    for ( int dahidx=0; dahidx<dah.size(); dahidx++ )
	d2t.add( dah[dahidx], time[dahidx] + shift );

    if ( d2t.size() > 1 )
    wd_.d2tchanged.trigger();
}



void WellTieD2TModelManager::setAsCurrent( Well::D2TModel* d2t )
{
    if ( !d2t || d2t->size() < 1 )
    { pErrMsg("Bad D2TMdl: ignoring"); delete d2t; return; }

    if ( prvd2t_ )
	delete prvd2t_;
    prvd2t_ =  new Well::D2TModel( d2T() );
    wd_.setD2TModel( d2t );
}


bool WellTieD2TModelManager::undo()
{
    if ( !prvd2t_ ) return false; 
	setAsCurrent( prvd2t_ );
    return true;
}


bool WellTieD2TModelManager::cancel()
{
    if ( emptyoninit_ )
    {
	wd_.d2TModel()->erase();	
	wd_.d2tchanged.trigger();
    }
    else
	setAsCurrent( orgd2t_ );
    return true;
}


bool WellTieD2TModelManager::updateFromWD()
{
    if ( !wd_.d2TModel() || wd_.d2TModel()->size()<1 )
       return false;	
    setAsCurrent( wd_.d2TModel() );
    return true;
}


bool WellTieD2TModelManager::commitToWD()
{
    mDynamicCastGet(const IOStream*,iostrm,IOM().get(wtsetup_.wellid_))
    if ( !iostrm ) 
	return false;
    StreamProvider sp( iostrm->fileName() );
    sp.addPathIfNecessary( iostrm->dirName() );
    BufferString fname = sp.fileName();

    Well::Writer wtr( fname, wd_ );
    if ( !wtr.putD2T() ) 
	return false;

    wd_.d2tchanged.trigger();

    return true;
}


bool WellTieD2TModelManager::save( const char* filename )
{
    StreamData sdo = StreamProvider( filename ).makeOStream();
    if ( !sdo.usable() )
    {
	sdo.close();
	return false;
    }

    const Well::D2TModel& d2t = d2T();
    for ( int idx=0; idx< d2t.size(); idx++ )
    {
	*sdo.ostrm <<  d2t.dah(idx); 
	*sdo.ostrm << '\t';
       	*sdo.ostrm <<  d2t.value(idx);
	*sdo.ostrm << '\n';
    }
    sdo.close();

    return true;
}
