/*+
___________________________________________________________________

 CopyRight: 	(C) dGB Beheer B.V.
 Author: 	K. Tingdahl
 Date: 		Jul 2003
___________________________________________________________________

-*/
static const char* rcsID = "$Id: uiodhortreeitem.cc,v 1.14 2009-01-23 11:07:10 cvsnanne Exp $";

#include "uiodhortreeitem.h"

#include "emhorizon2d.h"
#include "emhorizon3d.h"
#include "emobject.h"
#include "emmanager.h"
#include "datapointset.h"
#include "survinfo.h"

#include "uiattribpartserv.h"
#include "uiemattribpartserv.h"
#include "uiempartserv.h"
#include "uihor2dfrom3ddlg.h"
#include "uimenu.h"
#include "uimenuhandler.h"
#include "uimpepartserv.h"
#include "uimsg.h"
#include "uiodapplmgr.h"
#include "uiodscenemgr.h"
#include "uivisemobj.h"
#include "uivispartserv.h"


#include "visemobjdisplay.h"
#include "vissurvscene.h"


uiODHorizonParentTreeItem::uiODHorizonParentTreeItem()
    : uiODTreeItem( "Horizon" )
{}


bool uiODHorizonParentTreeItem::showSubMenu()
{
    mDynamicCastGet(visSurvey::Scene*,scene,
	    	    ODMainWin()->applMgr().visServer()->getObject(sceneID()));

    const bool hastransform = scene && scene->getDataTransform();

    uiPopupMenu mnu( getUiParent(), "Action" );
    mnu.insertItem( new uiMenuItem("&Load ..."), 0 );
    uiMenuItem* newmenu = new uiMenuItem("&New ...");
    mnu.insertItem( newmenu, 1 );
    newmenu->setEnabled( !hastransform );
    if ( children_.size() )
    {
	mnu.insertSeparator();
	mnu.insertItem( new uiMenuItem("&Display all only at sections"), 2 );
	mnu.insertItem( new uiMenuItem("&Show all in full"), 3 );
    }

    addStandardItems( mnu );

    const int mnuid = mnu.exec();
    if ( mnuid == 0 )
    {
	TypeSet<EM::ObjectID> emids;
	applMgr()->EMServer()->selectHorizons( emids, false );
	for ( int idx=0; idx<emids.size(); idx++ )
	{
	    if ( emids[idx] < 0 ) continue;
	    addChild( new uiODHorizonTreeItem(emids[idx]), false );
	}
    }
    else if ( mnuid == 1 )
    {
	if ( !applMgr()->visServer()->
			 clickablesInScene(EM::Horizon3D::typeStr(),sceneID()) )
	    return true;
	//Will be restored by event (evWizardClosed) from mpeserv
	applMgr()->enableMenusAndToolBars( false );
	applMgr()->enableTree( false );
	applMgr()->visServer()->reportMPEWizardActive( true );

	uiMPEPartServer* mps = applMgr()->mpeServer();
	mps->setCurrentAttribDescSet( 
				applMgr()->attrServer()->curDescSet(false) );
	mps->addTracker( EM::Horizon3D::typeStr(), sceneID() );
	return true;
    }
    else if ( mnuid == 2 || mnuid == 3 )
    {
	const bool onlyatsection = mnuid == 2;
	for ( int idx=0; idx<children_.size(); idx++ )
	{
	    mDynamicCastGet(uiODEarthModelSurfaceTreeItem*,itm,children_[idx])
	    if ( itm )
	    {
		itm->visEMObject()->setOnlyAtSectionsDisplay( onlyatsection );
		itm->updateColumnText( uiODSceneMgr::cColorColumn() );
	    }
	}
    }
    else
	handleStandardItems( mnuid );

    return true;
}


uiTreeItem* uiODHorizonTreeItemFactory::create( int visid, uiTreeItem* ) const
{
    const char* objtype = uiVisEMObject::getObjectType(visid);
    if ( !objtype ) return 0;

    if ( !strcmp(objtype, EM::Horizon3D::typeStr() ) )
	return new uiODHorizonTreeItem(visid,true);

    return 0;
}


// uiODHorizonTreeItem

uiODHorizonTreeItem::uiODHorizonTreeItem( const EM::ObjectID& mid_ )
    : uiODEarthModelSurfaceTreeItem( mid_ )
{ initMenuItems(); }


uiODHorizonTreeItem::uiODHorizonTreeItem( int id, bool )
    : uiODEarthModelSurfaceTreeItem( 0 )
{
    initMenuItems();
    displayid_=id;
}


void uiODHorizonTreeItem::initMenuItems()
{
    shiftmnuitem_.text = "&Shift ..";
    algomnuitem_.text = "&Algorithms";
    fillholesmnuitem_.text = "Fill &holes ...";
    filterhormnuitem_.text = "&Filter ...";
    snapeventmnuitem_.text = "Snap to &event ...";
}


