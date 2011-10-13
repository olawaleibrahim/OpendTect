/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bert
 Date:          Oct 2010
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uistratsynthcrossplot.cc,v 1.36 2011-10-13 07:25:59 cvsbruno Exp $";

#include "uistratsynthcrossplot.h"
#include "uistratsynthdisp.h"
#include "uistratlayseqattrsetbuild.h"
#include "uistratseisevent.h"
#include "uiattribsetbuild.h"
#include "uidatapointset.h"
#include "uiseparator.h"
#include "uitaskrunner.h"
#include "uilabel.h"
#include "uicombobox.h"
#include "uigeninput.h"
#include "uimsg.h"
#include "stratlevel.h"
#include "stratlayermodel.h"
#include "stratlayersequence.h"
#include "stratlayseqattrib.h"
#include "stratlayseqattribcalc.h"
#include "attribdesc.h"
#include "attribdescset.h"
#include "attribengman.h"
#include "attribprocessor.h"
#include "datapointset.h"
#include "posvecdataset.h"
#include "datacoldef.h"
#include "prestackgather.h"
#include "seisbuf.h"
#include "seisbufadapters.h"
#include "seistrc.h"
#include "survinfo.h"
#include "velocitycalc.h"
#include "valseriesevent.h"



uiStratSynthCrossplot::uiStratSynthCrossplot( uiParent* p,
			const Strat::LayerModel& lm,
			const ObjectSet<SyntheticData>& synths)
    : uiDialog(p,Setup("Layer model/synthetics cross-plotting",
			mNoDlgTitle,mTODOHelpID))
    , lm_(lm)
    , synthdatas_(synths)	     
{
    if ( lm.isEmpty() )
	{ errmsg_ = "Model is empty"; return;}

    TypeSet<DataPack::FullID> fids, psfids; 
    for ( int idx=0; idx<synths.size(); idx++ )
    {
	const SyntheticData& sd = *synths[idx];
	if ( !sd.getPack( false ) || !sd.getPack( true ) ) 
	    continue;

	psfids += sd.prestackpackid_; 
	fids += sd.poststackpackid_; 
    }
    if ( fids.isEmpty() && psfids.isEmpty() )
	{ errmsg_ = "Missing or invalid datapacks"; return;}

    uiAttribDescSetBuild::Setup bsu( true );
    bsu.showdepthonlyattrs(false).showusingtrcpos(true).showps( psfids.size() );
    seisattrfld_ = new uiAttribDescSetBuild( this, bsu );
    seisattrfld_->setDataPackInp( fids, false );
    seisattrfld_->setDataPackInp( psfids, true );

    uiSeparator* sep = new uiSeparator( this, "sep1", true );
    sep->attach( stretchedBelow, seisattrfld_ );

    layseqattrfld_ = new uiStratLaySeqAttribSetBuild( this, lm_ );
    layseqattrfld_->attach( alignedWith, seisattrfld_ );
    layseqattrfld_->attach( ensureBelow, sep );

    sep = new uiSeparator( this, "sep2" );
    sep->attach( stretchedBelow, layseqattrfld_ );

    evfld_ = new uiStratSeisEvent( this, uiStratSeisEvent::Setup(true) );
    evfld_->attach( alignedWith, layseqattrfld_ );
    evfld_->attach( ensureBelow, sep );
}


