/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        N. Hemstra
 Date:          May 2002
________________________________________________________________________

-*/


#include "uiseisfileman.h"

#include "cbvsreadmgr.h"
#include "trckeyzsampling.h"
#include "dirlist.h"
#include "file.h"
#include "filepath.h"
#include "iopar.h"
#include "iostrm.h"
#include "keystrs.h"
#include "seis2dlineio.h"
#include "seiscbvs.h"
#include "seispsioprov.h"
#include "seistrctr.h"
#include "survinfo.h"
#include "seisstatscollector.h"
#include "zdomain.h"
#include "separstr.h"
#include "timedepthconv.h"
#include "fileview.h"

#include "ui2dgeomman.h"
#include "uitoolbutton.h"
#include "uilistbox.h"
#include "uitextedit.h"
#include "uiioobjmanip.h"
#include "uiioobjselgrp.h"
#include "uimergeseis.h"
#include "uiseispsman.h"
#include "uiseissampleeditor.h"
#include "uiseiscopy.h"
#include "uiseisioobjinfo.h"
#include "uiseis2dfileman.h"
#include "uiseis2dgeom.h"
#include "uisplitter.h"
#include "uistatsdisplaywin.h"
#include "uitaskrunner.h"
#include "od_helpids.h"

mDefineInstanceCreatedNotifierAccess(uiSeisFileMan)

class uiSeisSampleEdBrowserMgr : public CallBacker
{ mODTextTranslationClass(uiSeisSampleEdBrowserMgr)
public:

uiSeisSampleEdBrowserMgr()
{
    uiSeisFileMan::BrowserDef* bdef = new uiSeisFileMan::BrowserDef;
    bdef->tooltip_ = tr( "Browse/Edit cube '%1'" );
    bdef->cb_ = mCB(this,uiSeisSampleEdBrowserMgr,doBrowse);
    uiSeisFileMan::addBrowser( bdef );
}

void doBrowse( CallBacker* cb )
{
    mDynamicCastGet(uiSeisFileMan*,sfm,cb)
    if ( sfm && sfm->curIOObj() )
	uiSeisSampleEditor::launch( sfm, sfm->curIOObj()->key() );
}

};

static uiSeisSampleEdBrowserMgr* sampleedbrowsermgr_ = 0;


#define mHelpID is2d ? mODHelpKey(mSeisFileMan2DHelpID) : \
                       mODHelpKey(mSeisFileMan3DHelpID)
uiSeisFileMan::uiSeisFileMan( uiParent* p, bool is2d )
    :uiObjFileMan(p,uiDialog::Setup(is2d
            ? uiStrings::phrManage(tr("2D Seismics"))
            : uiStrings::phrManage(tr("3D Seismics")),
				    mNoDlgTitle,mHelpID)
				    .nrstatusflds(1).modal(false),
		  SeisTrcTranslatorGroup::ioContext())
    , is2d_(is2d)
    , browsebut_(0)
    , man2dlinesbut_(0)
    , mergecubesbut_(0)
{
    if ( !sampleedbrowsermgr_ )
	sampleedbrowsermgr_ = new uiSeisSampleEdBrowserMgr;

    IOObjContext* freshctxt = Seis::getIOObjContext(
					is2d_ ? Seis::Line : Seis::Vol, true );
    ctxt_ = *freshctxt;
    delete freshctxt;

    createDefaultUI( true );
    selgrp_->getListField()->doubleClicked.notify(
				is2d_ ? mCB(this,uiSeisFileMan,man2DPush)
				      : mCB(this,uiSeisFileMan,browsePush) );

    uiIOObjManipGroup* manipgrp = selgrp_->getManipGroup();

    copybut_ = manipgrp->addButton( "copyobj", is2d ? uiStrings::phrCopy(
				tr("dataset")) : uiStrings::phrCopy(tr("cube")),
				mCB(this,uiSeisFileMan,copyPush) );
    if ( is2d )
    {
	man2dlinesbut_ = manipgrp->addButton( "man2d", uiStrings::phrManage(
					   uiStrings::sLine(2)),
					   mCB(this,uiSeisFileMan,man2DPush) );
    }
    else
    {
	mergecubesbut_ = manipgrp->addButton( "mergeseis",uiStrings::phrMerge(
					tr("cube parts into one cube")),
					mCB(this,uiSeisFileMan,mergePush) );
	browsebut_ = manipgrp->addButton( "browseseis",
				tr("Browse/Edit this cube"),
				mCB(this,uiSeisFileMan,browsePush) );
    }

    histogrambut_ = manipgrp->addButton( "histogram", tr("Show histogram"),
				mCB(this,uiSeisFileMan,showHistogram) );
    attribbut_ = manipgrp->addButton( "attributes", sShowAttributeSet(),
				      mCB(this,uiSeisFileMan,showAttribSet) );

    mTriggerInstanceCreatedNotifier();
    selChg(0);
}


