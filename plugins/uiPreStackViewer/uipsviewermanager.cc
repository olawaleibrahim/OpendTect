/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Yuancheng Liu
 Date:		5-11-2007
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uipsviewermanager.cc,v 1.30 2008-12-19 21:58:00 cvsyuancheng Exp $";

#include "uipsviewermanager.h"

#include "bufstringset.h"
#include "ioman.h"
#include "ioobj.h"
#include "prestackgather.h"
#include "prestackprocessor.h"
#include "settings.h"
#include "survinfo.h"
#include "uidlggroup.h"
#include "uiflatviewer.h"
#include "uiflatviewstdcontrol.h"
#include "uiflatviewmainwin.h"
#include "uimenuhandler.h"
#include "uimsg.h"
#include "uiodapplmgr.h"
#include "uiodmain.h"
#include "uiodscenemgr.h"
#include "uipsviewershapetab.h"
#include "uipsviewerposdlg.h"
#include "uipsviewersettingdlg.h"
#include "uiseispartserv.h"
#include "uisoviewer.h"
#include "uivispartserv.h"
#include "visflatviewer.h"
#include "visplanedatadisplay.h"
#include "visprestackviewer.h"
#include "visseis2ddisplay.h"
#include "vistransform.h"
#include "uipsviewercoltab.h"
#include "uiamplspectrum.h"
#include "uiobjdisposer.h"


