/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	A.H. Bril
 Date:		Jul 2005
________________________________________________________________________

-*/

#include "picksetfact.h"
#include "pickset.h"

#include "ascstream.h"
#include "binidvalset.h"
#include "ctxtioobj.h"
#include "datapointset.h"
#include "filepath.h"
#include "iodir.h"
#include "iodirentry.h"
#include "ioman.h"
#include "ioobj.h"
#include "iopar.h"
#include "keystrs.h"
#include "polygon.h"
#include "ptrman.h"
#include "separstr.h"
#include "streamconn.h"
#include "strmprov.h"
#include "survinfo.h"
#include "unitofmeasure.h"

mDefSimpleTranslatorioContext( PickSet, Loc )
mDefSimpleTranslatorSelector( PickSet )

bool PickSetTranslator::retrieve( Pick::Set& ps, const IOObj* ioobj,
				  bool checkdir, BufferString& bs )
{
    if ( !ioobj ) { bs = "Cannot find object in data base"; return false; }
    mDynamicCast(PickSetTranslator*,PtrMan<PickSetTranslator> tr,
		 ioobj->createTranslator());
    if ( !tr ) { bs = "Selected object is not a PointSet"; return false; }
    PtrMan<Conn> conn = ioobj->getConn( Conn::Read );
    if ( !conn )
        { bs = "Cannot open "; bs += ioobj->fullUserExpr(true); return false; }
    bs = tr->read( ps, *conn, checkdir );

    IOPar disppars;
    if ( Pick::Mgr().readDisplayPars(ioobj->key(),disppars) )
	ps.useDisplayPars( disppars );

    return bs.isEmpty();
}


bool PickSetTranslator::store( const Pick::Set& ps, const IOObj* ioobj,
				BufferString& bs )
{
    if ( !ioobj ) { bs = "No object to store set in data base"; return false; }
    mDynamicCast(PickSetTranslator*,PtrMan<PickSetTranslator> tr,
		 ioobj->createTranslator());
    if ( !tr ) { bs = "Selected object is not a PointSet"; return false; }

    bs = "";
    PtrMan<Conn> conn = ioobj->getConn( Conn::Write );
    if ( !conn )
        { bs = "Cannot open "; bs += ioobj->fullUserExpr(false); }
    else
	bs = tr->write( ps, *conn );

    IOPar disppars;
    ps.fillDisplayPars( disppars );
    Pick::Mgr().writeDisplayPars( ioobj->key(), disppars );

    FileMultiString pstype;
    ioobj->pars().get( sKey::Type(), pstype );
    if ( pstype.isEmpty() || pstype.size() > 1 )
    {
	ioobj->pars().set( sKey::Type(), ps.isPolygon() ? sKey::Polygon()
							: sKey::PickSet() );
	IOM().commitChanges( *ioobj );
    }

    return bs.isEmpty();
}


const char* dgbPickSetTranslator::read( Pick::Set& ps, Conn& conn,
					bool checkdir )
{
    if ( !conn.forRead() || !conn.isStream() )
	return "Internal error: bad connection";

    ascistream astrm( ((StreamConn&)conn).iStream() );
    if ( !astrm.isOK() )
	return "Cannot read from PointSet file";
    else if ( !astrm.isOfFileType(mTranslGroupName(PickSet)) )
	return "Input file is not a PointSet";
    if ( atEndOfSection(astrm) ) astrm.next();
    if ( atEndOfSection(astrm) )
	return "Input file contains no pick sets";

    ps.setName( IOM().nameOf(conn.linkedTo()) );

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
		ps.disp_.color_.setTransparency( 0 );
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
		Pick::Location loc;
		if ( !loc.fromString(astrm.keyWord()) )
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
	const Pos::SurvID survid( ps.getSurvID() );

	astrm.next();
	while ( !atEndOfSection(astrm) )
	{
	    Pick::Location loc;
	    loc.setSurvID( survid );
	    if ( loc.fromString(astrm.keyWord()) )
		ps += loc;

	    astrm.next();
	}
    }

    return nullptr;
}


const char* dgbPickSetTranslator::write( const Pick::Set& ps, Conn& conn )
{
    if ( !conn.forWrite() || !conn.isStream() )
	return "Internal error: bad connection";

    ascostream astrm( ((StreamConn&)conn).oStream() );
    astrm.putHeader( mTranslGroupName(PickSet) );
    if ( !astrm.isOK() )
	return "Cannot write to output PointSet file";

    BufferString str;
    IOPar par;
    ps.fillPar( par );
    par.set( sKey::ZUnit(),
	     UnitOfMeasure::surveyDefZStorageUnit()->name() );
    par.putTo( astrm );

    od_ostream& strm = astrm.stream();
    for ( int iloc=0; iloc<ps.size(); iloc++ )
    {
	ps[iloc].toString( str );
	strm << str << od_newline;
    }

    astrm.newParagraph();
    return astrm.isOK() ? 0
	:  "Error during write to output PointSet file";
}


void PickSetTranslator::createBinIDValueSets(
			const BufferStringSet& ioobjids,
			ObjectSet<BinIDValueSet>& bivsets )
{
    for ( int idx=0; idx<ioobjids.size(); idx++ )
    {
	TypeSet<Coord3> crds;
	if ( !getCoordSet(ioobjids.get(idx),crds) )
	    continue;

	BinIDValueSet* bs = new BinIDValueSet( 1, true );
	bivsets += bs;

	for ( int ipck=0; ipck<crds.size(); ipck++ )
	{
	    const Coord3& crd( crds[ipck] );
	    bs->add( SI().transform(crd), (float) crd.z );
	}
    }
}


