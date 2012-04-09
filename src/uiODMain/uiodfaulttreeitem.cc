/*+
___________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author: 	K. Tingdahl
 Date: 		Jul 2003
___________________________________________________________________

-*/
static const char* rcsID = "$Id: uiodfaulttreeitem.cc,v 1.56 2012-04-09 22:15:07 cvsnanne Exp $";

#include "uiodfaulttreeitem.h"

#include "uimpepartserv.h"
#include "visfaultdisplay.h"
#include "visfaultsticksetdisplay.h"
#include "emfaultstickset.h"
#include "emfault3d.h"
#include "emmanager.h"
#include "mpeengine.h"
#include "ioman.h"
#include "ioobj.h"

#include "mousecursor.h"
#include "randcolor.h"
#include "uiempartserv.h"
#include "uimenu.h"
#include "uimenuhandler.h"
#include "uimsg.h"
#include "uiodapplmgr.h"
#include "uiodscenemgr.h"
#include "uivispartserv.h"


uiODFaultParentTreeItem::uiODFaultParentTreeItem()
   : uiODTreeItem( "Fault" )
{}

#define mAddMnuID	0
#define mNewMnuID	1

#define mDispInFull	2
#define mDispAtSect	3
#define mDispAtHors	4
#define mDispAtBoth	5
#define mDispPlanes	6
#define mDispSticks	7
#define mDispPSBoth	8


#define mInsertItm( menu, name, id, enable ) \
{ \
    uiMenuItem* itm = new uiMenuItem( name ); \
    menu->insertItem( itm, id ); \
    itm->setEnabled( enable ); \
}

bool uiODFaultParentTreeItem::showSubMenu()
{
    mDynamicCastGet(visSurvey::Scene*,scene,
		    ODMainWin()->applMgr().visServer()->getObject(sceneID()));
    if ( scene && scene->getZAxisTransform() )
    {
	//uiMSG().message( "Cannot add Faults to this scene" );
	//return false;
    }

    uiPopupMenu mnu( getUiParent(), "Action" );
    mnu.insertItem( new uiMenuItem("&Add ..."), mAddMnuID );
    mnu.insertItem( new uiMenuItem("&New ..."), mNewMnuID );

    if ( children_.size() )
    {
	bool candispatsect = false;
	bool candispathors = false;
	for ( int idx=0; idx<children_.size(); idx++ )
	{
	    mDynamicCastGet( uiODFaultTreeItem*, itm, children_[idx] );
	    mDynamicCastGet( visSurvey::FaultDisplay*, fd,
			applMgr()->visServer()->getObject(itm->displayID()) );

	    if ( fd && fd->canDisplayIntersections() )
		candispatsect = true;
	    if ( fd && fd->canDisplayHorizonIntersections() )
		candispathors = true;
	}

	mnu.insertSeparator();
	uiPopupMenu* dispmnu = new uiPopupMenu( getUiParent(), "&Display all" );

	mInsertItm( dispmnu, "&In full", mDispInFull, true );
	mInsertItm( dispmnu, "&Only at sections", mDispAtSect, candispatsect );
	mInsertItm( dispmnu, "Only at &horizons", mDispAtHors, candispathors );
	mInsertItm( dispmnu, "&At sections && horizons", mDispAtBoth,
					    candispatsect && candispathors );
	dispmnu->insertSeparator();
	mInsertItm( dispmnu, "Fault &planes", mDispPlanes, true );
	mInsertItm( dispmnu, "Fault &sticks", mDispSticks, true );
	mInsertItm( dispmnu, "&Fault planes && sticks", mDispPSBoth, true );
	mnu.insertItem( dispmnu );
    }

    addStandardItems( mnu );

    const int mnuid = mnu.exec();
    if ( mnuid==mAddMnuID )
    {
	ObjectSet<EM::EMObject> objs;
	applMgr()->EMServer()->selectFaults( objs, false );
	MouseCursorChanger uics( MouseCursor::Wait );
	for ( int idx=0; idx<objs.size(); idx++ )
	    addChild( new uiODFaultTreeItem(objs[idx]->id()), false );

	deepUnRef( objs );
    }
    else if ( mnuid == mNewMnuID )
    {
	RefMan<EM::EMObject> emo =
	    EM::EMM().createTempObject( EM::Fault3D::typeStr() );
	if ( !emo )
	    return false;

	emo->setPreferredColor( getRandomColor(false) );
	emo->setNewName();
	emo->setFullyLoaded( true );
	addChild( new uiODFaultTreeItem( emo->id() ), false );
	return true;
    }
    else if ( mnuid>=mDispInFull && mnuid<=mDispPSBoth )
    {
	for ( int idx=0; idx<children_.size(); idx++ )
	{
	    mDynamicCastGet( uiODFaultTreeItem*, itm, children_[idx] );
	    mDynamicCastGet( visSurvey::FaultDisplay*, fd,
			applMgr()->visServer()->getObject(itm->displayID()) );
	    if ( !fd ) continue;

	    if ( mnuid>=mDispPlanes && mnuid<=mDispPSBoth )
		fd->display( mnuid!=mDispPlanes, mnuid!=mDispSticks );

	    if ( mnuid>=mDispInFull && mnuid<=mDispAtBoth )
	    {
		const bool atboth = mnuid==mDispAtBoth;
		fd->displayIntersections( mnuid==mDispAtSect || atboth );
		fd->displayHorizonIntersections( mnuid==mDispAtHors || atboth );
	    }
	}
    }
    else
	handleStandardItems( mnuid );

    return true;
}