uiSeisFileMan::~uiSeisFileMan()
{
}


static ObjectSet<uiSeisFileMan::BrowserDef> browserdefs_;


int uiSeisFileMan::addBrowser( uiSeisFileMan::BrowserDef* bd )
{
    browserdefs_ += bd;
    return browserdefs_.size() - 1;
}


#define mIsOfTranslName(nm) (FixedString(curioobj_->translator()) == nm)
#define mIsOfTranslType(typ) \
	mIsOfTranslName(typ##SeisTrcTranslator::translKey())


void uiSeisFileMan::ownSelChg()
{
    setToolButtonProperties();
}


#define mSetButToolTip(but,str1,curattribnms,str2,deftt) { \
    if ( but ) \
    { \
	if ( but->isSensitive() ) \
	{ \
	    tt.setEmpty(); \
	    tt = str1; \
	    tt.appendPlainText( curattribnms ).appendPlainText( str2 ); \
	    but->setToolTip( tt ); \
	} \
	else \
	    but->setToolTip( deftt ); \
    } }

void uiSeisFileMan::setToolButtonProperties()
{
    BufferString cursel;
    if ( curioobj_ )
	cursel.add( curioobj_->name() );

    uiString tt;
    copybut_->setSensitive( !cursel.isEmpty() );
    mSetButToolTip(copybut_,tr("Make a Copy of '"), cursel,
		   "'", is2d_ ? uiStrings::phrCopy(tr("dataset")) :
		   uiStrings::phrCopy(uiStrings::sCube().toLower()));
    if ( browsebut_ )
    {
	const BrowserDef* bdef = getBrowserDef();
	const bool enabbrowse = curimplexists_ && bdef;
	browsebut_->setSensitive( enabbrowse );
	if ( !enabbrowse )
	    mSetButToolTip( browsebut_, tr("No browser for '"),
			    cursel, "'",
			    tr("Browse/edit selected cube") )
	else
	{
	    uiString bdeftt( bdef->tooltip_ );
	    browsebut_->setToolTip( bdeftt.arg(curioobj_->uiName()) );
	}
    }

    if ( mergecubesbut_ )
    {
	BufferStringSet selcubenms;
	selgrp_->getChosen( selcubenms );
	if ( selcubenms.size() > 1 )
	    mSetButToolTip(mergecubesbut_,uiStrings::sMerge(),
			   selcubenms.getDispString(2), "",
			   uiStrings::phrMerge(uiStrings::sCube().toLower()))
	else
	    mergecubesbut_->setToolTip( uiStrings::phrMerge(
					    uiStrings::sCube(2).toLower()) );
    }

    if ( man2dlinesbut_ )
    {
	man2dlinesbut_->setSensitive( !cursel.isEmpty() );
	mSetButToolTip(man2dlinesbut_,uiStrings::phrManage(tr("2D lines in '")),
		       cursel,"'", uiStrings::phrManage(uiStrings::sLine(2)))
    }

    if ( histogrambut_ )
    {
	const SeisIOObjInfo info( curioobj_ );
	histogrambut_->setSensitive( info.haveStats() );
    }

    if ( attribbut_ )
    {
	attribbut_->setSensitive( curioobj_ );
	if ( curioobj_ )
	{
	     File::Path fp( curioobj_->mainFileName() );
	     fp.setExtension( sProcFileExtension() );
	     attribbut_->setSensitive( File::exists(fp.fullPath()) );
	     mSetButToolTip(attribbut_,tr("Show AttributeSet for "),
			    cursel, "", sShowAttributeSet())
	}
	else
	    attribbut_->setToolTip( sShowAttributeSet() );
    }
}


const uiSeisFileMan::BrowserDef* uiSeisFileMan::getBrowserDef() const
{
    if ( curioobj_ )
    {
	for ( int ipass=0; ipass<2; ipass++ )
	{
	    for ( int idx=0; idx<browserdefs_.size(); idx++ )
	    {
		const BrowserDef* bdef = browserdefs_[idx];
		if ( ipass == 0 && bdef->name_ == curioobj_->translator() )
		    return bdef;
		else if ( ipass == 1 && bdef->name_.isEmpty() )
		    return bdef;
	    }
	}
    }

    return 0;
}