void PickSetTranslator::createDataPointSets( const BufferStringSet& ioobjids,
					     ObjectSet<DataPointSet>& dpss,
					     bool is2d, bool mini )
{
    for ( int idx=0; idx<ioobjids.size(); idx++ )
    {
	TypeSet<Coord3> crds;
	if ( !getCoordSet(ioobjids.get(idx),crds) )
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


bool PickSetTranslator::getCoordSet( const char* id, TypeSet<Coord3>& crds )
{
    TypeSet<TrcKey> tks;
    return getCoordSet( id, crds, tks );
}


bool PickSetTranslator::getCoordSet( const char* id, TypeSet<Coord3>& crds,
				     TypeSet<TrcKey>& tks )
{
    const MultiID key( id );
    const int setidx = Pick::Mgr().indexOf( key );
    const Pick::Set* ps = setidx < 0 ? 0 : &Pick::Mgr().get( setidx );
    Pick::Set* createdps = 0;
    if ( !ps )
    {
	PtrMan<IOObj>ioobj = IOM().get( key );
	BufferString msg;
	if ( !ioobj )
	{
	    msg = "Cannot find PointSet with key "; msg += key;
	    ErrMsg( msg ); return false;
	}

	ps = createdps = new Pick::Set;
	if ( !retrieve(*createdps,ioobj,true,msg) )
	    { ErrMsg( msg ); delete createdps; return false; }
    }

    for ( int ipck=0; ipck<ps->size(); ipck++ )
    {
	crds += ((*ps)[ipck]).pos_;
	tks += ((*ps)[ipck]).trcKey();
    }

    delete createdps;
    return true;
}


void PickSetTranslator::fillConstraints( IOObjContext& ctxt, bool ispoly )
{
    if ( ispoly )
	ctxt.toselect_.require_.set( sKey::Type(), sKey::Polygon() );
    else
	ctxt.toselect_.require_.set( sKey::Type(), sKey::PickSet() );
}


ODPolygon<float>* PickSetTranslator::getPolygon( const IOObj& ioobj,
						 BufferString& emsg )
{
    Pick::Set ps; BufferString msg;
    if ( !PickSetTranslator::retrieve(ps,&ioobj,true,msg) )
    {
	emsg = "Cannot read polygon '"; emsg += ioobj.name();
	emsg += "':\n"; emsg += msg;
	return 0;
    }
    if ( ps.size() < 2 )
    {
	emsg = "Polygon '"; emsg += ioobj.name();
	emsg += "' contains less than 2 points";
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


bool PickSetTranslator::implRemove( const IOObj* ioobj ) const
{
    if ( !ioobj )
	return false;

    const bool res = Translator::implRemove( ioobj );
    if ( !res )
	return false;

    StreamProvider sp( Pick::SetMgr::getDispFileName(ioobj->key()) );
    if ( sp.exists(true) )
	sp.remove();

    return res;
}


bool PickSetTranslator::implRename( const IOObj* ioobj, const char* newnm,
				    const CallBack* cb ) const
{
    if ( !ioobj )
	return false;

    const bool res = Translator::implRename( ioobj, newnm, cb );
    if ( !res )
	return false;

    const FilePath fp( Pick::SetMgr::getDispFileName(ioobj->key()) );
    StreamProvider oldsp( fp.fullPath().buf() );
    FilePath newfp( newnm );
    newfp.setExtension( fp.extension() );
    if ( oldsp.exists(true) )
	oldsp.rename( newfp.fullPath().buf(), cb );

    const int setidx = Pick::Mgr().indexOf( ioobj->key() );
    if ( setidx>= 0 )
    {
	Pick::Set& ps = Pick::Mgr().get( setidx );
	ps.setName( ioobj->name() );
    }

    return res;
}


void PickSetTranslator::tagLegacyPickSets()
{
    IOObjContext ctxt = mIOObjContext( PickSet );
    ctxt.fixTranslator( dgbPickSetTranslator::translKey() );
    const IODir iodir( ctxt.getSelKey() );
    const ObjectSet<IOObj>& objs = iodir.getObjs();
    bool haschanges = false;
    for ( auto ioobj : objs )
    {
	if ( !ioobj || !ctxt.validIOObj(*ioobj) ||
					ioobj->pars().hasKey(sKey::Type()) )
	    continue;

	ioobj->pars().set( sKey::Type(), sKey::PickSet() );
	haschanges = true;
    }

    if ( haschanges )
	iodir.doWrite();
}


Pick::Set* Pick::getSet( const MultiID& mid, BufferString& errmsg )
{
    PtrMan<IOObj> ioobj = IOM().get( mid );
    const int setidx = Pick::Mgr().indexOf( mid );
    if ( setidx<0 )
    {
	Pick::Set* ps = new Pick::Set;
	if ( PickSetTranslator::retrieve(*ps,ioobj,true,errmsg) )
	{
	    Pick::Mgr().set( mid, ps );
	    return ps;
	}

	delete ps;
	return nullptr;
    }

    return &(Pick::Mgr().get(setidx));
}



Pick::Set* Pick::getSet( const DBKey& key, BufferString& errmsg )
{
    if ( !key.hasSurveyLocation() )
	return getSet( sCast(const MultiID&,key), errmsg );

    SurveyChanger chgr( key.surveyDiskLocation() );
    PtrMan<IOObj> ioobj = IODir::getObj( key );
    if ( !ioobj )
	return nullptr;

    Pick::Set* ps = new Pick::Set;
    if ( PickSetTranslator::retrieve(*ps,ioobj,true,errmsg) )
	return ps;

    delete ps;
    return nullptr;
}
