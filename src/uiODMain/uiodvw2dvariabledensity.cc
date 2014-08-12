/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Umesh Sinha
 Date:		June 2010
 RCS:		$Id$
________________________________________________________________________

-*/

#include "uiodvw2dvariabledensity.h"

#include "uiattribpartserv.h"
#include "uiflatviewwin.h"
#include "uiflatviewer.h"
#include "uimenuhandler.h"
#include "uiodviewer2d.h"
#include "uiodviewer2dmgr.h"
#include "uitaskrunner.h"
#include "uitreeview.h"

#include "attribdatapackzaxistransformer.h"
#include "attribdesc.h"
#include "attribdescset.h"
#include "attribdescsetsholder.h"
#include "coltabmapper.h"
#include "coltabsequence.h"
#include "filepath.h"
#include "flatposdata.h"
#include "ioobj.h"
#include "pixmap.h"
#include "seisioobjinfo.h"
#include "view2dseismic.h"
#include "view2ddataman.h"
#include "zaxistransform.h"


uiODVW2DVariableDensityTreeItem::uiODVW2DVariableDensityTreeItem()
    : uiODVw2DTreeItem( "VD" )
    , dummyview_(0)
    , menu_(0)
    , selattrmnuitem_(tr("Select Attribute"))
{}


uiODVW2DVariableDensityTreeItem::~uiODVW2DVariableDensityTreeItem()
{
    if ( viewer2D()->viewwin()->nrViewers() )
    {
	uiFlatViewer& vwr = viewer2D()->viewwin()->viewer(0);
	vwr.dataChanged.remove(
		mCB(this,uiODVW2DVariableDensityTreeItem,dataChangedCB) );
    }

    if ( menu_ )
    {
	menu_->createnotifier.remove(
		mCB(this,uiODVW2DVariableDensityTreeItem,createMenuCB) );
	menu_->handlenotifier.remove(
		mCB(this,uiODVW2DVariableDensityTreeItem,handleMenuCB) );
	menu_->unRef();
    }

    viewer2D()->dataMgr()->removeObject( dummyview_ );
}


bool uiODVW2DVariableDensityTreeItem::init()
{
    if ( !viewer2D()->viewwin()->nrViewers() )
	return false;

    uiFlatViewer& vwr = viewer2D()->viewwin()->viewer(0);
    vwr.dataChanged.notify(
	    mCB(this,uiODVW2DVariableDensityTreeItem,dataChangedCB) );

    const FlatView::DataDispPars& ddp = vwr.appearance().ddpars_;
    uitreeviewitem_->setCheckable( vwr.isVisible(true) &&
				   viewer2D()->selSpec(false).id().isValid() );
    uitreeviewitem_->setChecked( ddp.vd_.show_ );

    checkStatusChange()->notify(
	    mCB(this,uiODVW2DVariableDensityTreeItem,checkCB) );

    dummyview_ = new VW2DSeis();
    viewer2D()->dataMgr()->addObject( dummyview_ );

    if ( vwr.hasPack(false) )
    {
	if ( !vwr.control() )
	    displayMiniCtab(0);

	ColTab::Sequence seq( vwr.appearance().ddpars_.vd_.ctab_ );
	displayMiniCtab( &seq );
    }

    return true;
}


bool uiODVW2DVariableDensityTreeItem::select()
{
    if ( !uitreeviewitem_->isSelected() )
	return false;

    viewer2D()->dataMgr()->setSelected( dummyview_ );

    return true;
}


void uiODVW2DVariableDensityTreeItem::checkCB( CallBacker* )
{
    const uiFlatViewer& vwr = viewer2D()->viewwin()->viewer(0);
    if ( !vwr.hasPack(false) )
    {
	if ( !isChecked() ) return;
	const DataPack::ID dpid = viewer2D()->getDataPackID( false );
	if ( dpid != DataPack::cNoID() ) viewer2D()->setUpView( dpid, false );
	return;
    }

    for ( int ivwr=0; ivwr<viewer2D()->viewwin()->nrViewers(); ivwr++ )
	viewer2D()->viewwin()->viewer(ivwr).setVisible( false, isChecked() );
}


