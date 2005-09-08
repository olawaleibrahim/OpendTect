/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        A.H. Bril
 Date:          May 2001
 RCS:           $Id: uiempartserv.cc,v 1.65 2005-09-08 10:25:07 cvsnanne Exp $
________________________________________________________________________

-*/

#include "uiempartserv.h"

#include "binidselimpl.h"
#include "emmanager.h"
#include "emsurfaceiodata.h"
#include "emsurfaceauxdata.h"
#include "emsticksettransl.h"
#include "emposid.h"
#include "emhorizon.h"
#include "emfault.h"

#include "datainpspec.h"
#include "parametricsurface.h"
#include "ctxtioobj.h"
#include "ioobj.h"
#include "ioman.h"
#include "survinfo.h"
#include "binidvalset.h"
#include "binidselimpl.h"
#include "surfaceinfo.h"
#include "cubesampling.h"
#include "uiimphorizon.h"
#include "uiimpfault.h"
#include "uiexphorizon.h"
#include "uiiosurfacedlg.h"
#include "uigeninputdlg.h"
#include "uilistboxdlg.h"
#include "uisurfaceman.h"
#include "uiexecutor.h"
#include "uiioobjsel.h"
#include "uimsg.h"
#include "uimenu.h"
#include "ptrman.h"

#include <math.h>

const int uiEMPartServer::evDisplayHorizon = 0;

#define mErrRet(s) { BufferString msg( "Cannot load '" ); msg += s; msg += "'";\
    			uiMSG().error( msg ); return false; }

#define mDynamicCastAll() \
    EM::EMManager& em = EM::EMM(); \
    const EM::ObjectID objid = em.multiID2ObjectID(id); \
    EM::EMObject* object = em.getObject(objid); \
    mDynamicCastGet(EM::Surface*,surface,object) \
    mDynamicCastGet(EM::Horizon*,hor,object) \
    mDynamicCastGet(EM::Fault*,fault,object) \
    mDynamicCastGet(EM::StickSet*,stickset,object) 


uiEMPartServer::uiEMPartServer( uiApplService& a )
	: uiApplPartServer(a)
    	, selemid_(*new MultiID(""))
{
}


uiEMPartServer::~uiEMPartServer()
{
    delete &selemid_;
}


void uiEMPartServer::manageSurfaces( bool hor )
{
    uiSurfaceMan dlg( appserv().parent(), hor );
    dlg.go();
}


bool uiEMPartServer::ioHorizon( bool imp )
{
    bool res = false;
    if ( imp )
    {
	uiImportHorizon dlg( appserv().parent() );
	res = dlg.go();
	if ( res && dlg.doDisplay() )
	{
	    selemid_ = dlg.getSelID();	
	    sendEvent( evDisplayHorizon );
	}
    }
    else
    {
	uiExportHorizon dlg( appserv().parent() );
	res = dlg.go();
    }

    return res;    
}


bool uiEMPartServer::importHorizon() { return ioHorizon( true ); }

bool uiEMPartServer::exportHorizon() { return ioHorizon( false ); }


BufferString uiEMPartServer::getName(const MultiID& mid) const
{
    EM::EMManager& em = EM::EMM();
    return em.objectName(em.multiID2ObjectID(mid));
}


const char* uiEMPartServer::getType(const MultiID& mid) const
{
    EM::EMManager& em = EM::EMM();
    return em.objectType(em.multiID2ObjectID(mid));
}


bool uiEMPartServer::isChanged(const MultiID& mid) const
{
    EM::EMManager& em = EM::EMM();
    EM::EMObject* emobj = em.getObject(em.multiID2ObjectID(mid));
    return emobj && emobj->isChanged();
}


bool uiEMPartServer::isFullResolution(const MultiID& mid) const
{
    EM::EMManager& em = EM::EMM();
    mDynamicCastGet(EM::Surface*,emsurf,em.getObject(em.multiID2ObjectID(mid)));
    return emsurf && emsurf->geometry.isFullResolution();
}


bool  uiEMPartServer::createHorizon( MultiID& id, const char* nm )
{ return createSurface( id, true, nm ); }


bool uiEMPartServer::createFault( MultiID& id, const char* nm )
{ return createSurface( id, false, nm ); }


