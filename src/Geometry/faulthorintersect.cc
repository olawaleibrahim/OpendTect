/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Y. Liu
 * DATE     : March 2010
-*/

static const char* rcsID = "$Id: faulthorintersect.cc,v 1.13 2011-03-04 21:53:04 cvsyuancheng Exp $";

#include "faulthorintersect.h"

#include "binidsurface.h"
#include "indexedshape.h"
#include "positionlist.h"
#include "survinfo.h"
#include "trigonometry.h"
#include "explfaultsticksurface.h"
#include "isocontourtracer.h"


namespace Geometry
{


class FBIntersectionCalculator : public ParallelTask
{
public:
FBIntersectionCalculator( const BinIDSurface& surf, float surfshift, 
	const ExplFaultStickSurface& shape )
    : surf_( surf )
    , surfzrg_( -1, -1 )
    , zshift_( surfshift )
    , shape_( shape )
{}

od_int64 nrIterations() const   { return shape_.getGeometry().size(); }
const TypeSet<TypeSet<Coord3> >& getResult() const { return res_; }

bool doPrepare( int )
{
    for ( int idx=0; idx<nrIterations(); idx++ )
	res_ += TypeSet<Coord3>();

    const Array2D<float>* depths = surf_.getArray();
    const float* data = depths ? depths->getData() : 0;
    if ( !data )
	return false;

    bool found = false;
    const int totalsz = depths->info().getTotalSz();
    for ( int idx=0; idx<totalsz; idx++ )
    {
	if ( mIsUdf(data[idx]) )
	    continue;

	if ( !found )
	{
	    found = true;
	    surfzrg_.start = surfzrg_.stop = data[idx];
	}
	else
	    surfzrg_.include( data[idx] );
    }

    return found;
}


bool doWork( od_int64 start, od_int64 stop, int )
{
    const float zscale = SI().zScale();
    const StepInterval<int>& surfrrg = surf_.rowRange();
    const StepInterval<int>& surfcrg = surf_.colRange();
    RefMan<const Coord3List> coordlist = shape_.coordList();

    for ( int idx=start; idx<=stop; idx++ )
    {	
    	const IndexedGeometry* inp = shape_.getGeometry()[idx];
	if ( !inp ) continue;
    
	TypeSet<Coord3>& res = res_[idx];
	for ( int idy=0; idy<inp->coordindices_.size()-3; idy+=4 )
	{
	    Coord3 v[3];
	    for ( int k=0; k<3; k++ )
		v[k] = coordlist->get(inp->coordindices_[idy+k]);
    
	    const Coord3 center = (v[0]+v[1]+v[2])/3;
      
	    Interval<int> trrg, tcrg; 
	    Coord3 rcz[3];
	    bool allabove = true;
	    bool allbelow = true;
	    for ( int k=0; k<3; k++ )
	    {
		BinID bid = SI().transform( v[k] );
		RowCol rc(surfrrg.snap(bid.inl),surfcrg.snap(bid.crl));

		const float pz = surf_.getKnot(rc, false).z + zshift_;
		rcz[k] = Coord3( rc.row, rc.col, pz );
		bool defined = !mIsUdf(pz);
		if ( allabove )
    		    allabove = defined ? v[k].z>=pz : v[k].z >= surfzrg_.stop;
		if ( allbelow )
    		    allbelow = defined ? v[k].z<=pz : v[k].z <= surfzrg_.start;

		if ( !k )
		{
		    trrg.start = trrg.stop = rc.row;
		    tcrg.start = tcrg.stop = rc.col;
		}
		else
		{
		    trrg.include( rc.row );
		    tcrg.include( rc.col );
		}
	    }

	    if ( trrg.start > surfrrg.stop || trrg.stop < surfrrg.start ||
		 tcrg.start > surfcrg.stop || tcrg.stop < surfcrg.start ||
		 allabove || allbelow ) 
		continue; 

	    Coord3 tri[3];
	    for ( int k=0; k<3; k++ )
	    {
		tri[k] = v[k] - center;
		tri[k].z *= zscale;
	    }
	    Plane3 triangle( tri[0], tri[1], tri[2] );
	    
	    StepInterval<int> smprrg( mMAX(surfrrg.start, trrg.start),
		    		      mMIN(surfrrg.stop, trrg.stop), 1 );
	    StepInterval<int> smpcrg( mMAX(surfcrg.start, tcrg.start),
				      mMIN(surfcrg.stop, tcrg.stop), 1 );
	    Array2DImpl<float> field( smprrg.nrSteps()+1, smpcrg.nrSteps()+1 );
	    for ( int row=smprrg.start; row<=smprrg.stop; row++ )
	    {
		for ( int col=smpcrg.start; col<=smpcrg.stop; col++ )
		{
		    Coord3 pos = surf_.getKnot(RowCol(row,col), false);
		    float dist = mUdf( float );
		    if ( !mIsUdf(pos.z) )
		    {
			pos -= center;
			pos.z += zshift_;
			pos.z *= zscale;
			dist = triangle.distanceToPoint(pos,true);
		    }

		    field.set( row-smprrg.start, col-smpcrg.start, dist );
		}
	    }

	    IsoContourTracer ictracer( field );
	    ictracer.setSampling( smprrg, smpcrg );
	    ObjectSet<ODPolygon<float> > isocontours;
	    ictracer.getContours( isocontours, 0, false );
	    for ( int cidx=0; cidx<isocontours.size(); cidx++ )
	    {
		const ODPolygon<float>& ic = *isocontours[cidx];
		for ( int vidx=0; vidx<ic.size(); vidx++ )
		{
		    const Geom::Point2D<float> vertex = ic.getVertex( vidx );
		    if ( !pointInTriangle2D( Coord(vertex.x,vertex.y),
				rcz[0],rcz[1],rcz[2],0) )
			continue;

		    const int minrow = (int)vertex.x;
		    const int maxrow = minrow < vertex.x ? minrow+1 : minrow;
		    const int mincol = (int)vertex.y;
		    const int maxcol = mincol < vertex.y ? mincol+1 : mincol;

		    TypeSet<Coord3> neighbors;
		    TypeSet<float> weights;
		    float weightsum = 0;
		    bool isdone = false;
		    for ( int r=minrow; r<=maxrow; r++ )
		    {
			if ( isdone )
			    break;

			for ( int c=mincol; c<=maxcol; c++ )
			{
			    Coord3 pos = surf_.getKnot( RowCol(r,c), false );
			    if ( mIsUdf(pos.z) )
				continue;
			    else
				pos.z += zshift_;

			    float dist = fabs(r-vertex.x) + fabs(c-vertex.y);
			    if ( mIsZero(dist,1e-5) && res.indexOf(pos)!=-1 )
			    {
				res += pos;
				isdone = true;
				break;
			    }
			    else
				dist = 1/dist;
			    
			    weights += dist;
			    weightsum += dist;
			    neighbors += pos;
			}
		    }

		    if ( isdone || !neighbors.size() )
			continue;

		    Coord3 intersect(0,0,0);
		    for ( int pidx=0; pidx<neighbors.size(); pidx++ )
			intersect += neighbors[pidx] * weights[pidx];
		    intersect /= weightsum; 

		    if ( res.indexOf(intersect)!=-1 )
			continue;

		    Coord3 temp = intersect - center;
		    temp.z *= zscale;
		    if ( pointInTriangle3D(temp,tri[0],tri[1],tri[2],0) )
			res += intersect;
		}
		    
		if ( ic.isClosed() && res.size() )
		    res += res[0]; 
	    }
	    
	    deepErase( isocontours );
	}
    }

    return true;
}

protected:

const BinIDSurface&		surf_;
Interval<float>			surfzrg_;
float				zshift_;
const ExplFaultStickSurface&	shape_;
TypeSet<TypeSet<Coord3> >	res_;
};


FaultBinIDSurfaceIntersector::FaultBinIDSurfaceIntersector( float horshift,
	const BinIDSurface& surf, const ExplFaultStickSurface& eshape, 
	Coord3List& cl )
    : surf_( surf )
    , crdlist_( cl )
    , output_( 0 )
    , zshift_( horshift )
    , eshape_( eshape )						      
{}


void FaultBinIDSurfaceIntersector::setShape( const IndexedShape& ns )
{
    delete output_;
    output_ = &ns;
}


const IndexedShape* FaultBinIDSurfaceIntersector::getShape( bool takeover ) 
{ 
    return takeover ? output_ : new IndexedShape(*output_); 
}


void FaultBinIDSurfaceIntersector::compute()
{
    if ( !output_ || !output_->getGeometry()[0] )
	return;
    
    IndexedGeometry* geo = 
	const_cast<IndexedGeometry*>(output_->getGeometry()[0]);
    geo->removeAll( true );

    FBIntersectionCalculator calculator( surf_, zshift_, eshape_ );
    if ( !calculator.execute() )
	return;

    const TypeSet<TypeSet<Coord3> >& res = calculator.getResult();
    for ( int idx=0; idx<res.size(); idx++ )
    {
	const TypeSet<Coord3>& pres = res[idx];
	for ( int idz=0; idz<pres.size(); idz++ )
	    geo->coordindices_ += crdlist_.add( pres[idz] );
    }

    geo->coordindices_ += -1;
    geo->ischanged_ = true;
}




};