uiTreeItem* uiODFaultTreeItemFactory::createForVis(int visid, uiTreeItem*) const
{
    mDynamicCastGet(visSurvey::FaultDisplay*,fd,
	    ODMainWin()->applMgr().visServer()->getObject(visid));
    return fd ? new uiODFaultTreeItem( visid, true ) : 0;
}


#define mCommonInit \
    , savemnuitem_("&Save") \
    , saveasmnuitem_("Save as ...") \
    , displayplanemnuitem_ ( "Fault &planes" ) \
    , displaystickmnuitem_ ( "Fault &sticks" ) \
    , displayintersectionmnuitem_( "&Only at sections" ) \
    , displayintersecthorizonmnuitem_( "Only at &horizons" ) \
    , singlecolmnuitem_( "Use single &color" ) \

#define mCommonInit2 \
    displayplanemnuitem_.checkable = true; \
    displaystickmnuitem_.checkable = true; \
    displayintersectionmnuitem_.checkable = true; \
    displayintersecthorizonmnuitem_.checkable = true; \
    singlecolmnuitem_.checkable = true; \
    savemnuitem_.iconfnm = "save.png"; \
    saveasmnuitem_.iconfnm = "saveas.png"; \



uiODFaultTreeItem::uiODFaultTreeItem( const EM::ObjectID& oid )
    : uiODDisplayTreeItem()
    , emid_( oid )
    mCommonInit
{
    mCommonInit2
}


uiODFaultTreeItem::uiODFaultTreeItem( int id, bool dummy )
    : uiODDisplayTreeItem()
    , emid_(-1)
    , faultdisplay_(0)
    mCommonInit
{
    mCommonInit2
    displayid_ = id;
}


uiODFaultTreeItem::~uiODFaultTreeItem()
{
    if ( faultdisplay_ )
    {
	faultdisplay_->materialChange()->remove(
	    mCB(this,uiODFaultTreeItem,colorChCB));
	faultdisplay_->selection()->remove(
		mCB(this,uiODFaultTreeItem,selChgCB) );
	faultdisplay_->deSelection()->remove(
		mCB(this,uiODFaultTreeItem,deSelChgCB) );
	faultdisplay_->unRef();
    }
}


