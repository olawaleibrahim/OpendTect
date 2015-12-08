/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	A.H. Bril
 Date:		Jul 2005
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "picksetfact.h"
#include "pickset.h"
#include "ctxtioobj.h"
#include "binidvalset.h"
#include "datapointset.h"
#include "ascstream.h"
#include "ioobj.h"
#include "ioman.h"
#include "iopar.h"
#include "ptrman.h"
#include "survinfo.h"
#include "streamconn.h"
#include "polygon.h"
#include "keystrs.h"

mDefSimpleTranslatorioContextWithExtra( PickSet, Loc,
	ctxt->toselect_.require_.set( sKey::Type(), "PickSet``Polygon" ) )

mDefSimpleTranslatorSelector( PickSet );

bool PickSetTranslator::retrieve( Pick::Set& ps, const IOObj* ioobj,
				  bool checkdir, uiString& bs )
{
    if ( !ioobj )
    {
	bs = uiStrings::phrCannotFindDBEntry( ioobj->uiName() );
	return false;
    }

    mDynamicCast(PickSetTranslator*,PtrMan<PickSetTranslator> tr,
		 ioobj->createTranslator());
    if ( !tr )
    {
	bs = uiStrings::phrSelectObjectWrongType( uiStrings::sPickSet() );
	return false;
    }

    PtrMan<Conn> conn = ioobj->getConn( Conn::Read );
    if ( !conn )
    {
	bs = uiStrings::phrCannotOpen( ioobj->uiName() );
	return false;
    }

    bs = tr->read( ps, *conn, checkdir );
    return bs.isEmpty();
}


bool PickSetTranslator::store( const Pick::Set& ps, const IOObj* ioobj,
			       uiString& bs )
{
    if ( !ioobj )
    {
	bs = uiStrings::phrCannotFindDBEntry( ioobj->uiName() );
	return false;
    }

    mDynamicCast(PickSetTranslator*,PtrMan<PickSetTranslator> tr,
		 ioobj->createTranslator());
    if ( !tr )
    {
	bs = uiStrings::phrSelectObjectWrongType( uiStrings::sPickSet() );
	return false;
    }

    bs = uiString::emptyString();
    PtrMan<Conn> conn = ioobj->getConn( Conn::Write );
    if ( !conn )
	{ bs = uiStrings::phrCannotOpen( ioobj->uiName() ); }
    else
	bs = tr->write( ps, *conn );

    return bs.isEmpty();
}


uiString dgbPickSetTranslator::read( Pick::Set& ps, Conn& conn,
				     bool checkdir )
{
    if ( !conn.forRead() || !conn.isStream() )
	return uiStrings::sCantOpenInpFile();

    ascistream astrm( ((StreamConn&)conn).iStream() );
    if ( !astrm.isOK() )
	return uiStrings::sCantOpenInpFile();
    else if ( !astrm.isOfFileType(mTranslGroupName(PickSet)) )
	return uiStrings::phrSelectObjectWrongType( uiStrings::sPickSet() );
    if ( atEndOfSection(astrm) ) astrm.next();
    if ( atEndOfSection(astrm) )
	return uiStrings::sNoValidData();

    ps.setName( IOM().nameOf(conn.linkedTo()) );

    Pick::Location loc;

    if ( astrm.hasKeyword("Ref") ) // Keep support for pre v3.2 format
    {
	// In old format we can find mulitple pick sets. Just gather them all
	// in the pick set
	for ( int ips=0; !atEndOfSection(astrm); ips++ )
	{
	    astrm.next();
	    if ( astrm.hasKeyword(sKey::Color()) )
	    {
		ps.disp_.color_.use( astrm.value() );
		astrm.next();
	    }
	    if ( astrm.hasKeyword(sKey::Size()) )
	    {
		ps.disp_.pixsize_ = astrm.getIValue();
		astrm.next();
	    }
	    if ( astrm.hasKeyword(Pick::Set::sKeyMarkerType()) )
	    {
		ps.disp_.markertype_ = astrm.getIValue();
		astrm.next();
	    }
	    while ( !atEndOfSection(astrm) )
	    {
		if ( !loc.fromString( astrm.keyWord() ) )
		    break;
		ps += loc;
		astrm.next();
	    }
	    while ( !atEndOfSection(astrm) ) astrm.next();
	    astrm.next();
	}
    }
    else // New format
    {
	IOPar iopar; iopar.getFrom( astrm );
	ps.usePar( iopar );

	astrm.next();
	while ( !atEndOfSection(astrm) )
	{
	    if ( loc.fromString( astrm.keyWord(), true, checkdir ) )
		ps += loc;

	    astrm.next();
	}
    }

    return ps.size() ? uiStrings::sEmptyString() : uiStrings::sNoValidData();
}