void uiSeisFileMan::mkFileInfo()
{
    uiPhrase txt;
    SeisIOObjInfo oinf( curioobj_ );

    if ( oinf.isOK() )
    {

    if ( is2d_ )
    {
	BufferStringSet nms;
	SeisIOObjInfo::Opts2D opts2d; opts2d.zdomky_ = "*";
	oinf.getLineNames( nms, opts2d );
	txt = tr("Number of lines: %1").arg( nms.size() );
    }

#define mAddRangeTxt(line) \
    txt.appendPhrase(uiStrings::sRange().toLower(), uiString::Space, \
				    uiString::OnSameLine); \
    txt.appendPlainText( ": " ); \
    txt.appendPlainText(toString(cs.hsamp_.start_.line)); \
    txt.appendPlainText( " - " ); \
    txt.appendPlainText(toString(cs.hsamp_.stop_.line)); \
    txt.appendPlainText( " [" ); \
    txt.appendPlainText(toString(cs.hsamp_.step_.line)).appendPlainText( "]" );

#define mAddZValTxt(memb) txt.appendPlainText(toString( \
					zistm ? mNINT32(1000*memb) : memb));

    const bool zistm = oinf.isTime();
    const ZDomain::Def& zddef = oinf.zDomainDef();
    TrcKeyZSampling cs;
    if ( !is2d_ )
    {
	if ( oinf.getRanges(cs) )
	{
	    txt.setEmpty();
	    if ( !mIsUdf(cs.hsamp_.stop_.inl()) )
	    {
		txt.appendPhrase(uiStrings::sInline(),uiString::NoSep);
		mAddRangeTxt(inl());
	    }
	    if ( !mIsUdf(cs.hsamp_.stop_.crl()) )
	    {
		txt.appendPhrase(uiStrings::sCrossline(),uiString::NoSep);
		mAddRangeTxt(crl());
	    }
	    float area = SI().getArea( cs.hsamp_.inlRange(),
				       cs.hsamp_.crlRange() );
	    txt.appendPhrase(uiStrings::sArea(),uiString::NoSep)
		.appendPlainText(": ")
		.appendPlainText( getAreaString( area, true, 0 ) );

	    const uiString rgstr = zddef.getRange();
	    txt.appendPhrase( rgstr, uiString::NoSep );
	    txt.appendPhrase( zddef.unitStr(true), uiString::NoSep,
		uiString::OnSameLine );
	    txt.appendPlainText( ": " ); mAddZValTxt(cs.zsamp_.start)
	    txt.appendPlainText(" - "); mAddZValTxt(cs.zsamp_.stop)
	    txt.appendPlainText(" ["); mAddZValTxt(cs.zsamp_.step)
	    txt.appendPlainText("]");
	}
    }

    if ( !curioobj_->pars().isEmpty() )
    {
	const IOPar& pars = curioobj_->pars();
	FixedString parstr = pars.find( "Type" );
	if ( !parstr.isEmpty() )
	    txt.appendPhrase( tr("Type"), uiString::NoSep )
			    .appendPlainText(": ").appendPlainText( parstr );

	parstr = pars.find( "Optimized direction" );
	if ( !parstr.isEmpty() )
	    txt.appendPhrase( tr("Optimized direction") )
			    .appendPlainText(": ").appendPlainText( parstr );
	if ( pars.isTrue("Is Velocity") )
	{
	    Interval<float> topvavg, botvavg;
	    txt.appendPhrase( tr("Velocity Type") ); txt.appendPlainText(": ");
	    parstr = pars.find( "Velocity Type" );
	    txt.appendPhrase( parstr.isEmpty() ? tr("<unknown>") :
		toUiString(parstr), uiString::NoSep, uiString::OnSameLine );

	    if ( pars.get(VelocityStretcher::sKeyTopVavg(),topvavg)
	      && pars.get(VelocityStretcher::sKeyBotVavg(),botvavg))
	    {
		const StepInterval<float> sizrg = SI().zRange(true);
		StepInterval<float> dispzrg;

		if ( SI().zIsTime() )
		{
		    dispzrg.start = sizrg.start * topvavg.start / 2;
		    dispzrg.stop = sizrg.stop * botvavg.stop / 2;
		    dispzrg.step = (dispzrg.stop-dispzrg.start)
					/ sizrg.nrSteps();
		    txt.appendPhrase( tr("Depth Range"), uiString::NoSep );
		    txt.appendPhrase( ZDomain::Depth().unitStr(true),
			    uiString::Space, uiString::OnSameLine );
		}

		else
		{
		    dispzrg.start = 2 * sizrg.start / topvavg.stop;
		    dispzrg.stop = 2 * sizrg.stop / botvavg.start;
		    dispzrg.step = (dispzrg.stop-dispzrg.start)
					/ sizrg.nrSteps();
		    dispzrg.scale( (float)ZDomain::Time().userFactor() );
		    txt.appendPhrase( tr("Time Range"), uiString::NoSep );
		    txt.appendPhrase( ZDomain::Time().unitStr(true),
			    uiString::Space, uiString::OnSameLine );
		}

		txt.appendPlainText( ": " )
		   .appendPlainText( toString(dispzrg.start) )
		   .appendPlainText( " - " )
		   .appendPlainText( toString(dispzrg.stop) );
	    }
	}
    }

    BufferString dsstr = curioobj_->pars().find( sKey::DataStorage() );
    SeisTrcTranslator* tri = (SeisTrcTranslator*)curioobj_->createTranslator();
    if ( tri && tri->initRead( new StreamConn(curioobj_->fullUserExpr(true),
				Conn::Read), Seis::Scan ) )
    {
	const BasicComponentInfo& bci = *tri->componentInfo()[0];
	const DataCharacteristics::UserType ut = bci.datachar_.userType();
	dsstr = DataCharacteristics::toString(ut);
    }
    delete tri;
    if ( dsstr.size() > 4 )
	txt.appendPhrase(tr("Storage"), uiString::NoSep).appendPlainText(": ")
		    .appendPlainText( toString(dsstr.buf() + 4) );

    const int nrcomp = oinf.nrComponents();
    if ( nrcomp > 1 )
	txt.appendPhrase( tr("Number of components"), uiString::NoSep )
		.appendPlainText(": ").appendPlainText( toString(nrcomp) );


    } // if ( oinf.isOK() )

    if ( txt.isEmpty() )
	txt = tr("<No specific info available>");
    txt.appendPhrase( mToUiStringTodo(getFileInfo()), uiString::NoSep,
						    uiString::AfterEmptyLine );

    setInfo( txt );
}