void uiODVW2DVariableDensityTreeItem::dataChangedCB( CallBacker* )
{
    if ( !viewer2D()->viewwin()->nrViewers() )
	return;

    displayMiniCtab(0);

    uiFlatViewer& vwr = viewer2D()->viewwin()->viewer(0);
    const FlatView::DataDispPars& ddp = vwr.appearance().ddpars_;

    uitreeviewitem_->setCheckable( vwr.isVisible(true) &&
				   viewer2D()->selSpec(false).id().isValid() );
    uitreeviewitem_->setChecked( ddp.vd_.show_ );

    if ( !vwr.hasPack(false) )
	displayMiniCtab(0);
    else
    {
	if ( !vwr.control() )
	    displayMiniCtab(0);

	ColTab::Sequence seq( ddp.vd_.ctab_ );
	displayMiniCtab( &seq );
    }
}


void uiODVW2DVariableDensityTreeItem::displayMiniCtab(
						const ColTab::Sequence* seq )
{
    if ( !seq )
    {
	uiTreeItem::updateColumnText( uiODViewer2DMgr::cColorColumn() );
	return;
    }

    PtrMan<ioPixmap> pixmap = new ioPixmap( *seq, cPixmapWidth(),
					    cPixmapHeight(), true );
    uitreeviewitem_->setPixmap( uiODViewer2DMgr::cColorColumn(), *pixmap );
}


bool uiODVW2DVariableDensityTreeItem::showSubMenu()
{
    if ( !menu_ )
    {
	menu_ = new uiMenuHandler( getUiParent(), -1 );
	menu_->ref();
	menu_->createnotifier.notify(
		mCB(this,uiODVW2DVariableDensityTreeItem,createMenuCB) );
	menu_->handlenotifier.notify(
		mCB(this,uiODVW2DVariableDensityTreeItem,handleMenuCB) );
    }
    return menu_->executeMenu(uiMenuHandler::fromTree());
}


void uiODVW2DVariableDensityTreeItem::createMenuCB( CallBacker* cb )
{
    selattrmnuitem_.removeItems();
    createSelMenu( selattrmnuitem_ );

    mDynamicCastGet(MenuHandler*,menu,cb);
    if ( selattrmnuitem_.nrItems() )
	mAddMenuItem( menu, &selattrmnuitem_, true, false );
}


void uiODVW2DVariableDensityTreeItem::handleMenuCB( CallBacker* cb )
{
    mCBCapsuleUnpackWithCaller( int, mnuid, caller, cb );
    mDynamicCastGet(MenuHandler*,menu,caller);
    if ( mnuid==-1 || menu->isHandled() )
	return;

    if ( handleSelMenu(mnuid) )
	menu->setIsHandled(true);
}


void uiODVW2DVariableDensityTreeItem::createSelMenu( MenuItem& mnu )
{
    const uiFlatViewer& vwr = viewer2D()->viewwin()->viewer(0);
    ConstDataPackRef<FlatDataPack> dp = vwr.obtainPack( false, true );
    if ( !dp ) return;

    const Attrib::SelSpec& as = viewer2D()->selSpec( false );
    MenuItem* subitem = 0;
    applMgr()->attrServer()->resetMenuItems();

    mDynamicCastGet(const Attrib::Flat2DDHDataPack*,dp2ddh,dp.ptr());
    if ( dp2ddh )
    {
//	BufferString ln;
//	dp2ddh->getLineName( ln );
// TODO: Use lnm to get attributes for this line only
	subitem = applMgr()->attrServer()->storedAttribMenuItem(as,true,false);
    }
    else
	subitem = applMgr()->attrServer()->storedAttribMenuItem(as,false,false);
    mAddMenuItem( &mnu, subitem, subitem->nrItems(), subitem->checked );
    subitem = applMgr()->attrServer()->calcAttribMenuItem( as, dp2ddh, true );
    mAddMenuItem( &mnu, subitem, subitem->nrItems(), subitem->checked );
    if ( dp2ddh )
    {
//	BufferString ln;
//	dp2ddh->getLineName( ln );
// TODO: Use lnm to get attributes for this line only
	subitem = applMgr()->attrServer()->storedAttribMenuItem(as,true,true);
    }
    else
	subitem = applMgr()->attrServer()->storedAttribMenuItem(as,false,true );
    mAddMenuItem( &mnu, subitem, subitem->nrItems(), subitem->checked );
}