uiString dgbPickSetTranslator::write( const Pick::Set& ps, Conn& conn )
{
    if ( !conn.forWrite() || !conn.isStream() )
	return uiStrings::sCantOpenOutpFile();

    ascostream astrm( ((StreamConn&)conn).oStream() );
    astrm.putHeader( mTranslGroupName(PickSet) );
    if ( !astrm.isOK() )
	return uiStrings::sCantOpenOutpFile();

    BufferString str;
    IOPar par;
    ps.fillPar( par );
    par.putTo( astrm );

    od_ostream& strm = astrm.stream();
    for ( int iloc=0; iloc<ps.size(); iloc++ )
    {
	ps[iloc].toString( str );
	strm << str << od_newline;
    }

    astrm.newParagraph();
    return astrm.isOK() ? uiStrings::sEmptyString()
			: uiStrings::phrCannotWrite( uiStrings::sPickSet() );
}


void PickSetTranslator::createBinIDValueSets(
			const BufferStringSet& ioobjids,
			ObjectSet<BinIDValueSet>& bivsets,
			uiString& errmsg )
{
    for ( int idx=0; idx<ioobjids.size(); idx++ )
    {
	TypeSet<Coord3> crds;
	if ( !getCoordSet(ioobjids.get(idx),crds,errmsg) )
	    continue;

	BinIDValueSet* bs = new BinIDValueSet( 1, true );
	bivsets += bs;

	for ( int ipck=0; ipck<crds.size(); ipck++ )
	{
	    const Coord3& crd( crds[idx] );
	    bs->add( SI().transform(crd), (float) crd.z );
	}
    }
}


void PickSetTranslator::createDataPointSets( const BufferStringSet& ioobjids,
					     ObjectSet<DataPointSet>& dpss,
					     uiString& errmsg, bool is2d,
					     bool mini )
{
    for ( int idx=0; idx<ioobjids.size(); idx++ )
    {
	TypeSet<Coord3> crds;
	if ( !getCoordSet(ioobjids.get(idx),crds,errmsg) )
	    continue;

	DataPointSet* dps = new DataPointSet( is2d, mini );
	dpss += dps;

	DataPointSet::DataRow dr;
	for ( int ipck=0; ipck<crds.size(); ipck++ )
	{
	    const Coord3& crd( crds[ipck] );
	    dr.pos_.set( crd );
	    dps->addRow( dr );
	}
	dps->dataChanged();
    }
}


bool PickSetTranslator::getCoordSet( const char* id, TypeSet<Coord3>& crds,
				     uiString& errmsg )
{
    const MultiID key( id );
    const int setidx = Pick::Mgr().indexOf( key );
    const Pick::Set* ps = setidx < 0 ? 0 : &Pick::Mgr().get( setidx );
    Pick::Set* createdps = 0;
    if ( !ps )
    {
	PtrMan<IOObj> ioobj = IOM().get( key );
	if ( !ioobj )
	{
	    errmsg = uiStrings::phrCannotFindDBEntry( uiStrings::sPickSet() );
	    return false;
	}

	ps = createdps = new Pick::Set;
	if ( !retrieve(*createdps,ioobj,true,errmsg) )
	    { delete createdps; return false; }
    }

    for ( int ipck=0; ipck<ps->size(); ipck++ )
	crds += ((*ps)[ipck]).pos_;

    delete createdps;
    return true;
}


ODPolygon<float>* PickSetTranslator::getPolygon( const IOObj& ioobj,
						 uiString& errmsg )
{
    Pick::Set ps;
    if ( !PickSetTranslator::retrieve(ps,&ioobj,true,errmsg) )
	return 0;

    if ( ps.size() < 2 )
    {
	errmsg = tr( "Polygon '%1' contains less than 2 points" )
		   .arg( ioobj.uiName() );
	return 0;
    }

    ODPolygon<float>* ret = new ODPolygon<float>;
    for ( int idx=0; idx<ps.size(); idx++ )
    {
	const Pick::Location& pl = ps[idx];
	Coord fbid = SI().binID2Coord().transformBackNoSnap( pl.pos_ );
	ret->add( Geom::Point2D<float>((float) fbid.x,(float) fbid.y) );
    }

    return ret;
}