bool uiODFaultTreeItem::init()
{
    if ( displayid_==-1 )
    {
	visSurvey::FaultDisplay* fd = visSurvey::FaultDisplay::create();
	displayid_ = fd->id();
	faultdisplay_ = fd;
	faultdisplay_->ref();

	fd->setEMID( emid_ );
	visserv_->addObject( fd, sceneID(), true );
    }
    else
    {
	mDynamicCastGet( visSurvey::FaultDisplay*, fd,
			 visserv_->getObject(displayid_) );
	if ( !fd )
	    return false;

	faultdisplay_ = fd;
	faultdisplay_->ref();
	emid_ = fd->getEMID();
    }

    faultdisplay_->materialChange()->notify(
	    mCB(this,uiODFaultTreeItem,colorChCB));
    faultdisplay_->selection()->notify(
	    mCB(this,uiODFaultTreeItem,selChgCB) );
    faultdisplay_->deSelection()->notify(
	    mCB(this,uiODFaultTreeItem,deSelChgCB) );

    return uiODDisplayTreeItem::init();
}


void uiODFaultTreeItem::colorChCB( CallBacker* )
{
    updateColumnText( uiODSceneMgr::cColorColumn() );
}


void uiODFaultTreeItem::selChgCB( CallBacker* )
{ MPE::engine().setActiveFaultObjID( emid_ ); }


void uiODFaultTreeItem::deSelChgCB( CallBacker* )
{ MPE::engine().setActiveFaultObjID( -1 ); }


bool uiODFaultTreeItem::askContinueAndSaveIfNeeded( bool withcancel )
{
    return applMgr()->EMServer()->askUserToSave( emid_, withcancel );
}


void uiODFaultTreeItem::prepareForShutdown()
{
    if ( faultdisplay_ )
    {
	faultdisplay_->materialChange()->remove(
	    mCB(this,uiODFaultTreeItem,colorChCB));
	faultdisplay_->unRef();
    }

    faultdisplay_ = 0;
    uiODDisplayTreeItem::prepareForShutdown();
}


void uiODFaultTreeItem::createMenu( MenuHandler* menu, bool istb )
{
    uiODDisplayTreeItem::createMenu( menu, istb );
    if ( !menu || menu->menuID()!=displayID() || istb )
	return;

    mDynamicCastGet(visSurvey::FaultDisplay*,fd,
	    ODMainWin()->applMgr().visServer()->getObject(displayID()));
    if ( !fd )
	return;

    mAddMenuItem( &displaymnuitem_, &displayintersectionmnuitem_,
		  faultdisplay_->canDisplayIntersections(),
		  faultdisplay_->areIntersectionsDisplayed() );
    mAddMenuItem( &displaymnuitem_, &displayintersecthorizonmnuitem_,
		  faultdisplay_->canDisplayHorizonIntersections(),
		  faultdisplay_->areHorizonIntersectionsDisplayed() );
    mAddMenuItem( &displaymnuitem_, &displayplanemnuitem_, true,
		  faultdisplay_->arePanelsDisplayed() );
    mAddMenuItem( &displaymnuitem_, &displaystickmnuitem_, true,
		  faultdisplay_->areSticksDisplayed() );
    mAddMenuItem( menu, &displaymnuitem_, true, true );

    mAddMenuItem( &displaymnuitem_, &singlecolmnuitem_,
		  faultdisplay_->arePanelsDisplayedInFull(),
		  !faultdisplay_->showingTexture() );

    const bool enablesave = applMgr()->EMServer()->isChanged(emid_) &&
			    applMgr()->EMServer()->isFullyLoaded(emid_);

    mAddMenuItem( menu, &savemnuitem_, enablesave, false );
    mAddMenuItem( menu, &saveasmnuitem_, true, false );
}