bool uiEMPartServer::createSurface( MultiID& id, bool ishor, const char* name )
{
    const char* type = ishor ? EM::Horizon::typeStr() : EM::Fault::typeStr();
    if ( EM::EMM().findObject(type,name) != -1 )
    {
	if ( !uiMSG().askGoOn( "An object with that name does already exist."
				" Overwrite?", true ) )
	    return false;
    }
	
    EM::ObjectID objid = EM::EMM().createObject( type, name );
    if ( objid==-1 )
    {
	uiMSG().error("Could not create object with that name");
	return false;
    }

    mDynamicCastGet( EM::Surface*, emsurf, EM::EMM().getObject(objid) );
    emsurf->ref();
    id = emsurf->multiID();

    if ( !emsurf->geometry.nrSections() )
	emsurf->geometry.addSection(0,true);

    emsurf->unRefNoDelete();

    return true;
}


bool uiEMPartServer::selectHorizon( MultiID& id )
{ return selectSurface( id, true ); }


bool uiEMPartServer::selectFault( MultiID& id )
{ return selectSurface(id, false ); }


bool uiEMPartServer::selectSurface( MultiID& id, bool selhor )
{
    uiReadSurfaceDlg dlg( appserv().parent(), selhor );
    if ( !dlg.go() ) return false;

    IOObj* ioobj = dlg.ioObj();
    if ( !ioobj ) return false;
    
    id = ioobj->key();

    EM::SurfaceIOData sd;
    EM::SurfaceIODataSelection sel( sd );
    dlg.getSelection( sel );
    return loadSurface( id, &sel );
}


bool uiEMPartServer::selectStickSet( MultiID& multiid )
{
   PtrMan<CtxtIOObj> ctio = mMkCtxtIOObj(EMStickSet);
   ctio->ctxt.forread = true;
   uiIOObjSelDlg dlg( appserv().parent(), *ctio );
   if ( !dlg.go() ) return false;

    multiid = dlg.ioObj()->key();

    EM::EMManager& em = EM::EMM();
    EM::ObjectID objid = em.multiID2ObjectID(multiid);
    if ( em.getObject(objid) )
	return true;

    PtrMan<Executor> loadexec = em.objectLoader(multiid);
    if ( !loadexec)
	mErrRet( IOM().nameOf(multiid) );

    EM::EMObject* obj = EM::EMM().getObject( objid );
    obj->ref();
    uiExecutor exdlg( appserv().parent(), *loadexec );
    if ( exdlg.go() <= 0 )
    {
	obj->unRef();
	return false;
    }

    obj->unRefNoDelete();
    return true;
}


bool uiEMPartServer::createStickSet( MultiID& id )
{
    DataInpSpec* inpspec = new StringInpSpec();
    uiGenInputDlg dlg( appserv().parent(), "Enter name", "EnterName", inpspec );

    bool success = false;
    while ( !success )
    {
	if ( !dlg.go() )
	    break;

	if ( EM::EMM().findObject(EM::StickSet::typeStr(),dlg.text()) != -1 )
	{
	    if ( !uiMSG().askGoOn(
			"An object with that name does already exist."
			 " Overwrite?", true ) )
		continue;
	}

	id = EM::EMM().createObject( EM::StickSet::typeStr(), dlg.text() );
	if ( id.ID(0)==-1 )
	{
	    uiMSG().error("Could not create object with that name");
	    continue;
	}

	success = true;
    }

    return success;
}


bool uiEMPartServer::loadAuxData( const MultiID& id, int selidx )
{
    mDynamicCastAll()
    if ( !surface ) return false;

    surface->auxdata.removeAll();
    PtrMan<Executor> exec = surface->auxdata.auxDataLoader( selidx );
    uiExecutor exdlg( appserv().parent(), *exec );
    return exdlg.go();
}


bool uiEMPartServer::loadAuxData( const MultiID& id, const char* attrnm )
{
    mDynamicCastAll()
    if ( !surface ) return false;
    
    EM::SurfaceIOData sd;
    em.getSurfaceData( id, sd );
    const int nritems = sd.valnames.size();
    int selidx = -1;
    for ( int idx=0; idx<nritems; idx++ )
    {
	const BufferString& nm = *sd.valnames[idx];
	if ( nm == attrnm )
	{ selidx= idx; break; }
    }

    if ( selidx < 0 ) return false;
    return loadAuxData( id, selidx );
}


bool uiEMPartServer::loadAuxData( const MultiID& id )
{
    mDynamicCastAll()
    if ( !surface ) return false;

    EM::SurfaceIOData sd;
    em.getSurfaceData( id, sd );
    uiListBoxDlg dlg( appserv().parent(), sd.valnames, "Surface data" );
    dlg.box()->setMultiSelect();
    if ( !dlg.go() ) return false;

    TypeSet<int> selattribs;
    dlg.box()->getSelectedItems( selattribs );
    if ( !selattribs.size() ) return false;

    surface->auxdata.removeAll();
    ExecutorGroup exgrp( "Surface data loader" );
    exgrp.setNrDoneText( "Nr done" );
    for ( int idx=0; idx<selattribs.size(); idx++ )
	exgrp.add( surface->auxdata.auxDataLoader(selattribs[idx]) );

    uiExecutor exdlg( appserv().parent(), exgrp );
    return exdlg.go();
}