od_int64 uiSeisFileMan::getFileSize( const char* filenm, int& nrfiles ) const
{
    return SeisIOObjInfo::getFileSize( filenm, nrfiles );
}


void uiSeisFileMan::mergePush( CallBacker* )
{
    if ( !curioobj_ ) return;

    const DBKey key( curioobj_->key() );
    DBKeySet chsnmids;
    selgrp_->getChosen( chsnmids );
    uiMergeSeis dlg( this );
    dlg.setInputIds( chsnmids );
    dlg.go();
    selgrp_->fullUpdate( key );
}


void uiSeisFileMan::browsePush( CallBacker* )
{
    const BrowserDef* bdef = getBrowserDef();
    if ( bdef )
    {
	CallBack cb( bdef->cb_ );
	cb.doCall( this );
    }
}


void uiSeisFileMan::man2DPush( CallBacker* )
{
    if ( !curioobj_ ) return;

    const DBKey key( curioobj_->key() );
    uiSeis2DFileMan dlg( this, *curioobj_ );
    dlg.go();

    selgrp_->fullUpdate( key );
}


void uiSeisFileMan::copyPush( CallBacker* )
{
    if ( !curioobj_ )
	return;

    const DBKey key( curioobj_->key() );
    bool needrefresh = false;
    if ( is2d_ )
    {
	uiSeisCopy2DDataSet dlg2d( this, curioobj_ );
	needrefresh = dlg2d.go();
    }
    else
    {
	uiSeisCopyCube dlg( this, curioobj_ );
	needrefresh = dlg.go();
    }

    if ( needrefresh )
	selgrp_->fullUpdate( key );
}


void uiSeisFileMan::manPS( CallBacker* )
{
    uiSeisPreStackMan dlg( this, is2d_ );
    dlg.go();
}


void uiSeisFileMan::showHistogram( CallBacker* )
{
    const SeisIOObjInfo info( curioobj_ ); IOPar iop;
    if ( !info.getStats(iop) )
	return;

    uiStatsDisplay::Setup su;
    uiStatsDisplayWin* statswin = new uiStatsDisplayWin( this, su, 1, false );
    statswin->setDeleteOnClose( true );

    ConstRefMan<DataDistribution<float> > distrib
			= SeisStatsCollector::getDistribution( iop );
    if ( !distrib )
	statswin->statsDisplay()->usePar( iop );
    else
    {
	const Interval<float> rg = SeisStatsCollector::getExtremes( iop );
	const od_int64 nrsamples = SeisStatsCollector::getNrSamples( iop );
	statswin->statsDisplay()->setData( *distrib, nrsamples, rg );
    }

    statswin->setDataName( curioobj_->name() );
    statswin->show();
}


void uiSeisFileMan::showAttribSet( CallBacker* )
{
    if ( !curioobj_ ) return;

    File::Path fp( curioobj_->mainFileName() );
    fp.setExtension( sProcFileExtension() );
    File::launchViewer( fp.fullPath(), File::ViewPars() );
}