void uiODFaultTreeItem::handleMenuCB( CallBacker* cb )
{
    uiODDisplayTreeItem::handleMenuCB(cb);
    mCBCapsuleUnpackWithCaller( int, mnuid, caller, cb );
    mDynamicCastGet(MenuHandler*,menu,caller);
    if ( menu->isHandled() || menu->menuID()!=displayID() || mnuid==-1 )
	return;

    if ( mnuid==saveasmnuitem_.id ||  mnuid==savemnuitem_.id )
    {
	menu->setIsHandled(true);
	bool saveas = mnuid==saveasmnuitem_.id ||
		      applMgr()->EMServer()->getStorageID(emid_).isEmpty();
	if ( !saveas )
	{
	    PtrMan<IOObj> ioobj =
		IOM().get( applMgr()->EMServer()->getStorageID(emid_) );
	    saveas = !ioobj;
	}

	applMgr()->EMServer()->storeObject( emid_, saveas );

	if ( saveas && faultdisplay_ &&
	     !applMgr()->EMServer()->getName(emid_).isEmpty() )
	{
	    faultdisplay_->setName( applMgr()->EMServer()->getName(emid_));
	    updateColumnText( uiODSceneMgr::cNameColumn() );
	}
    }
    else if ( mnuid==displayplanemnuitem_.id )
    {
	menu->setIsHandled(true);
	const bool stickchecked = displaystickmnuitem_.checked;
	const bool planechecked = displayplanemnuitem_.checked;
	faultdisplay_->display( stickchecked || planechecked, !planechecked );
    }
    else if ( mnuid==displaystickmnuitem_.id )
    {
	menu->setIsHandled(true);
	const bool stickchecked = displaystickmnuitem_.checked;
	const bool planechecked = displayplanemnuitem_.checked;
	faultdisplay_->display( !stickchecked, stickchecked || planechecked );
    }
    else if ( mnuid==displayintersectionmnuitem_.id )
    {
	menu->setIsHandled(true);
	const bool interchecked = displayintersectionmnuitem_.checked;
	faultdisplay_->displayIntersections( !interchecked );
    }
    else if ( mnuid==displayintersecthorizonmnuitem_.id )
    {
	menu->setIsHandled(true);
	const bool interchecked = displayintersecthorizonmnuitem_.checked;
	faultdisplay_->displayHorizonIntersections( !interchecked );
    }
    else if ( mnuid==singlecolmnuitem_.id )
    {
	menu->setIsHandled(true);
	faultdisplay_->useTexture( !faultdisplay_->showingTexture(), true );
	visserv_->triggerTreeUpdate();
    }
}



uiODFaultStickSetParentTreeItem::uiODFaultStickSetParentTreeItem()
   : uiODTreeItem( "FaultStickSet" )
{}


bool uiODFaultStickSetParentTreeItem::showSubMenu()
{
    mDynamicCastGet(visSurvey::Scene*,scene,
		    ODMainWin()->applMgr().visServer()->getObject(sceneID()));
    if ( scene && scene->getZAxisTransform() )
    {
	uiMSG().message( "Cannot add FaultStickSets to this scene" );
	return false;
    }

    uiPopupMenu mnu( getUiParent(), "Action" );
    mnu.insertItem( new uiMenuItem("&Add ..."), mAddMnuID );
    mnu.insertItem( new uiMenuItem("&New ..."), mNewMnuID );

    if ( children_.size() )
    {
	mnu.insertSeparator();
	uiPopupMenu* dispmnu = new uiPopupMenu( getUiParent(), "&Display all" );
	dispmnu->insertItem( new uiMenuItem("&In full"), mDispInFull );
	dispmnu->insertItem( new uiMenuItem("&Only at sections"), mDispAtSect );
	mnu.insertItem( dispmnu );
    }

    addStandardItems( mnu );

    const int mnuid = mnu.exec();
    if ( mnuid==mAddMnuID )
    {
	ObjectSet<EM::EMObject> objs;
	applMgr()->EMServer()->selectFaultStickSets( objs );
	MouseCursorChanger uics( MouseCursor::Wait );
	for ( int idx=0; idx<objs.size(); idx++ )
	    addChild( new uiODFaultStickSetTreeItem(objs[idx]->id()), false );
	deepUnRef( objs );
    }
    else if ( mnuid == mNewMnuID )
    {
	//applMgr()->mpeServer()->saveUnsaveEMObject();
	RefMan<EM::EMObject> emo =
	    EM::EMM().createTempObject( EM::FaultStickSet::typeStr() );
	if ( !emo )
	    return false;

	emo->setPreferredColor( getRandomColor(false) );
	emo->setNewName();
	emo->setFullyLoaded( true );
	addChild( new uiODFaultStickSetTreeItem( emo->id() ), false );
	return true;
    }
    else if ( mnuid==mDispInFull || mnuid==mDispAtSect )
    {
	for ( int idx=0; idx<children_.size(); idx++ )
	{
	    mDynamicCastGet( uiODFaultStickSetTreeItem*, itm, children_[idx] );
	    mDynamicCastGet( visSurvey::FaultStickSetDisplay*, fssd,
			applMgr()->visServer()->getObject(itm->displayID()) );
	    if ( fssd )
		fssd->setDisplayOnlyAtSections( mnuid==mDispAtSect );
	}
    }
    else
	handleStandardItems( mnuid );

    return true;
}