void uiODHorizonTreeItem::initNotify()
{
    mDynamicCastGet(visSurvey::EMObjectDisplay*,
	    	    emd,visserv_->getObject(displayid_));
    if ( emd )
	emd->changedisplay.notify( mCB(this,uiODHorizonTreeItem,dispChangeCB) );
}


BufferString uiODHorizonTreeItem::createDisplayName() const
{
    const uiVisPartServer* cvisserv =
	const_cast<uiODHorizonTreeItem*>(this)->visserv_;

    BufferString res = cvisserv->getObjectName( displayid_ );

    if (  uivisemobj_ && uivisemobj_->getShift() )
    {
	res += " (";
	res += uivisemobj_->getShift() * SI().zFactor();
	res += ")";
    }

    return res;
}


void uiODHorizonTreeItem::dispChangeCB(CallBacker*)
{
    updateColumnText( uiODSceneMgr::cColorColumn() );
}


void uiODHorizonTreeItem::createMenuCB( CallBacker* cb )
{
    uiODEarthModelSurfaceTreeItem::createMenuCB( cb );
    mDynamicCastGet(uiMenuHandler*,menu,cb);

    mDynamicCastGet(visSurvey::Scene*,scene,visserv_->getObject(sceneID()));

    const bool hastransform = scene && scene->getDataTransform();

    if ( menu->menuID()!=displayID() || hastransform )
    {
	mResetMenuItem( &shiftmnuitem_ );
	mResetMenuItem( &fillholesmnuitem_ );
	mResetMenuItem( &filterhormnuitem_ );
	mResetMenuItem( &snapeventmnuitem_ );
	mResetMenuItem( &createflatscenemnuitem_ );
    }
    else
    {
	mAddMenuItem( menu, &createflatscenemnuitem_, true, false );

	const bool islocked = visserv_->isLocked( displayID() );
	mAddMenuItem( menu, &shiftmnuitem_, !islocked, false )
	mAddMenuItem( menu, &algomnuitem_, true, false );
	mAddMenuItem( &algomnuitem_, &fillholesmnuitem_, !islocked, false );
	mAddMenuItem( &algomnuitem_, &filterhormnuitem_, !islocked, false );
	mAddMenuItem( &algomnuitem_, &snapeventmnuitem_, !islocked, false );
    }
}


void uiODHorizonTreeItem::handleMenuCB( CallBacker* cb )
{
    uiODEarthModelSurfaceTreeItem::handleMenuCB( cb );
    mCBCapsuleUnpackWithCaller( int, mnuid, caller, cb );
    mDynamicCastGet(uiMenuHandler*,menu,caller)
    if ( menu->menuID()!=displayID() || mnuid==-1 || menu->isHandled() )
	return;

    bool handled = true;
    if ( mnuid==fillholesmnuitem_.id )
	applMgr()->EMServer()->fillHoles( emid_ );
    else if ( mnuid==filterhormnuitem_.id )
	applMgr()->EMServer()->filterSurface( emid_ );
    else if ( mnuid==snapeventmnuitem_.id )
    {
	applMgr()->EMAttribServer()->snapHorizon( emid_ );
	visserv_->setObjectName( displayid_,
		(const char*) applMgr()->EMServer()->getName(emid_) );
	updateColumnText( uiODSceneMgr::cNameColumn() );
    }
    else if ( mnuid==shiftmnuitem_.id )
    {
	BufferStringSet attribnames;
	TypeSet<int> attribids;
	const int nrattrib = visserv_->getNrAttribs( displayID());
	bool depthdisplayed = false;
	for ( int idx=0; idx<nrattrib; idx++ )
	{
	    const Attrib::SelSpec* as =
		visserv_->getSelSpec( displayID(), idx );
	    if ( as->id() != as->cOtherAttrib() )
	    {
		if ( as->id() == as->cNoAttrib() )
		{
		    depthdisplayed = true;
		    continue;
		}
		attribids += idx;
		attribnames.add( as ? as->userRef() : "" );
	    }
	}
	if ( attribnames.isEmpty() && !depthdisplayed )
	{
	    BufferString msg( "Horizons cant be shifted for surface data");
	    uiMSG().error( msg );
	    return;
	}

	applMgr()->EMServer()->showHorShiftDlg( getUiParent(), emid_,
						attribnames, attribids );
    }
    else
	handled = false;

    menu->setIsHandled( handled );
}


uiODHorizon2DParentTreeItem::uiODHorizon2DParentTreeItem()
    : uiODTreeItem( "2D Horizon" )
{}