bool uiEMPartServer::storeObject( const MultiID& id, bool storeas )
{
    mDynamicCastAll()
    if ( !object ) return false;

    PtrMan<Executor> exec = 0;

    if ( storeas && surface )
    {
	uiWriteSurfaceDlg dlg( appserv().parent(), *surface );
	if ( !dlg.go() ) return false;

	EM::SurfaceIOData sd;
	EM::SurfaceIODataSelection sel( sd );
	dlg.getSelection( sel );

	const MultiID& key = dlg.ioObj() ? dlg.ioObj()->key() : "";
	exec = surface->geometry.saver( &sel, &key );
    }
    else
	exec = object->saver();

    if ( !exec )
	return false;

    uiExecutor exdlg( appserv().parent(), *exec );
    return exdlg.go();
}


bool uiEMPartServer::storeAuxData( const MultiID& id, bool storeas )
{
    mDynamicCastAll()
    if ( !surface ) return false;

    int dataidx = -1;
    int fileidx = -1;
    if ( storeas )
    {
	uiStoreAuxData dlg( appserv().parent(), *surface );
	if ( !dlg.go() ) return false;

	dataidx = 0;
	fileidx = dlg.getDataFileIdx();
    }

    PtrMan<Executor> saver = surface->auxdata.auxDataSaver( dataidx, fileidx );
    if ( !saver )
    {
	uiMSG().error( "Cannot save attribute" );
	return false;
    }

    uiExecutor exdlg( appserv().parent(), *saver );
    return exdlg.go();
}


bool uiEMPartServer::getDataVal( const MultiID& id, 
				 ObjectSet<BinIDValueSet>& data, 
				 BufferString& attrnm, float& shift )
{
    mDynamicCastAll()
    if ( !surface ) return false;

    if ( !surface->auxdata.nrAuxData() )
	return false;

    int dataidx = 0;
    attrnm = surface->auxdata.auxDataName( dataidx );
    shift = surface->geometry.getShift();

    deepErase( data );
    for ( int sidx=0; sidx<surface->geometry.nrSections(); sidx++ )
    {
	const EM::SectionID sectionid = surface->geometry.sectionID( sidx );
	const Geometry::ParametricSurface* meshsurf =
				    surface->geometry.getSurface( sectionid );

	BinIDValueSet& res = *new BinIDValueSet( 1+1, false );
	data += &res;

	EM::PosID posid( objid, sectionid );
	const int nrnodes = meshsurf->nrKnots();
	for ( int idy=0; idy<nrnodes; idy++ )
	{
	    const RowCol rc = meshsurf->getKnotRowCol(idy);
	    const Coord3 coord = meshsurf->getKnot( rc, false );
	    const BinID bid = SI().transform(coord);
	    posid.setSubID( rc.getSerialized() );
	    const float auxval = surface->auxdata.getAuxDataVal(dataidx,posid);

	    res.add( bid, coord.z, auxval );
	}
    }

    return true;
}


void uiEMPartServer::setAuxData( const MultiID& id,
				 ObjectSet<BinIDValueSet>& data, 
				 const char* attribnm )
{
    BufferStringSet nms; nms.add( attribnm );
    setAuxData( id, data, nms );
}



void uiEMPartServer::setAuxData( const MultiID& id,
				 ObjectSet<BinIDValueSet>& data, 
				 const BufferStringSet& attribnms )
{
    mDynamicCastAll()
    if ( !surface ) { uiMSG().error( "Cannot find surface" ); return; }
    if ( !data.size() ) { uiMSG().error( "No data calculated" ); return; }

    surface->auxdata.removeAll();

    const int nrdatavals = data[0]->nrVals();
    TypeSet<int> dataidxs;
    for ( int idx=0; idx<nrdatavals; idx++ )
    {
	BufferString name;
	if ( idx<attribnms.size() )
	    name = attribnms.get(idx);
	else
	{
	    name = "AuxData"; name += idx;
	}

	dataidxs += surface->auxdata.addAuxData( name );
    }

    BinID bid;
    BinIDValueSet::Pos pos;
    const int nrvals = surface->auxdata.nrAuxData();
    float vals[nrvals];
    for ( int sidx=0; sidx<data.size(); sidx++ )
    {
	const EM::SectionID sectionid = surface->geometry.sectionID( sidx );
	BinIDValueSet& bivs = *data[sidx];

	EM::PosID posid( objid, sectionid );
	while ( bivs.next(pos) )
	{
	    bivs.get( pos, bid, vals );
	    RowCol rc( bid.inl, bid.crl );
	    EM::SubID subid = surface->geometry.rowCol2SubID( rc );
	    posid.setSubID( subid );
	    for ( int idv=0; idv<nrvals; idv++ )
		surface->auxdata.setAuxDataVal( dataidxs[idv], posid, 
						vals[idv+1] );
	}
    }
}