DataPointSet* uiStratSynthCrossplot::getData( const Attrib::DescSet& seisattrs,
					const Strat::LaySeqAttribSet& seqattrs,
					const Strat::Level& lvl,
					const StepInterval<float>& extrwin )
{
    //If default Desc(s) present remove it
    for ( int idx=seisattrs.size()-1; idx>=0; idx-- )
    {
	const Attrib::Desc* tmpdesc = seisattrs.desc(idx);
	if ( tmpdesc && tmpdesc->isStored() && !tmpdesc->isStoredInMem() )
	    const_cast<Attrib::DescSet*>(&seisattrs)->removeDesc(tmpdesc->id());
    }

    DataPointSet* dps = seisattrs.createDataPointSet(Attrib::DescSetup());
    if ( !dps )
	{ uiMSG().error(seisattrs.errMsg()); return false; }
    dps->dataSet().add( new DataColDef(sKey::Depth) );
    const int depthcol = dps->nrCols() - 1;
    for ( int iattr=0; iattr<seqattrs.size(); iattr++ )
	dps->dataSet().add(
		new DataColDef(seqattrs.attr(iattr).name(),toString(iattr)) );

    for ( int idx=0; idx<synthdatas_.size(); idx++ )
    {
	const SyntheticData& sd = *synthdatas_[idx];
	const ObjectSet<const TimeDepthModel>& d2tmodels = sd.d2tmodels_;
	const int nrmdls = d2tmodels.size();

	mDynamicCastGet(const SeisTrcBufDataPack*,tbpack,sd.getPack(false));
	if ( !tbpack ) continue;

	const SeisTrcBuf& tbuf = tbpack->trcBuf();
	if ( tbuf.size() != nrmdls )
	    { pErrMsg("DataPack nr of traces != nr of d2t models"); return 0; }

	TypeSet<float> lvltms;
	const Strat::SeisEvent& ssev = evfld_->event();
	for ( int imod=0; imod<nrmdls; imod++ )
	{
	    SeisTrc& trc = const_cast<SeisTrc&>( *tbuf.get( imod ) );
	    const float dpth = lm_.sequence(imod).depthOf( lvl );
	    trc.info().pick = d2tmodels[imod]->getTime( dpth );
	    lvltms += ssev.snappedTime( trc );
	}

	const int nrextr = extrwin.nrSteps() + 1;
	for ( int iextr=0; iextr<nrextr; iextr++ )
	{
	    const float relz = extrwin.atIndex( iextr );
	    for ( int itrc=0; itrc<nrmdls; itrc++ )
	    {
		const SeisTrc& trc = *tbuf.get( itrc );
		DataPointSet::DataRow dr;
		dr.pos_.nr_ = itrc+1;
		dr.pos_.set( trc.info().coord );
		dr.pos_.z_ = lvltms[itrc] + relz;
		dr.data_.setSize( dps->nrCols(), mUdf(float) );
		dr.data_[depthcol] = d2tmodels[itrc]->getDepth( dr.pos_.z_ );
		dps->addRow( dr );
	    }
	}

	mDynamicCastGet(const PreStack::GatherSetDataPack*,
				pspack,sd.getPack(true));
	if ( !pspack ) continue;

	lvltms.erase();
	for ( int imod=0; imod<nrmdls; imod++ )
	{
	    const PreStack::Gather& gather = *pspack->getGathers()[imod];
	    const float dpth = lm_.sequence(imod).depthOf( lvl );
	    lvltms += d2tmodels[imod]->getTime( dpth );//TODO snapp
	}

	for ( int iextr=0; iextr<nrextr; iextr++ )
	{
	    const float relz = extrwin.atIndex( iextr );
	    for ( int itrc=0; itrc<pspack->getGathers().size(); itrc++ )
	    {
		const PreStack::Gather& gather = *pspack->getGathers()[itrc];
		DataPointSet::DataRow dr;
		dr.pos_.nr_ = itrc+1;
		dr.pos_.set( gather.getCoord() );
		dr.pos_.z_ = lvltms[itrc] + relz;
		dr.data_.setSize( dps->nrCols(), mUdf(float) );
		dr.data_[depthcol] = d2tmodels[itrc]->getDepth( dr.pos_.z_ );
		dps->addRow( dr );
	    }
	}
    }

    dps->dataChanged();

    if ( dps->isEmpty() )
    {
	uiMSG().error( "No positions for data extraction" );
	delete dps; dps = 0;
    }
    else if ( !extractSeisAttribs(*dps,seisattrs)
	    || !extractLayerAttribs(*dps,seqattrs) )
	{ delete dps; dps = 0; }

    return dps;
}


bool uiStratSynthCrossplot::extractSeisAttribs( DataPointSet& dps,
						const Attrib::DescSet& attrs )
{
    BufferString errmsg;                                                        
    PtrMan<Attrib::EngineMan> aem = createEngineMan( attrs );

    PtrMan<Executor> exec = aem->getTableExtractor( dps, attrs, errmsg,0,false);
    if ( !exec )                                                                
    {                                                                           
	uiMSG().error( errmsg );                                                
	return false;
    }

    exec->setName( "Attributes from Traces" );                       
    uiTaskRunner dlg( this );                                                   
    dlg.execute(*exec);
    return true;
}


bool uiStratSynthCrossplot::extractLayerAttribs( DataPointSet& dps,
				     const Strat::LaySeqAttribSet& seqattrs )
{
    Strat::LayModAttribCalc lmac( lm_, seqattrs, dps );
    uiTaskRunner tr( this );
    return tr.execute( lmac );
}


bool uiStratSynthCrossplot::launchCrossPlot( const DataPointSet& dps,
					const Strat::Level& lvl,
					const StepInterval<float>& extrwin )
{
    BufferString wintitl( "Attributes at ", lvl.name() );
    wintitl.add( " [" ).add( extrwin.start )
	   .add( "-" ).add( extrwin.stop ).add( "]" );
    uiDataPointSet::Setup su( wintitl, false );
    uiDataPointSet* uidps = new uiDataPointSet( this, dps, su, 0 );
    seisattrfld_->descSet().fillPar( uidps->storePars() );
    uidps->setDeleteOnClose( true );
    uidps->show();
    return false;
}


Attrib::EngineMan* uiStratSynthCrossplot::createEngineMan(
					    const Attrib::DescSet& attrs ) const
{                                                                               
    Attrib::EngineMan* aem = new Attrib::EngineMan;

    TypeSet<Attrib::SelSpec> attribspecs;
    attrs.fillInSelSpecs( Attrib::DescSetup().hidden(false), attribspecs );

    aem->setAttribSet( &attrs ); 
    aem->setAttribSpecs( attribspecs );                                         
    return aem;                                                                 
}


void uiStratSynthCrossplot::setRefLevel( const char* lvlnm )
{
    evfld_->setLevel( lvlnm );
}


#define mErrRet(s) { if ( s ) uiMSG().error(s); return false; }

bool uiStratSynthCrossplot::acceptOK( CallBacker* )
{
    if ( !errmsg_.isEmpty() )
	return true;

    const Attrib::DescSet& seisattrs = seisattrfld_->descSet();
    if ( seisattrs.isEmpty() )
	mErrRet("Please define a seismic attribute")
    const Strat::LaySeqAttribSet& seqattrs = layseqattrfld_->attribSet();
    if ( seqattrs.isEmpty() )
	mErrRet("Please define a layer attribute")
    if ( !evfld_->getFromScreen() )
	mErrRet(0)

    const StepInterval<float>& extrwin = evfld_->event().extrwin_;
    const Strat::Level& lvl = *evfld_->event().level_;
    DataPointSet* dps = getData( seisattrs, seqattrs, lvl, extrwin );
    return dps ? launchCrossPlot( *dps, lvl, extrwin ) : false;
}