bool uiODHorizon2DParentTreeItem::showSubMenu()
{
    mDynamicCastGet(visSurvey::Scene*,scene,
	    	    ODMainWin()->applMgr().visServer()->getObject(sceneID()));
    const bool hastransform = scene && scene->getDataTransform();
    if ( hastransform )
    {
	uiMSG().message( "Cannot add 2D horizons to this scene (yet)" );
	return false;
    }

    uiPopupMenu mnu( getUiParent(), "Action" );
    mnu.insertItem( new uiMenuItem("&Load ..."), 0 );
    uiMenuItem* newmenu = new uiMenuItem("&New ...");
    mnu.insertItem( newmenu, 1 );
    mnu.insertItem( new uiMenuItem("&Create from 3D ..."), 2 );
    newmenu->setEnabled( !hastransform );
    if ( children_.size() )
    {
	mnu.insertSeparator();
	mnu.insertItem( new uiMenuItem("&Display all only at sections"), 3 );
	mnu.insertItem( new uiMenuItem("&Show all in full"), 4 );
    }
    addStandardItems( mnu );

    const int mnuid = mnu.exec();
    if ( mnuid == 0 )
    {
	TypeSet<EM::ObjectID> emids;
	applMgr()->EMServer()->selectHorizons( emids, true ); 
	for ( int idx=0; idx<emids.size(); idx++ )
	{
	    if ( emids[idx] < 0 ) continue;
	    addChild( new uiODHorizon2DTreeItem(emids[idx]), false );
	}
    }
    else if ( mnuid == 1 )
    {
	if ( !applMgr()->visServer()->
			clickablesInScene(EM::Horizon2D::typeStr(),sceneID()) )
	    return true; 

	//Will be restored by event (evWizardClosed) from mpeserv
	applMgr()->enableMenusAndToolBars( false );
	applMgr()->enableTree( false );
	applMgr()->visServer()->reportMPEWizardActive( true );

	uiMPEPartServer* mps = applMgr()->mpeServer();
	mps->setCurrentAttribDescSet(applMgr()->attrServer()->curDescSet(true));
	mps->addTracker( EM::Horizon2D::typeStr(), sceneID() );
	return true;
    }
    else if ( mnuid == 2 )
    {
	uiHor2DFrom3DDlg dlg( getUiParent() );
	dlg.go();
    }
    else if ( mnuid == 3 || mnuid == 4 )
    {
	const bool onlyatsection = mnuid == 3;
	for ( int idx=0; idx<children_.size(); idx++ )
	{
	    mDynamicCastGet(uiODHorizon2DTreeItem*,itm,children_[idx])
	    if ( itm )
	    {
		itm->visEMObject()->setOnlyAtSectionsDisplay( onlyatsection );
		itm->updateColumnText( uiODSceneMgr::cColorColumn() );
	    }
	}
    }
    else
	handleStandardItems( mnuid );

    return true;
}


uiTreeItem* uiODHorizon2DTreeItemFactory::create( int visid, uiTreeItem* ) const
{
    const char* objtype = uiVisEMObject::getObjectType(visid);
    return objtype && !strcmp(objtype, EM::Horizon2D::typeStr())
	? new uiODHorizon2DTreeItem(visid,true) : 0;
}


// uiODHorizon2DTreeItem

uiODHorizon2DTreeItem::uiODHorizon2DTreeItem( const EM::ObjectID& mid_ )
    : uiODEarthModelSurfaceTreeItem( mid_ )
{ initMenuItems(); }


uiODHorizon2DTreeItem::uiODHorizon2DTreeItem( int id, bool )
    : uiODEarthModelSurfaceTreeItem( 0 )
{ 
    initMenuItems();
    displayid_=id; 
}


void uiODHorizon2DTreeItem::initMenuItems()
{
    derive3dhormnuitem_.text = "Derive &3D horizon ...";
}


void uiODHorizon2DTreeItem::initNotify()
{
    mDynamicCastGet(visSurvey::EMObjectDisplay*,
	    	    emd,visserv_->getObject(displayid_));
    if ( emd )
	emd->changedisplay.notify(mCB(this,uiODHorizon2DTreeItem,dispChangeCB));
}


void uiODHorizon2DTreeItem::dispChangeCB(CallBacker*)
{
    updateColumnText( uiODSceneMgr::cColorColumn() );
}


void uiODHorizon2DTreeItem::createMenuCB( CallBacker* cb )
{
    uiODEarthModelSurfaceTreeItem::createMenuCB( cb );
    mDynamicCastGet(uiMenuHandler*,menu,cb)

    if ( menu->menuID()!=displayID() )
    {
	mResetMenuItem( &derive3dhormnuitem_ );
	mResetMenuItem( &createflatscenemnuitem_ );
    }
    else
    {
	const bool isempty = applMgr()->EMServer()->isEmpty(emid_);
	mAddMenuItem( menu, &derive3dhormnuitem_, !isempty, false );
	mAddMenuItem( menu, &createflatscenemnuitem_, !isempty, false );
    }
	
}


void uiODHorizon2DTreeItem::handleMenuCB( CallBacker* cb )
{
    uiODEarthModelSurfaceTreeItem::handleMenuCB( cb );
    mCBCapsuleUnpackWithCaller( int, mnuid, caller, cb );
    mDynamicCastGet(uiMenuHandler*,menu,caller)
    if ( menu->menuID()!=displayID() || mnuid==-1 || menu->isHandled() )
	return;

    bool handled = true;
    if ( mnuid==derive3dhormnuitem_.id )
	applMgr()->EMServer()->deriveHor3DFrom2D( emid_ );
    else
	handled = false;

    menu->setIsHandled( handled );
}
