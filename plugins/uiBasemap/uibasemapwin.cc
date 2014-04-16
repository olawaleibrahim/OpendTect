/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		January 2014
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "uibasemapwin.h"

#include "uibasemaptreeitem.h"
#include "uidockwin.h"
#include "uisurvmap.h"
#include "uitreeview.h"

#include "survinfo.h"


uiBasemapWin::uiBasemapWin( uiParent* p )
    : uiMainWin(p,Setup("Basemap").withmenubar(false).nrstatusflds(3)
				  .deleteonclose(false))
{
    basemapview_ = new uiSurveyMap( this );
    basemapview_->setPrefHeight( 250 );
    basemapview_->setPrefWidth( 250 );
    basemapview_->setSurveyInfo( &SI() );

    treedw_ = new uiDockWin( this, "Basemap Tree" );
    addDockWindow( *treedw_, uiMainWin::Left );

    tree_ = new uiTreeView( treedw_ );
    initTree();

    postFinalise().notify( mCB(this,uiBasemapWin,initWin) );
}


uiBasemapWin::~uiBasemapWin()
{}


void uiBasemapWin::initWin( CallBacker* )
{
    readSettings();
}


void uiBasemapWin::initTree()
{
    tree_->setColumnText( 0, "Elements" );

    uiBasemapTreeTop* topitm = new uiBasemapTreeTop( tree_, *basemapview_ );

    const BufferStringSet& nms = uiBasemapTreeItem::factory().getNames();
    const TypeSet<uiString>& usrnms =
				uiBasemapTreeItem::factory().getUserNames();
    for ( int idx=0; idx<nms.size(); idx++ )
    {
	uiBasemapTreeItem* itm =
		uiBasemapTreeItem::factory().create( nms.get(idx) );
	itm->setName( usrnms[idx].getFullString() );
	itm->setChecked( true );
	topitm->addChild( itm, true );
    }
}


bool uiBasemapWin::closeOK()
{
    saveSettings();
    return true;
}