uiTreeItem*
uiODFaultStickSetTreeItemFactory::createForVis( int visid, uiTreeItem* ) const
{
    mDynamicCastGet(visSurvey::FaultStickSetDisplay*,fd,
	    ODMainWin()->applMgr().visServer()->getObject(visid));
    return fd ? new uiODFaultStickSetTreeItem( visid, true ) : 0;
}


#undef mCommonInit
#define mCommonInit \
    , faultsticksetdisplay_(0) \
    , savemnuitem_("&Save") \
    , saveasmnuitem_("Save &as ...") \
    , onlyatsectmnuitem_("&Only at sections")


uiODFaultStickSetTreeItem::uiODFaultStickSetTreeItem( const EM::ObjectID& oid )
    : uiODDisplayTreeItem()
    , emid_( oid )
    mCommonInit
{
    onlyatsectmnuitem_.checkable = true;
    savemnuitem_.iconfnm = "save.png";
    saveasmnuitem_.iconfnm = "saveas.png";
}


uiODFaultStickSetTreeItem::uiODFaultStickSetTreeItem( int id, bool dummy )
    : uiODDisplayTreeItem()
    , emid_(-1)
    mCommonInit
{
    displayid_ = id;
    onlyatsectmnuitem_.checkable = true;
    savemnuitem_.iconfnm = "save.png";
    saveasmnuitem_.iconfnm = "saveas.png";
}


uiODFaultStickSetTreeItem::~uiODFaultStickSetTreeItem()
{
    if ( faultsticksetdisplay_ )
    {
	faultsticksetdisplay_->materialChange()->remove(
	    mCB(this,uiODFaultStickSetTreeItem,colorChCB) );
	faultsticksetdisplay_->selection()->remove(
		mCB(this,uiODFaultStickSetTreeItem,selChgCB) );
	faultsticksetdisplay_->deSelection()->remove(
		mCB(this,uiODFaultStickSetTreeItem,deSelChgCB) );
	faultsticksetdisplay_->unRef();
    }
}


bool uiODFaultStickSetTreeItem::init()
{
    if ( displayid_==-1 )
    {
	visSurvey::FaultStickSetDisplay* fd =
				    visSurvey::FaultStickSetDisplay::create();
	displayid_ = fd->id();
	faultsticksetdisplay_ = fd;
	faultsticksetdisplay_->ref();

	fd->setEMID( emid_ );
	visserv_->addObject( fd, sceneID(), true );
    }
    else
    {
	mDynamicCastGet(visSurvey::FaultStickSetDisplay*,fd,
			visserv_->getObject(displayid_));
	if ( !fd )
	    return false;

	faultsticksetdisplay_ = fd;
	faultsticksetdisplay_->ref();
	emid_ = fd->getEMID();
    }

    faultsticksetdisplay_->materialChange()->notify(
	    mCB(this,uiODFaultStickSetTreeItem,colorChCB) );
    faultsticksetdisplay_->selection()->notify(
	    mCB(this,uiODFaultStickSetTreeItem,selChgCB) );
    faultsticksetdisplay_->deSelection()->notify(
	    mCB(this,uiODFaultStickSetTreeItem,deSelChgCB) );
    		
    return uiODDisplayTreeItem::init();
}