namespace PreStackView
{

uiViewerMgr::uiViewerMgr()
    : selectpsdatamenuitem_( "D&isplay PS Gather" )
    , positionmenuitem_( "P&osition ..." )  
    , proptymenuitem_( "&Properties ..." )				 
    , viewermenuitem_( "View in &2D Panel" )
    , amplspectrumitem_( "&Amplitude spectrum ..." )
    , removemenuitem_( "&Remove" ) 
    , visserv_( ODMainWin()->applMgr().visServer() )
    , preprocmgr_( new PreStack::ProcessManager )
{
    posdialogs_.allowNull();
    visserv_->removeAllNotifier().notify( mCB(this,uiViewerMgr,removeAllCB) );
    RefMan<MenuHandler> menuhandler = visserv_->getMenuHandler();

    IOM().surveyToBeChanged.notify(mCB(this,uiViewerMgr,surveyToBeChangedCB));
    ODMainWin()->sessionSave.notify( mCB(this,uiViewerMgr,sessionSaveCB) );
    ODMainWin()->sessionRestore.notify(mCB(this,uiViewerMgr,sessionRestoreCB));
       
    menuhandler->createnotifier.notify( mCB(this,uiViewerMgr,createMenuCB) );
    menuhandler->handlenotifier.notify( mCB(this,uiViewerMgr,handleMenuCB) );
}


uiViewerMgr::~uiViewerMgr()
{
    visserv_->removeAllNotifier().remove( mCB(this,uiViewerMgr,removeAllCB) );
    RefMan<MenuHandler> menuhandler = visserv_->getMenuHandler(); 

    IOM().surveyToBeChanged.remove(mCB(this,uiViewerMgr,surveyToBeChangedCB));
    ODMainWin()->sessionSave.remove( mCB(this,uiViewerMgr,sessionSaveCB) );
    ODMainWin()->sessionRestore.remove(mCB(this,uiViewerMgr,sessionRestoreCB));
    menuhandler->createnotifier.remove( mCB(this,uiViewerMgr,createMenuCB) );
    menuhandler->handlenotifier.remove( mCB(this,uiViewerMgr,handleMenuCB) );

    delete visserv_;
    removeAllCB( 0 );
    delete preprocmgr_;
}    


void uiViewerMgr::createMenuCB( CallBacker* cb )
{
    mDynamicCastGet( uiMenuHandler*, menu, cb );
    
    RefMan<visBase::DataObject> dataobj = visserv_->getObject( menu->menuID() );

    mDynamicCastGet( visSurvey::PlaneDataDisplay*, pdd, dataobj.ptr() );
    mDynamicCastGet( visSurvey::Seis2DDisplay*, s2d, dataobj.ptr() );
    if ( (pdd && pdd->getOrientation()!=visSurvey::PlaneDataDisplay::Timeslice) 
	   || s2d ) 
    {
	uiSeisPartServer* seisserv = ODMainWin()->applMgr().seisServer();

	BufferStringSet gnms; seisserv->getStoredGathersList(!s2d,gnms);
	if ( gnms.size() )
	{
	    selectpsdatamenuitem_.removeItems();
	    selectpsdatamenuitem_.createItems(gnms);
	    
	    mAddMenuItem( menu, &selectpsdatamenuitem_, true, false );
	}
	else
	    mResetMenuItem( &selectpsdatamenuitem_ );
    }
    else
	mResetMenuItem( &selectpsdatamenuitem_ );

    mDynamicCastGet( PreStackView::Viewer*, psv, dataobj.ptr() );
    if ( psv )
    {
    	mAddMenuItem( menu, &positionmenuitem_, true, false );
	mAddMenuItem( menu, &proptymenuitem_, true, false );
    	mAddMenuItem( menu, &viewermenuitem_, true, false ); 
    	mAddMenuItem( menu, &amplspectrumitem_, true, false ); 
    	mAddMenuItem( menu, &removemenuitem_, true, false ); 
    }
    else
    {
	mResetMenuItem( &positionmenuitem_ );
	mResetMenuItem( &proptymenuitem_ );
	mResetMenuItem( &viewermenuitem_ );
	mResetMenuItem( &amplspectrumitem_ );
	mResetMenuItem( &removemenuitem_ );
    }
}


void uiViewerMgr::handleMenuCB( CallBacker* cb )
{
    mCBCapsuleUnpackWithCaller( int, mnuid, caller, cb );
    mDynamicCastGet(uiMenuHandler*,menu,caller);

    if ( menu->isHandled() )
	return;

    int sceneid = getSceneID( menu->menuID() );
    if ( sceneid==-1 )
	return;

    const int mnuidx = selectpsdatamenuitem_.itemIndex( mnuid );

    RefMan<visBase::DataObject> dataobj = visserv_->getObject( menu->menuID() );
    mDynamicCastGet(PreStackView::Viewer*,psv,dataobj.ptr())
    if ( mnuidx < 0 && !psv )
	return;

    if ( mnuidx>=0 )
    {
	menu->setIsHandled( true );
	if ( !addNewPSViewer( menu, sceneid, mnuidx ) )
	    return;
    }
    else if ( mnuid==removemenuitem_.id )
    {
	menu->setIsHandled( true );
	visserv_->removeObject( psv, sceneid );
	const int idx = viewers_.indexOf( psv );
	viewers_.remove( idx )->unRef();
	delete posdialogs_.remove( idx );
    }
    else if ( mnuid==proptymenuitem_.id )
    {
	menu->setIsHandled( true );
	uiViewerSettingDlg dlg(menu->getParent(), *psv, *this, *preprocmgr_);
	dlg.go();
    }
    else if ( mnuid==positionmenuitem_.id )
    {
	menu->setIsHandled( true );
	const int idx = viewers_.indexOf( psv );
	if ( !posdialogs_[idx] )
	{
	    mDeclareAndTryAlloc( uiViewerPositionDlg*, dlg,
		    uiViewerPositionDlg( menu->getParent(), *psv ) );
	    if ( dlg )
		posdialogs_.replace( idx, dlg );
	}

	if ( posdialogs_[idx] )
	    posdialogs_[idx]->show();
    }
    else if ( mnuid==viewermenuitem_.id )
    {
	menu->setIsHandled( true );
	PtrMan<IOObj> ioobj = IOM().get( psv->getMultiID() );
	if ( !ioobj )
	   return;

	BufferString title = psv->is3DSeis() ?
	    getSeis3DTitle( psv->getBinID(), ioobj->name() ) :
	    getSeis2DTitle( psv->traceNr(), psv->lineName() );	
	uiFlatViewWin* viewwin = create2DViewer( title, psv->getDataPackID() );

	if ( viewwin )
	{
    	    viewwindows_ += viewwin;
    	    viewwin->start();
	}
    }
    else if ( mnuid==amplspectrumitem_.id )
    {
	menu->setIsHandled( true );
	uiAmplSpectrum* asd = new uiAmplSpectrum( menu->getParent() );
	asd->setDeleteOnClose( true );
	asd->setDataPackID( psv->getDataPackID(), DataPackMgr::FlatID );
	BufferString capt( "Amplitude spectrum for " );
	capt += psv->getObjectName();
	capt += " at ";
	if ( psv->is3DSeis() )
	    { capt += psv->getPosition().inl; capt += "/"; }
	capt += psv->getPosition().crl;
	asd->setCaption( capt );
	asd->show();
    }
}


int uiViewerMgr::getSceneID( int mnid )
{
    int sceneid = -1;
    TypeSet<int> sceneids;
    visserv_->getChildIds( -1, sceneids );
    for ( int idx=0; idx<sceneids.size(); idx++ )
    {
	TypeSet<int> scenechildren;
	visserv_->getChildIds( sceneids[idx], scenechildren );
	if ( scenechildren.indexOf( mnid ) )
	{
	    sceneid = sceneids[idx];
	    break;
	}
    }
    
    return sceneid;
}


#define mErrReturn(msg) { uiMSG().error(msg); return false; }

bool uiViewerMgr::addNewPSViewer( const uiMenuHandler* menu, 
				    int sceneid, int mnuidx )
{
    if ( !menu )
	return false;

    PtrMan<IOObj> ioobj = IOM().getLocal(
	    selectpsdatamenuitem_.getItem(mnuidx)->text );
    if ( !ioobj )
	mErrReturn( "No object selected" )

    RefMan<visBase::DataObject> dataobj =
	visserv_->getObject( menu->menuID() );
		
    mDynamicCastGet( visSurvey::PlaneDataDisplay*, pdd, dataobj.ptr() );
    mDynamicCastGet( visSurvey::Seis2DDisplay*, s2d, dataobj.ptr() );
    if ( !pdd && !s2d )
	mErrReturn( "Display panel is not set." )

    PreStackView::Viewer* viewer = PreStackView::Viewer::create();
    viewer->ref();
    viewer->setMultiID( ioobj->key() );
    visserv_->addObject( viewer, sceneid, false );
    viewers_ += viewer;
    posdialogs_ += 0;
   
    //Read defaults 
    const Settings& settings = Settings::fetch(uiViewerMgr::sSettingsKey()); 
    bool autoview;
    if ( settings.getYN(PreStackView::Viewer::sKeyAutoWidth(), autoview) )
	viewer->displaysAutoWidth( autoview );

    float factor;
    if ( settings.get( PreStackView::Viewer::sKeyFactor(), factor ) )
	viewer->setFactor( factor );
   
    float width; 
    if ( settings.get( PreStackView::Viewer::sKeyWidth(), width ) )
	viewer->setWidth( width );
    
    IOPar* flatviewpar = settings.subselect( sKeyFlatviewPars() );
    if ( flatviewpar )
	viewer->flatViewer()->appearance().ddpars_.usePar( *flatviewpar );
    
    if ( viewer->getScene() )
	viewer->getScene()->change.notifyIfNotNotified( 
		mCB( this, uiViewerMgr, sceneChangeCB ) );

    //set viewer position
    const Coord3 pickedpos = menu->getPickedPos();
    bool settingok = true;
    if ( pdd )
    {
	viewer->setSectionDisplay( pdd ); 
	BinID bid;
	if (  menu->getMenuType() != uiMenuHandler::fromScene ) 
	{
	    HorSampling hrg = pdd->getCubeSampling().hrg;
	    bid = SI().transform((SI().transform(hrg.start)
				 +SI().transform(hrg.stop))/2);
	}
	else bid = SI().transform( pickedpos );

	settingok = viewer->setPosition( bid );

	//This will make the initial color the same as the section.
	/*
	if ( viewer->flatViewer() && pdd->getColTabSequence(0) )
	{
	    viewer->flatViewer()->appearance().ddpars_.vd_.ctab_ =
		pdd->getColTabSequence(0)->name();
	    viewer->flatViewer()->handleChange( FlatView::Viewer::VDPars );
	}*/

    } 
    else if ( s2d )
    {
	int trcnr;
	if ( menu->getMenuType() != uiMenuHandler::fromScene )
	    trcnr = s2d->getTraceNrRange().center();
	else
	    trcnr = s2d->getNearestTraceNr( pickedpos );
	
	settingok = viewer->setSeis2DDisplay( s2d, trcnr );
    }
    
    if ( !settingok )
	return false;

    //set viewer angle.
    const uiSoViewer*  sovwr = ODMainWin()->sceneMgr().getSoViewer( sceneid );
    const Coord3 campos = sovwr->getCameraPosition();
    const Coord3 displaycampos = 
	viewer->getScene()->getUTM2DisplayTransform()->transformBack( campos );
    const BinID dir0 = SI().transform(displaycampos)-SI().transform(pickedpos);
    const Coord dir( dir0.inl, dir0.crl );
    viewer->displaysOnPositiveSide( viewer->getBaseDirection().dot(dir)>0 );
    
    return true;
}


#define mErrRes(msg) { uiMSG().error(msg); return 0; }

uiFlatViewWin* uiViewerMgr::create2DViewer( BufferString title, const int dpid )
{
    uiFlatViewWin* viewwin = new uiFlatViewMainWin( 
	    ODMainWin()->applMgr().seisServer()->appserv().parent(), 
	    uiFlatViewMainWin::Setup(title) );
    
    viewwin->setWinTitle( title );
    viewwin->setDarkBG( false );
    
    uiFlatViewer& vwr = viewwin->viewer();
    vwr.appearance().annot_.setAxesAnnot( true );
    vwr.appearance().setGeoDefaults( true );
    vwr.appearance().ddpars_.show( false, true );
    vwr.appearance().ddpars_.wva_.overlap_ = 1;

    DataPack* dp = DPM(DataPackMgr::FlatID).obtain( dpid );
    if ( !dp )
	return 0;

    mDynamicCastGet( const FlatDataPack*, fdp, dp );
    if ( !fdp )
    {
	DPM(DataPackMgr::FlatID).release( dp );
	return false;
    }

    vwr.setPack( false, dpid, false, true );
    int pw = 200 + 10 * fdp->data().info().getSize( 1 );
    if ( pw < 400 ) pw = 400; if ( pw > 800 ) pw = 800;
    
    vwr.setInitialSize( uiSize(pw,500) );  
    viewwin->addControl( new uiFlatViewStdControl( vwr,
			 uiFlatViewStdControl::Setup().withstates(false) ) );
    vwr.drawBitMaps();
    vwr.drawAnnot();
    DPM(DataPackMgr::FlatID).release( dpid );
    return viewwin;
}


void uiViewerMgr::sceneChangeCB( CallBacker* )
{
    for ( int idx = 0; idx<viewers_.size(); idx++ )
    {
	PreStackView::Viewer* psv = viewers_[idx];
	visBase::Scene* scene = psv->getScene();	

	int dpid = psv->getDataPackID();
	const visSurvey::PlaneDataDisplay* pdd = psv->getSectionDisplay();
	const visSurvey::Seis2DDisplay*    s2d = psv->getSeis2DDisplay();
	if ( pdd && (!scene || scene->getFirstIdx( pdd )==-1 ) )
	{
	    removeViewWin( dpid );
	    viewers_.remove( idx );
	    delete posdialogs_.remove( idx );
	    if ( scene ) visserv_->removeObject( psv, scene->id() );
	    psv->unRef();
	    idx--;
	}
	
	if ( s2d && (!scene || scene->getFirstIdx( s2d )==-1 ) )
	{
	    removeViewWin( dpid );
	    viewers_.remove( idx );
	    delete posdialogs_.remove( idx );
	    if ( scene ) visserv_->removeObject( psv, scene->id() );
	    psv->unRef();
	    idx--;
	}
    }
}


void uiViewerMgr::removeViewWin( const int dpid )
{
    for ( int idx=0; idx<viewwindows_.size(); idx++ )
    {
	if ( viewwindows_[idx]->viewer().packID(false) !=dpid )
	    continue;
	
	viewwindows_ -= viewwindows_[idx];
	delete viewwindows_[idx];
    }
}


void uiViewerMgr::sessionRestoreCB( CallBacker* )
{
    deepErase( viewwindows_ );

    TypeSet<int> vispsviewids;
    visserv_->findObject( typeid(PreStackView::Viewer), vispsviewids );

    for ( int idx=0; idx<vispsviewids.size(); idx++ )
    {
	mDynamicCastGet( PreStackView::Viewer*, psv,
			 visserv_->getObject(vispsviewids[idx]) );
	if ( !psv )
	    continue;

	if ( psv->getScene() )
	    psv->getScene()->change.notifyIfNotNotified( 
		    mCB( this, uiViewerMgr, sceneChangeCB ) );
	viewers_ += psv;
	posdialogs_ += 0;
	psv->ref();
    }
    
    PtrMan<IOPar> allwindowspar = ODMainWin()->sessionPars().subselect(
	    			  sKey2DViewers() );
    if ( !allwindowspar )
	allwindowspar =
	    ODMainWin()->sessionPars().subselect( "PreStack 2D Viewers" );

    int nrwindows;
    if ( !allwindowspar || !allwindowspar->get(sKeyNrWindows(), nrwindows) )
	return;

    for ( int idx=0; idx<nrwindows; idx++ )
    {
	BufferString key = sKeyViewerPrefix();
	key += idx;
	PtrMan<IOPar> viewerpar = allwindowspar->subselect( key.buf() );
	if ( !viewerpar )
	    continue;

	MultiID mid;
	bool is3d;
	int trcnr;
	BinID bid;
	BufferString name2d;
	if ( !viewerpar->get( sKeyMultiID(), mid ) ||
	     !viewerpar->get( sKeyBinID(), bid ) ||
	     !viewerpar->get( sKeyTraceNr(), trcnr ) ||
	     !viewerpar->get( sKeyLineName(), name2d ) ||
	     !viewerpar->getYN( sKeyIs3D(), is3d ) )
	{
	    if ( !viewerpar->get( "uiFlatViewWin MultiID", mid ) ||
		 !viewerpar->get( "uiFlatViewWin binid", bid ) ||
		 !viewerpar->get( "Seis2D TraceNr", trcnr ) ||
		 !viewerpar->get( "Seis2D Name", name2d ) ||
		 !viewerpar->getYN( "Seis3D display", is3d ) )
		continue;
	}
    
	PtrMan<IOObj> ioobj = IOM().get( mid );
	if ( !ioobj )
	   continue;

	BufferString title = !is3d ? getSeis2DTitle( trcnr, name2d ) :
	    			    getSeis3DTitle( bid, ioobj->name() );

	PreStack::Gather* gather = new PreStack::Gather;
	int dpid;
	if ( is3d && gather->readFrom(mid,bid) )
	    dpid = gather->id();
	else if ( gather->readFrom( *ioobj, trcnr, name2d ) )
	    dpid = gather->id();
	else 
	{
	    delete gather;
	    continue;	    
	}

	DPM(DataPackMgr::FlatID).add( gather );
	DPM(DataPackMgr::FlatID).obtain( dpid );
	uiFlatViewWin* viewwin = create2DViewer( title, dpid );
	DPM(DataPackMgr::FlatID).release( gather );
	if ( !viewwin )
	    continue;

	viewwindows_ += viewwin;
	viewwin->start();
    }
    
    if ( preprocmgr_ )
	preprocmgr_->usePar( *allwindowspar );

    for ( int idx=0; idx<viewers_.size(); idx++ )
	viewers_[idx]->setPreProcessor( preprocmgr_ );
}


const char* uiViewerMgr::getSeis2DTitle( const int tracenr, BufferString nm )
{
    BufferString title( "Gather from [" );
    title += nm;
    title += "] at trace " ;
    title += tracenr;

    return title;
}


const char* uiViewerMgr::getSeis3DTitle( BinID bid, BufferString name )
{
    BufferString title( "Gather from [" );
    title += name;
    title += "] at ";
    title += bid.inl;
    title += "/";
    title += bid.crl;

    return title;
}


void uiViewerMgr::sessionSaveCB( CallBacker* ) 
{
    IOPar allwindowpar;
    int nrsaved = 0;
    for ( int idx=0; idx<viewwindows_.size(); idx++ )
    {
	const FlatDataPack* dp = viewwindows_[idx]->viewer().pack( false );
	mDynamicCastGet( const PreStack::Gather*, gather, dp );
	if ( !gather )
	    continue;

	IOPar viewerpar;
	viewwindows_[idx]->viewer().fillPar( viewerpar );
	viewerpar.set( sKeyBinID(), gather->getBinID() );
	viewerpar.set( sKeyMultiID(), gather->getStorageID() );
	viewerpar.set( sKeyTraceNr(), gather->getSeis2DTraceNr() );
	viewerpar.set( sKeyLineName(), gather->getSeis2DName() );
	viewerpar.setYN( sKeyIs3D(), gather->is3D() );

	BufferString key = sKeyViewerPrefix();
	key += nrsaved;
	nrsaved++;

	allwindowpar.mergeComp( viewerpar, key );
    }

    if ( preprocmgr_ )
	preprocmgr_->fillPar( allwindowpar );

    allwindowpar.set( sKeyNrWindows(), nrsaved );
    ODMainWin()->sessionPars().mergeComp( allwindowpar, sKey2DViewers() );
}


void  uiViewerMgr::removeAllCB( CallBacker* )
{
    deepUnRef( viewers_ );
    deepErase( posdialogs_ );
    deepErase( viewwindows_ );
}    


void uiViewerMgr::surveyToBeChangedCB( CallBacker* )
{
    deepUnRef( viewers_ );
    deepErase( posdialogs_ );
    deepErase( viewwindows_ );
}

}; // Namespace
