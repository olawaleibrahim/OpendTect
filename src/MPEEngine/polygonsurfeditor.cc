/*
___________________________________________________________________

 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Yuancheng Liu
 * DATE     : Aug. 2008
___________________________________________________________________

-*/


#include "polygonsurfeditor.h"

#include "empolygonbody.h"
#include "emmanager.h"
#include "polygonsurfaceedit.h"
#include "mpeengine.h"
#include "selector.h"
#include "survinfo.h"
#include "trigonometry.h"
#include "uistrings.h"

#include "undo.h"

namespace MPE
{

PolygonBodyEditor::PolygonBodyEditor( EM::PolygonBody& polygonsurf )
    : ObjectEditor(polygonsurf)
    , sowingpivot_(Coord3::udf())
{}


ObjectEditor* PolygonBodyEditor::create( EM::Object& emobj )
{
    mDynamicCastGet(EM::PolygonBody*,polygonsurf,&emobj);
    return polygonsurf ? new PolygonBodyEditor( *polygonsurf ) : 0;
}


void PolygonBodyEditor::initClass()
{
    MPE::ObjectEditor::factory().addCreator( create,
					     EM::PolygonBody::typeStr() );
}


Geometry::ElementEditor* PolygonBodyEditor::createEditor()
{
    const Geometry::Element* ge = emObject().geometryElement();
    if ( !ge ) return 0;

    mDynamicCastGet(const Geometry::PolygonSurface*,surface,ge);
    return !surface ? 0 : new Geometry::PolygonSurfEditor(
			  *const_cast<Geometry::PolygonSurface*>(surface) );
}


static EM::PosID lastclicked_ = EM::PosID::getInvalid();

void PolygonBodyEditor::setLastClicked( const EM::PosID& pid )
{
    lastclicked_ = pid;

    if ( sowingpivot_.isDefined() )
    {
	const Coord3 pos = emObject().getPos( pid );
	if ( pos.isDefined() )
	    sowinghistory_.insert( 0, pos );
    }
}


void PolygonBodyEditor::setSowingPivot( const Coord3 pos )
{
    if ( sowingpivot_.isDefined() && !pos.isDefined() )
	sowinghistory_.erase();

    sowingpivot_ = pos;
}


#define mCompareCoord( crd ) Coord3( crd.x_, crd.y_, crd.z_*zfactor )

void PolygonBodyEditor::getInteractionInfo( EM::PosID& nearestpid0,
					    EM::PosID& nearestpid1,
					    EM::PosID& insertpid,
					    const Coord3& mousepos,
					    float zfactor ) const
{
    nearestpid0 = EM::PosID::getInvalid();
    nearestpid1 = EM::PosID::getInvalid();
    insertpid = EM::PosID::getInvalid();

    const Coord3& pos = sowingpivot_.isDefined() && sowinghistory_.isEmpty()
			? sowingpivot_ : mousepos;

    int polygon;
    const float mindist = getNearestPolygon( polygon, pos, zfactor );
    if ( mIsUdf(mindist) )
    {
	const Geometry::Element* ge = emObject().geometryElement();
	if ( !ge ) return;

	mDynamicCastGet(const Geometry::PolygonSurface*,surface,ge);
	if ( !surface ) return;

	const StepInterval<int> rowrange = surface->rowRange();
	if ( !rowrange.isUdf() )
	    return;

	insertpid = EM::PosID::getFromRowCol( 0, 0 );
	return;
    }

    if ( fabs(mindist)>50 )
    {
	const Geometry::Element* ge = emObject().geometryElement();
	if ( !ge ) return;

	mDynamicCastGet(const Geometry::PolygonSurface*,surface,ge);
	if ( !surface ) return;

	const StepInterval<int> rowrange = surface->rowRange();
	if ( rowrange.isUdf() )
	    return;

	const int newpolygon = mindist>0
	    ? polygon+rowrange.step
	    : polygon==rowrange.start ? polygon-rowrange.step : polygon;

	insertpid = EM::PosID::getFromRowCol( newpolygon, 0 );
	return;
    }

    getPidsOnPolygon( nearestpid0, nearestpid1, insertpid, polygon,
		      pos, zfactor );
}


bool PolygonBodyEditor::removeSelection( const Selector<Coord3>& selector )
{
    mDynamicCastGet( EM::PolygonBody*, polygonsurf, emobject_.ptr() );
    if ( !polygonsurf )
	return false;

    bool change = false;
    const Geometry::Element* ge = polygonsurf->geometryElement();
    if ( !ge ) return false;

    mDynamicCastGet(const Geometry::PolygonSurface*,surface,ge);
    if ( !surface ) return false;

    const StepInterval<int> rowrange = surface->rowRange();
    if ( rowrange.isUdf() )
	return false;

    for ( int polygonidx=rowrange.nrSteps(); polygonidx>=0; polygonidx-- )
    {
	Coord3 avgpos( 0, 0, 0 );
	const int curpolygon = rowrange.atIndex(polygonidx);
	const StepInterval<int> colrange = surface->colRange( curpolygon );
	if ( colrange.isUdf() )
	    continue;

	for ( int knotidx=colrange.nrSteps(); knotidx>=0; knotidx-- )
	{
	    const RowCol rc( curpolygon,colrange.atIndex(knotidx) );
	    const Coord3 pos = surface->getKnot( rc );

	    if ( !pos.isDefined() || !selector.includes(pos) )
		continue;

	    EM::PolygonBodyGeometry& fg = polygonsurf->geometry();
	    const bool res = fg.nrKnots(curpolygon)==1
	       ? fg.removePolygon( curpolygon, true )
	       : fg.removeKnot( EM::PosID::getFromRowCol(rc), true );

	    if ( res ) change = true;
	}
    }

    if ( change )
    {
	EM::BodyMan().undo(emobject_->id()).setUserInteractionEnd(
		EM::BodyMan().undo(emobject_->id()).currentEventID() );
    }

    return change;
}


float PolygonBodyEditor::getNearestPolygon( int& polygon,
	const Coord3& mousepos, float zfactor ) const
{
    if ( !mousepos.isDefined() )
	return mUdf(float);

    int selsectionidx = -1, selpolygon = mUdf(int);
    float mindist = mUdf(float);

    const Geometry::Element* ge = emObject().geometryElement();
    if ( !ge ) return mUdf(float);

    mDynamicCastGet(const Geometry::PolygonSurface*,surface,ge);
    if ( !surface ) return mUdf(float);

    const StepInterval<int> rowrange = surface->rowRange();
    if ( rowrange.isUdf() ) return mUdf(float);

    for ( int polygonidx=rowrange.nrSteps(); polygonidx>=0; polygonidx-- )
    {
	Coord3 avgpos( 0, 0, 0 );
	const int curpolygon = rowrange.atIndex(polygonidx);
	const StepInterval<int> colrange = surface->colRange( curpolygon );
	if ( colrange.isUdf() )
	    continue;

	int count = 0;
	for ( int knotidx=colrange.nrSteps(); knotidx>=0; knotidx-- )
	{
	    const Coord3 pos = surface->getKnot(
		    RowCol(curpolygon,colrange.atIndex(knotidx)));

	    if ( pos.isDefined() )
	    {
		avgpos += mCompareCoord( pos );
		count++;
	    }
	}

	if ( !count ) continue;

	avgpos /= count;

	const Plane3 plane( surface->getPolygonNormal(curpolygon),
			    avgpos, false );
	const float disttoplane = (float)
	    plane.distanceToPoint( mCompareCoord(mousepos), true );

	if ( selsectionidx==-1 || fabs(disttoplane)<fabs(mindist) )
	{
	    mindist = disttoplane;
	    selpolygon = curpolygon;
	}
    }

    polygon = selpolygon;

    return mindist;
}


#define mRetNotInsideNext \
    if ( !nextdefined || (!sameSide3D(curpos,nextpos,v0,v1,0) && \
			  !sameSide3D(v0,v1,curpos,nextpos,0)) ) \
	return false;

#define mRetNotInsidePrev \
    if ( !prevdefined || (!sameSide3D(curpos,prevpos,v0,v1,0) && \
			  !sameSide3D(v0,v1,curpos,prevpos,0)) ) \
	return false;


bool PolygonBodyEditor::setPosition( const EM::PosID& pid, const Coord3& mpos )
{
    if ( !mpos.isDefined() ) return false;

    const BinID bid = SI().transform( mpos.getXY() );
    if ( !SI().inlRange( OD::UsrWork ).includes(bid.inl(),false) ||
	 !SI().crlRange( OD::UsrWork ).includes(bid.crl(),false) ||
	 !SI().zRange( OD::UsrWork ).includes(mpos.z_,false) )
	return false;

    const Geometry::Element* ge = emObject().geometryElement();
    mDynamicCastGet( const Geometry::PolygonSurface*, surface, ge );
    if ( !surface ) return false;

    const RowCol rc = pid.getRowCol();
    const StepInterval<int> colrg = surface->colRange( rc.row() );
    if ( colrg.isUdf() ) return false;

    const bool addtoundo = changedpids_.indexOf(pid) == -1;
    if ( addtoundo )
	changedpids_ += pid;

    if ( colrg.nrSteps()<3 )
	return emobject_->setPos( pid, mpos, addtoundo );

    const int zscale =  SI().zDomain().userFactor();
    const int previdx=rc.col()==colrg.start ? colrg.stop : rc.col()-colrg.step;
    const int nextidx=rc.col()<colrg.stop ? rc.col()+colrg.step : colrg.start;

    Coord3 curpos = mpos; curpos.z_ *= zscale;
    Coord3 prevpos = surface->getKnot( RowCol(rc.row(), previdx) );
    Coord3 nextpos = surface->getKnot( RowCol(rc.row(), nextidx) );

    const bool prevdefined = prevpos.isDefined();
    const bool nextdefined = nextpos.isDefined();
    if ( prevdefined ) prevpos.z_ *= zscale;
    if ( nextdefined ) nextpos.z_ *= zscale;

    for ( int knot=colrg.start; knot<=colrg.stop; knot += colrg.step )
    {
	const int nextknot = knot<colrg.stop ? knot+colrg.step : colrg.start;
	if ( knot==previdx || knot==rc.col() )
	    continue;

	Coord3 v0 = surface->getKnot( RowCol(rc.row(), knot) );
	Coord3 v1 = surface->getKnot( RowCol(rc.row(),nextknot));
	if ( !v0.isDefined() || !v1.isDefined() )
	    return false;

	v0.z_ *= zscale;
	v1.z_ *= zscale;
	if ( previdx==nextknot )
	{
	    mRetNotInsideNext
	}
	else if ( knot==nextidx )
	{
	    mRetNotInsidePrev
	}
	else
	{
	    mRetNotInsidePrev
	    mRetNotInsideNext
	}
    }

    return emobject_->setPos( pid, mpos, addtoundo );
}


void PolygonBodyEditor::getPidsOnPolygon(  EM::PosID& nearestpid0,
	EM::PosID& nearestpid1, EM::PosID& insertpid, int polygon,
	const Coord3& mousepos, float zfactor ) const
{
    nearestpid0 = EM::PosID::getInvalid();
    nearestpid1 = EM::PosID::getInvalid();
    insertpid = EM::PosID::getInvalid();
    if ( !mousepos.isDefined() ) return;

    const Geometry::Element* ge = emObject().geometryElement();
    mDynamicCastGet(const Geometry::PolygonSurface*,surface,ge);
    if ( !surface ) return;

    const StepInterval<int> colrange = surface->colRange( polygon );
    if ( colrange.isUdf() ) return;

    const Coord3 mp = mCompareCoord(mousepos);
    TypeSet<int> knots;
    int nearknotidx = -1;
    Coord3 nearpos;
    float minsqptdist = mUdf(float);
    for ( int knotidx=0; knotidx<colrange.nrSteps()+1; knotidx++ )
    {
	const Coord3 pt =
	    surface->getKnot( RowCol(polygon,colrange.atIndex(knotidx)) );
	if ( !pt.isDefined() )
	    continue;

	float sqdist = 0;
	if ( sowinghistory_.isEmpty() || sowinghistory_[0]!=pt )
	{
	    sqdist=(float) mCompareCoord(pt).sqDistTo(mCompareCoord(mousepos));
	    if ( mIsZero(sqdist, 1e-4) ) //mousepos is duplicated.
		return;
	}

	 if ( nearknotidx==-1 || sqdist<minsqptdist )
	 {
	     minsqptdist = sqdist;
	     nearknotidx = knots.size();
	     nearpos = mCompareCoord( pt );
	 }

	 knots += colrange.atIndex( knotidx );
    }

    if ( nearknotidx==-1 )
	return;

    nearestpid0 = EM::PosID::getFromRowCol( polygon, knots[nearknotidx] );
    if ( knots.size()<=2 )
    {
	insertpid = EM::PosID::getFromRowCol( polygon, knots.size() );
	return;
    }

    double minsqedgedist = -1;
    int nearedgeidx = mUdf(int);
    Coord3 v0, v1;
    for ( int knotidx=0; knotidx<knots.size(); knotidx++ )
    {
	const int col = knots[knotidx];
	Coord3 p0 = surface->getKnot(RowCol(polygon,col));
	Coord3 p1 = surface->getKnot( RowCol(polygon,
		    knots [ knotidx<knots.size()-1 ? knotidx+1 : 0 ]) );
	if ( !p0.isDefined() || !p1.isDefined() )
	    continue;

	p0.z_ *= zfactor;
	p1.z_ *= zfactor;

	const double t = (mp-p0).dot(p1-p0)/(p1-p0).sqAbs();
	if ( t<0 || t>1 )
	    continue;

	const double sqdist = mp.sqDistTo(p0+t*(p1-p0));
	if ( minsqedgedist==-1 || sqdist<minsqedgedist )
	{
	    minsqedgedist = sqdist;
	    nearedgeidx = knotidx;
	    v0 = p0;
	    v1 = p1;
	}
    }

    bool usenearedge = false;
    if ( minsqedgedist!=-1 && sowinghistory_.size()<=1 )
    {
	if ( nearknotidx==nearedgeidx ||
	     nearknotidx==(nearknotidx<knots.size()-1 ? nearknotidx+1 : 0) ||
	     ((v1-nearpos).cross(v0-nearpos)).dot((v1-mp).cross(v0-mp))<0  ||
	     minsqedgedist<minsqptdist )
	    usenearedge = true;
    }

    if ( usenearedge ) //use nearedgeidx only
    {
	if ( nearedgeidx<knots.size()-1 )
	{
	    nearestpid0 = EM::PosID::getFromRowCol(polygon,knots[nearedgeidx]);
	    nearestpid1 = EM::PosID::getFromRowCol( polygon,
						knots[nearedgeidx+1] );
	    insertpid = nearestpid1;
	}
	else
	{
	    const int nextcol = knots[nearedgeidx]+colrange.step;
	    insertpid = EM::PosID::getFromRowCol( polygon, nextcol );
	}

	return;
    }
    else  //use nearknotidx only
    {
	Coord3 prevpos = surface->getKnot( RowCol(polygon,
		    knots[nearknotidx ? nearknotidx-1 : knots.size()-1]) );
	Coord3 nextpos = surface->getKnot( RowCol(polygon,
		    knots[nearknotidx<knots.size()-1 ? nearknotidx+1 : 0]) );

	bool takeprevious;
	if ( sowinghistory_.size() <= 1 )
	{
	    const bool prevdefined = prevpos.isDefined();
	    const bool nextdefined = nextpos.isDefined();
	    if ( prevdefined ) prevpos.z_ *= zfactor;
	    if ( nextdefined ) nextpos.z_ *= zfactor;

	    takeprevious = prevdefined && nextdefined &&
			   sameSide3D(mp,prevpos,nearpos,nextpos,1e-3);
	}
	else
	    takeprevious = sowinghistory_[1]==prevpos;

	if ( takeprevious )
	{
	    if ( nearknotidx )
	    {
		nearestpid1 = EM::PosID::getFromRowCol( polygon,
						knots[nearknotidx-1] );
		insertpid = nearestpid0;
	    }
	    else
	    {
		const int insertcol = knots[nearknotidx]-colrange.step;
		insertpid = EM::PosID::getFromRowCol( polygon, insertcol );
	    }
	}
	else
	{
	    if ( nearknotidx<knots.size()-1 )
	    {
		nearestpid1 = EM::PosID::getFromRowCol( polygon,
							knots[nearknotidx+1] );
		insertpid = nearestpid1;
	    }
	    else
	    {
		const int insertcol = knots[nearknotidx]+colrange.step;
		insertpid = EM::PosID::getFromRowCol( polygon, insertcol );
	    }
	}
    }
}


};  // namespace MPE