void uiODFaultStickSetTreeItem::colorChCB( CallBacker* )
{
    updateColumnText( uiODSceneMgr::cColorColumn() );
}


void uiODFaultStickSetTreeItem::selChgCB( CallBacker* )
{ MPE::engine().setActiveFSSObjID( emid_ ); }


void uiODFaultStickSetTreeItem::deSelChgCB( CallBacker* )
{ MPE::engine().setActiveFSSObjID( -1 ); }


bool uiODFaultStickSetTreeItem::askContinueAndSaveIfNeeded( bool withcancel )
{
    return applMgr()->EMServer()->askUserToSave( emid_, withcancel );
}


void uiODFaultStickSetTreeItem::prepareForShutdown()
{
    if ( faultsticksetdisplay_ )
    {
	faultsticksetdisplay_->materialChange()->remove(
	    mCB(this,uiODFaultStickSetTreeItem,colorChCB) );
	faultsticksetdisplay_->unRef();
    }

    faultsticksetdisplay_ = 0;
    uiODDisplayTreeItem::prepareForShutdown();
}


void uiODFaultStickSetTreeItem::createMenu( MenuHandler* menu, bool istb )
{
    uiODDisplayTreeItem::createMenu( menu, istb );
    if ( !menu || menu->menuID()!=displayID() || istb )
	return;

    mDynamicCastGet(visSurvey::FaultStickSetDisplay*,fd,
	    ODMainWin()->applMgr().visServer()->getObject(displayID()));
    if ( !fd )
	return;

    mAddMenuItem( menu, &displaymnuitem_, true, false );
    mAddMenuItem( &displaymnuitem_, &onlyatsectmnuitem_,
		  true, fd->displayedOnlyAtSections() );

    const bool enablesave = applMgr()->EMServer()->isChanged(emid_) &&
			    applMgr()->EMServer()->isFullyLoaded(emid_);
    mAddMenuItem( menu, &savemnuitem_, enablesave, false );
    mAddMenuItem( menu, &saveasmnuitem_, true, false );
}


void uiODFaultStickSetTreeItem::handleMenuCB( CallBacker* cb )
{
    uiODDisplayTreeItem::handleMenuCB(cb);
    mCBCapsuleUnpackWithCaller( int, mnuid, caller, cb );
    mDynamicCastGet(MenuHandler*,menu,caller);
    if ( menu->isHandled() || menu->menuID()!=displayID() || mnuid==-1 )
	return;

    mDynamicCastGet(visSurvey::FaultStickSetDisplay*,fd,
	    ODMainWin()->applMgr().visServer()->getObject(displayID()));

    if ( mnuid==onlyatsectmnuitem_.id )
    {
	menu->setIsHandled(true);
	if ( fd )
	    fd->setDisplayOnlyAtSections( !fd->displayedOnlyAtSections() );
    }
    else if ( mnuid==saveasmnuitem_.id ||  mnuid==savemnuitem_.id )
    {
	menu->setIsHandled(true);
	bool saveas = mnuid==saveasmnuitem_.id ||
		      applMgr()->EMServer()->getStorageID(emid_).isEmpty();
	if ( !saveas )
	{
	    PtrMan<IOObj> ioobj =
		IOM().get( applMgr()->EMServer()->getStorageID(emid_) );
	    saveas = !ioobj;
	}

	applMgr()->EMServer()->storeObject( emid_, saveas );

	const BufferString emname = applMgr()->EMServer()->getName(emid_);
	if ( saveas && faultsticksetdisplay_ && !emname.isEmpty() )
	{
	    faultsticksetdisplay_->setName( emname );
	    updateColumnText( uiODSceneMgr::cNameColumn() );
	}
    }
}