bool uiODVW2DVariableDensityTreeItem::handleSelMenu( int mnuid )
{
    const Attrib::SelSpec& as = viewer2D()->selSpec( false );
    Attrib::SelSpec selas( as );

    uiAttribPartServer* attrserv = applMgr()->attrServer();

    const uiFlatViewer& vwr = viewer2D()->viewwin()->viewer(0);
    ConstDataPackRef<FlatDataPack> dp = vwr.obtainPack( false, true );
    if ( !dp ) return false;

    mDynamicCastGet(const Attrib::FlatRdmTrcsDataPack*,dprdm,dp.ptr());
    mDynamicCastGet(const Attrib::Flat2DDHDataPack*,dp2ddh,dp.ptr());

    DataPack::ID newid = DataPack::cNoID();
    RefMan<ZAxisTransform> zat = viewer2D()->getZAxisTransform();
    bool dousemulticomp = false;
    if ( dp2ddh )
    {
	BufferString attrbnm; bool stored = false;
	bool steering = false;
	attrserv->info2DAttribSubMenu( mnuid, attrbnm, steering, stored );

	uiTaskRunner uitr( &viewer2D()->viewwin()->viewer() );
	CubeSampling cs = dp2ddh->getCubeSampling();
	if ( zat ) cs.zrg = zat->getZInterval( true );

	if ( !stored )
	{
	    if ( !attrserv->handleAttribSubMenu(mnuid,selas,dousemulticomp) )
		return false;

	    attrserv->setTargetSelSpec( selas );
	    newid = attrserv->create2DOutput( cs, dp2ddh->getGeomID(), uitr );
	}
	else
	{
	    const SeisIOObjInfo objinfo( attrbnm );
	    if ( !objinfo.ioObj() )
		return false;

	    Attrib::DescID attribid = attrserv->getStoredID(
			    objinfo.ioObj()->key(), true, steering ? 1 : 0 );
	    selas.set( attrbnm, attribid, false, 0 );
	    selas.set2DFlag();

	    const Attrib::DescSet* ds = Attrib::DSHolder().getDescSet( true,
								       true );
	    if ( !ds )
		return false;

	    selas.setRefFromID( *ds );
	    selas.setUserRef( attrbnm );

	    const Attrib::Desc* targetdesc = ds->getDesc( attribid );
	    if ( !targetdesc )
		return false;

	    BufferString defstring;
	    targetdesc->getDefStr( defstring );
	    selas.setDefString( defstring );
	    attrserv->setTargetSelSpec( selas );
	    newid = attrserv->create2DOutput( cs, dp2ddh->getGeomID(), uitr );
	}
    }
    else if ( attrserv->handleAttribSubMenu(mnuid,selas,dousemulticomp) )
    {
	if ( dprdm )
	{
	    attrserv->setTargetSelSpec( selas );
	    const Interval<float> zrg = zat ? zat->getZInterval(true) :
		Interval<float>((float)dprdm->posData().range(false).start,
				(float)dprdm->posData().range(false).stop);

	    TypeSet<BinID> bids;
	    if ( dprdm->pathBIDs() )
		bids = *dprdm->pathBIDs();
	    newid = attrserv->createRdmTrcsOutput( zrg, &bids, &bids );
	}
	else
	{
	    newid = viewer2D()->createDataPack( selas );
	}
    }

    if ( zat && (dp2ddh || dprdm) )
    {
	ConstDataPackRef<FlatDataPack> newdp =
		DPM(DataPackMgr::FlatID()).obtain( newid );
	Attrib::FlatDataPackZAxisTransformer transformer( *zat );
	transformer.setInput( newdp.ptr() );
	transformer.setOutput( newid );
	transformer.execute();
    }

    if ( newid == DataPack::cNoID() ) return true;

    viewer2D()->setSelSpec( &selas, false );

    PtrMan<IOObj> ioobj = attrserv->getIOObj( selas );
    if ( ioobj )
    {
	FilePath fp( ioobj->fullUserExpr(true) );
	fp.setExtension( "par" );
	IOPar iop;
	if ( iop.read(fp.fullPath(),sKey::Pars()) && !iop.isEmpty() )
	{
	    ColTab::Sequence seq( 0 );
	    const char* ctname = iop.find( sKey::Name() );
	    seq = ColTab::Sequence( ctname );
	    displayMiniCtab( &seq );

	    ColTab::MapperSetup mapper;
	    mapper.usePar( iop );

	    for ( int ivwr=0; ivwr<viewer2D()->viewwin()->nrViewers(); ivwr++ )
	    {
		FlatView::DataDispPars& ddp =
		    viewer2D()->viewwin()->viewer(ivwr).appearance().ddpars_;
		ddp.vd_.ctab_ = ctname;
		ddp.vd_.mappersetup_ = mapper;
	    }
	}
    }

    viewer2D()->setUpView( newid, false );

    return true;
}


uiTreeItem* uiODVW2DVariableDensityTreeItemFactory::createForVis(
					const uiODViewer2D&,int id )const
{
    return 0;
}