bool uiEMPartServer::loadSurface( const MultiID& id,
       				  const EM::SurfaceIODataSelection* newsel )
{
    EM::EMManager& em = EM::EMM();
    const EM::ObjectID objid = em.multiID2ObjectID(id);
    if ( em.getObject(objid) )
	return true;

    PtrMan<Executor> exec = em.objectLoader( id, newsel );
    if ( !exec ) mErrRet( IOM().nameOf(id) );

    EM::EMObject* obj = em.getObject(objid);
    obj->ref();
    uiExecutor exdlg( appserv().parent(), *exec );
    if ( exdlg.go() <= 0 )
    {
	obj->unRef();
	return false;
    }

    obj->unRefNoDelete();
    return true;
}


bool uiEMPartServer::importLMKFault()
{
    uiImportLMKFault dlg( appserv().parent() );
    return dlg.go();
}


void uiEMPartServer::getSurfaceInfo( ObjectSet<SurfaceInfo>& hinfos )
{
    EM::EMManager& em = EM::EMM();
    for ( int idx=0; idx<em.nrLoadedObjects(); idx++ )
    {
	mDynamicCastGet(EM::Horizon*,hor,em.getObject(em.objectID(idx)));
	if ( hor ) hinfos += new SurfaceInfo( hor->name(), hor->id() );
    }
}


void uiEMPartServer::getSurfaceDef( const ObjectSet<MultiID>& selhorids,
				    BinIDValueSet& bivs,
				    const BinIDRange* br ) const
{
    bivs.empty(); bivs.setNrVals( 2, false );
    PtrMan<BinIDRange> sibr = 0;
    if ( !selhorids.size() ) return;
    else if ( !br )
    {
	sibr = new BinIDRange; br = sibr;
	sibr->start = SI().sampling(false).hrg.start;
	sibr->stop = SI().sampling(false).hrg.stop;
    }

    EM::EMManager& em = EM::EMM();
    const EM::ObjectID& id = em.multiID2ObjectID(*selhorids[0]); 
    mDynamicCastGet(EM::Horizon*,hor,em.getObject(id))
    if ( !hor ) return;
    hor->ref();

    EM::Horizon* hor2 = 0;
    if ( selhorids.size() > 1 )
    {
	hor2 = (EM::Horizon*)(em.getObject(em.multiID2ObjectID(*selhorids[1])));
	hor2->ref();
    }

    const BinID step( SI().inlStep(), SI().crlStep() );
    BinID bid;
    for ( bid.inl=br->start.inl; bid.inl<br->stop.inl; bid.inl+=step.inl )
    {
	for ( bid.crl=br->start.crl; bid.crl<br->stop.crl; bid.crl+=step.crl )
	{
	    RowCol rc(bid.inl,bid.crl);
	    TypeSet<Coord3> z1pos, z2pos;
	    hor->geometry.getPos( rc, z1pos );
	    if ( !z1pos.size() ) continue;

	    if ( !hor2 )
	    {
		for ( int posidx=0; posidx<z1pos.size(); posidx++ )
		    bivs.add( bid, z1pos[posidx].z, z1pos[posidx].z );
	    }
	    else
	    {
		hor2->geometry.getPos( rc, z2pos );
		if ( !z2pos.size() ) continue;

		Interval<float> zintv;
		float dist = 999999;
		for ( int z1idx=0; z1idx<z1pos.size(); z1idx++ )
		{
		    for ( int z2idx=0; z2idx<z2pos.size(); z2idx++ )
		    {
			float dist_ = z2pos[z2idx].z - z1pos[z1idx].z;
			if ( fabs(dist_) < dist )
			{
			    zintv.start = z1pos[z1idx].z;
			    zintv.stop = z2pos[z2idx].z;
			}
		    }
		}

		zintv.sort();
		bivs.add( bid, zintv.start, zintv.stop );
	    }
	}
    }
    
    hor->unRef();
    if ( hor2 ) hor2->unRef();
}
