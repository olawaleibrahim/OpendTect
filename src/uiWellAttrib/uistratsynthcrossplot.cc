/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bert
 Date:          Oct 2010
________________________________________________________________________

-*/

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
#include "uilistbox.h"
#include "uimsg.h"
#include "syntheticdata.h"
#include "syntheticdataimpl.h"
#include "stratlevel.h"
#include "stratlayer.h"
#include "stratlayermodel.h"
#include "stratlayersequence.h"
#include "stratlayseqattrib.h"
#include "stratlayseqattribcalc.h"
#include "attribdesc.h"
#include "attribdescset.h"
#include "attribparam.h"
#include "attribengman.h"
#include "attribprocessor.h"
#include "commondefs.h"
#include "datapointset.h"
#include "posvecdataset.h"
#include "datacoldef.h"
#include "prestackattrib.h"
#include "paramsetget.h"
#include "prestackgather.h"
#include "seisbuf.h"
#include "seisbufadapters.h"
#include "seistrc.h"
#include "survinfo.h"
#include "velocitycalc.h"
#include "valseriesevent.h"
#include "od_helpids.h"

/*Adhoc arrangement for od6.0, later versions should use the issynth parameter
  in uiAttribDescSetBuild::Setup*/

static const char* unwantedattribnms[] =
{
    "Horizon",
    "Log",
    "Curvature",
    "Curvature Gradient",
    "Dip steered median filter",
    "Perpendicular dip extractor",
    "Dip",
    "Dip Angle",
    "FaultDip",
    "FingerPrint",
    "Fracture Attributes",
    "HorizonCube Data",
    "HorizonCube Density",
    "HorizonCube Dip",
    "HorizonCube Layer",
    "HorizonCube Thickness",
    "Systems Tract",
    "EW2DErrorGrid",
    "EW3DModelBuilder",
    "EWDeterministicInversion",
    "EWStochasticInversion",
    "EWUtilities",
    "Fingerprint",
    "Fracture Attributes",
    "Texture",
    "Texture - Directional",
    0
};


class uiStratSynthAttribSetBuild : public uiAttribDescSetBuild
{
public:
uiStratSynthAttribSetBuild( uiParent* p, uiAttribDescSetBuild::Setup& setup )
    : uiAttribDescSetBuild(p,setup)
{
    BufferStringSet removeattrnms( unwantedattribnms );
    for ( int idx=removeattrnms.size()-1; idx>=0; idx-- )
    {
	const char* attrnm = removeattrnms.get(idx).buf();
	if ( avfld_->isPresent(attrnm) )
	    avfld_->removeItem( attrnm );
    }
}


};


uiStratSynthCrossplot::uiStratSynthCrossplot( uiParent* p,
			const Strat::LayerModel& lm,
			const ObjectSet<SyntheticData>& synths )
    : uiDialog(p,Setup(tr("Layer model/synthetics cross-plotting"),
			mNoDlgTitle, mODHelpKey(mStratSynthCrossplotHelpID) ))
    , lm_(lm)
    , synthdatas_(synths)
{
    if ( lm.isEmpty() )
    {
	errmsg_ = tr("Input model is empty.\n"
		     "You need to generate layer models.");
	return;
    }

    TypeSet<DataPack::FullID> fids, psfids;
    for ( int idx=0; idx<synths.size(); idx++ )
    {
	const SyntheticData& sd = *synths[idx];
	if ( sd.isPS() )
	    psfids += sd.datapackid_;
	else
	    fids += sd.datapackid_;
    }
    if ( fids.isEmpty() && psfids.isEmpty() )
    {
	errmsg_ = tr("Missing or invalid 'datapacks'."
		     "\nMost likely, no synthetics are available.");
	return;
    }

    uiAttribDescSetBuild::Setup bsu( true );
    bsu.showdepthonlyattrs(false).showusingtrcpos(true).showps( psfids.size() )
       .showhidden(false).showsteering(false);
    seisattrfld_ = new uiStratSynthAttribSetBuild( this, bsu );
    seisattrfld_->setDataPackInp( fids, false );
    seisattrfld_->setDataPackInp( psfids, true );

    uiSeparator* sep = new uiSeparator( this, "sep1" );
    sep->attach( stretchedBelow, seisattrfld_ );

    layseqattrfld_ = new uiStratLaySeqAttribSetBuild( this, lm_ );
    layseqattrfld_->attach( alignedWith, seisattrfld_ );
    layseqattrfld_->attach( ensureBelow, sep );

    sep = new uiSeparator( this, "sep2" );
    sep->attach( stretchedBelow, layseqattrfld_ );

    evfld_ = new uiStratSeisEvent( this,
			uiStratSeisEvent::Setup(true).allowlayerbased(true) );
    evfld_->attach( alignedWith, layseqattrfld_ );
    evfld_->attach( ensureBelow, sep );
}


uiStratSynthCrossplot::~uiStratSynthCrossplot()
{
    detachAllNotifiers();
    deepErase( extrgates_ );
}


#define mErrRet(s) { uiMSG().error(s); delete dps; dps = 0; return dps; }
#define mpErrRet(s) { pErrMsg(s); delete dps; dps = 0; return dps; }

DataPointSet* uiStratSynthCrossplot::getData( const Attrib::DescSet& seisattrs,
					const Strat::LaySeqAttribSet& seqattrs,
					const Strat::Level& lvl,
					const Interval<float>& extrwin,
					float zstep,
					const Strat::Level* stoplvl )
{
    //If default Desc(s) present remove them
    for ( int idx=seisattrs.size()-1; idx>=0; idx-- )
    {
	const Attrib::Desc* tmpdesc = seisattrs.desc(idx);
	if ( tmpdesc && tmpdesc->isStored() && !tmpdesc->isStoredInMem() )
	    const_cast<Attrib::DescSet*>(&seisattrs)->removeDesc(tmpdesc->id());
    }

    DataPointSet* dps = seisattrs.createDataPointSet(Attrib::DescSetup(),false);
    if ( !dps )
	{ uiMSG().error(seisattrs.errMsg()); return nullptr; }

    PosVecDataSet& pvds = dps->dataSet();
    const UnitOfMeasure* depunit = PropertyRef::thickness().unit();
    if ( dps->nrCols() )
    {
	pvds.insert( dps->nrFixedCols(),
		     new DataColDef( sKey::Depth(), nullptr, depunit ) );
	pvds.insert( dps->nrFixedCols()+1,
		     new DataColDef(Strat::LayModAttribCalc::sKeyModelIdx()) );
    }
    else
    {
	pvds.add( new DataColDef( sKey::Depth(), nullptr, depunit ) );
	pvds.add( new DataColDef(Strat::LayModAttribCalc::sKeyModelIdx()) );
    }

    int iattr = 0;
    for ( const auto* seqattr : seqattrs )
    {
	const UnitOfMeasure* uom = seqattr->prop_.unit();
	pvds.add( new DataColDef(seqattr->name(),toString(iattr++),uom) );
    }

    for ( int isynth=0; isynth<synthdatas_.size(); isynth++ )
    {
	const SyntheticData& sd = *synthdatas_[isynth];
	const ObjectSet<const TimeDepthModel>& d2tmodels =
	    sd.zerooffsd2tmodels_;
	const int nrmdls = d2tmodels.size();

	mDynamicCastGet(const SeisTrcBufDataPack*,tbpack,&sd.getPack());
	if ( !tbpack ) continue;

	const SeisTrcBuf& tbuf = tbpack->trcBuf();
	if ( tbuf.size() != nrmdls )
	    mpErrRet( "DataPack nr of traces != nr of d2t models" )

	if ( isynth > 0 )
	    continue;

	const Strat::SeisEvent& ssev = evfld_->event();
	for ( int imod=0; imod<nrmdls; imod++ )
	{
	    SeisTrc& trc = const_cast<SeisTrc&>( *tbuf.get( imod ) );
	    if ( !d2tmodels[imod] )
		mpErrRet( "DataPack does not have a TD model" )

	    float dpth = lm_.sequence(imod).depthPositionOf( lvl );
	    trc.info().pick = d2tmodels[imod]->getTime( dpth );
	    const float twt = ssev.snappedTime( trc );
	    dpth = d2tmodels[imod]->getDepth( twt );

	    Interval<float> timerg;
	    if ( !extrwin.isUdf() )
	    {
		timerg.setFrom( extrwin );
		timerg.shift( twt );
	    }
	    else
		timerg.start = twt;

	    float maxdepth = mUdf(float); float maxtwt = mUdf(float);
	    if ( stoplvl )
	    {
		maxdepth = lm_.sequence(imod).depthPositionOf( *stoplvl );
		maxtwt = d2tmodels[imod]->getTime( maxdepth );
	    }

	    if ( evfld_->doAllLayers() )
	    {
		Interval<float> depthrg( dpth, mUdf(float) );
		depthrg.stop = extrwin.isUdf() ? maxdepth
			     : d2tmodels[imod]->getDepth( timerg.stop );
		fillPosFromLayerSampling( *dps, *d2tmodels[imod],
					  trc.info(), depthrg, imod );
	    }
	    else
	    {
		if ( stoplvl && extrwin.isUdf() )
		    timerg.stop = twt + zstep *
			    mCast(float, mNINT32((maxtwt-twt)/zstep) );

		fillPosFromZSampling( *dps, *d2tmodels[imod], trc.info(),
				      zstep, maxtwt, timerg );
	    }
	}
    }

    dps->dataChanged();

    if ( dps->isEmpty() )
	mErrRet( tr("No positions for data extraction") )

    if ( !seisattrs.isEmpty() && !extractSeisAttribs(*dps,seisattrs) )
	mErrRet( tr("Could not extract any seismic attribute") )

    if ( !seqattrs.isEmpty() && !extractLayerAttribs(*dps,seqattrs,stoplvl) )
	mErrRet( tr("Could not extract any layer attribute") );

    if ( !extractModelNr(*dps) )
	uiMSG().warning( tr("Could not extract the model numbers") );

    return dps;
}


void uiStratSynthCrossplot::fillPosFromZSampling( DataPointSet& dps,
					       const TimeDepthModel& d2tmodel,
					       const SeisTrcInfo& trcinfo,
					       float step, float maxtwt,
					       const Interval<float>& extrwin )
{
    if ( mIsUdf(step) )
	uiMSG().error( tr("No valid step provided for data extraction"));

    const float halfstep = step / 2.f;
    const int trcnr = trcinfo.nr;
    const Coord trcpos = trcinfo.coord;
    const int depthidx = dps.indexOf( sKey::Depth() );
    const int nrcols = dps.nrCols();
    const StepInterval<float> win( extrwin.start, extrwin.stop, step );

    TypeSet<Interval<float> >* extrgate =  new TypeSet<Interval<float> >;
    for ( int iextr=0; iextr<win.nrSteps()+1; iextr++ )
    {
	const float twt = win.atIndex( iextr );
	if ( !mIsUdf(maxtwt) && twt > maxtwt )
	    break;

	float dah = d2tmodel.getDepth( twt );
	if ( mIsUdf(dah) )
	    continue;

	if ( SI().depthsInFeet() )
	    dah *= mToFeetFactorF;

	DataPointSet::DataRow dr;
	dr.data_.setSize( nrcols, mUdf(float) );
	dr.pos_.nr_ = trcnr;
	dr.pos_.set( trcpos );
	dr.pos_.z_ = twt;
	dr.data_[depthidx] = dah;
	dps.addRow( dr );

	Interval<float> timerg( twt - halfstep, twt + halfstep );
	Interval<float> depthrg;
	depthrg.start = d2tmodel.getDepth( timerg.start );
	depthrg.stop = d2tmodel.getDepth( timerg.stop );
	*extrgate += depthrg;
    }
    extrgates_ += extrgate;
}


void uiStratSynthCrossplot::fillPosFromLayerSampling( DataPointSet& dps,
						const TimeDepthModel& d2tmodel,
						const SeisTrcInfo& trcinfo,
						const Interval<float>& extrwin,
						int iseq )
{
    Strat::LayerSequence subseq;
    lm_.sequence( iseq ).getSequencePart( extrwin, true, subseq );

    if ( subseq.isEmpty() )
	return;

    const int trcnr = trcinfo.nr;
    const Coord trcpos = trcinfo.coord;
    const int depthidx = dps.indexOf( sKey::Depth() );
    const int nrcols = dps.nrCols();
    float dah = subseq.startDepth();
    for ( int ilay=0; ilay<subseq.size(); ilay++ )
    {
	const float laythickness = subseq.layers()[ilay]->thickness();
	DataPointSet::DataRow dr;
	dr.data_.setSize( nrcols, mUdf(float) );
	dr.pos_.nr_ = trcnr;
	dr.pos_.set( trcpos );
	float depth = dah + laythickness/2.f;
	dr.pos_.z_ = d2tmodel.getTime( depth );
	if ( SI().depthsInFeet() )
	    depth *= mToFeetFactorF;

	dr.data_[depthidx] = depth;
	dps.addRow( dr );
	dah += laythickness;
    }
}


bool uiStratSynthCrossplot::extractModelNr( DataPointSet& dps ) const
{
    const int modnridx =
	dps.indexOf( Strat::LayModAttribCalc::sKeyModelIdx() );
    if ( modnridx<0 ) return false;

    for ( int dpsrid=0; dpsrid<dps.size(); dpsrid++ )
    {
	float* dpsvals = dps.getValues( dpsrid );
	dpsvals[modnridx] = mCast(float,dps.trcNr(dpsrid));
    }

    return true;
}


void uiStratSynthCrossplot::preparePreStackDescs()
{
    TypeSet<DataPack::FullID> sddpfids;
    for ( int idx=0; idx<synthdatas_.size(); idx++ )
    {
	const SyntheticData* sd = synthdatas_[idx];
	sddpfids += sd->datapackid_;
    }

    Attrib::DescSet* ds =
	const_cast<Attrib::DescSet*>( &seisattrfld_->descSet() );
    for ( int dscidx=0; dscidx<ds->size(); dscidx++ )
    {
	Attrib::Desc& desc = (*ds)[dscidx];
	if ( !desc.isPS() )
	    continue;

	mDynamicCastGet(Attrib::EnumParam*,gathertypeparam,
			desc.getValParam(Attrib::PSAttrib::gathertypeStr()))
	if ( gathertypeparam->getIntValue()==(int)(Attrib::PSAttrib::Ang) )
	{
	    mDynamicCastGet(Attrib::BoolParam*,useangleparam,
			    desc.getValParam(Attrib::PSAttrib::useangleStr()))
	    useangleparam->setValue( true );
	    uiString errmsg;
	    Attrib::Provider* attrib = Attrib::Provider::create( desc, errmsg );
	    mDynamicCastGet(Attrib::PSAttrib*,psattrib,attrib);
	    if ( !psattrib )
		continue;

	    BufferString inputpsidstr( psattrib->psID() );
	    const char* dpbuf = inputpsidstr.buf();
	    dpbuf++;
	    inputpsidstr = dpbuf;
	    DataPack::FullID inputdpid( inputpsidstr );

	    int inpsdidx = sddpfids.indexOf( inputdpid );
	    if ( inpsdidx<0 || !synthdatas_.validIdx(inpsdidx) )
		continue;

	    mDynamicCastGet(const PreStackSyntheticData*,pssd,
			    synthdatas_[inpsdidx])
	    if ( !pssd )
		continue;

	    mDynamicCastGet(Attrib::IntParam*,angledpparam,
			    desc.getValParam(Attrib::PSAttrib::angleDPIDStr()))
	    angledpparam->setValue( pssd->angleData().id() );
	}
    }
}


bool uiStratSynthCrossplot::extractSeisAttribs( DataPointSet& dps,
						const Attrib::DescSet& attrs )
{
    preparePreStackDescs();

    uiString errmsg;
    PtrMan<Attrib::EngineMan> aem = createEngineMan( attrs );
    PtrMan<Executor> exec = aem->getTableExtractor(dps,attrs,errmsg,2,false);
    if ( !exec )
    {
	uiMSG().error( errmsg );
	return false;
    }

    exec->setName( "Attributes from Traces" );
    uiTaskRunner dlg( this );
    TaskRunner::execute( &dlg, *exec );
    return true;
}


bool uiStratSynthCrossplot::extractLayerAttribs( DataPointSet& dps,
				     const Strat::LaySeqAttribSet& seqattrs,
				     const Strat::Level* stoplvl )
{
    Strat::LayModAttribCalc lmac( lm_, seqattrs, dps );
    lmac.setExtrGates( extrgates_, stoplvl );
    uiTaskRunner taskrunner( this );
    return TaskRunner::execute( &taskrunner, lmac );
}


void uiStratSynthCrossplot::launchCrossPlot( const DataPointSet& dps,
					     const Strat::Level& lvl,
					     const Strat::Level* stoplvl,
					     const Interval<float>& extrwin,
					     float zstep )
{
    StepInterval<float> winms( extrwin.start, extrwin.stop, zstep );
    winms.scale( 1000.f );
    const bool layerbased = mIsUdf( zstep );
    const bool multiz = ( !extrwin.isUdf() && winms.nrSteps() > 1 )
			|| stoplvl || layerbased;
    const bool shiftedsingez = !extrwin.isUdf() && !layerbased &&
			       !mIsZero(extrwin.start,1e-6f);

    uiString wintitl = uiStrings::sAttribute(mPlural);
    uiString timegate = tr("[%1-%2]ms").arg(toUiString(winms.start,0))
				       .arg(toUiString(winms.stop,0));

    if ( !multiz )
    {
	if ( !shiftedsingez )
	    wintitl = tr( "%1 at" ).arg(wintitl);
	else
	{
	    wintitl = tr("%1 %2ms %3").arg(wintitl)
				      .arg(toUiString(fabs( winms.start )))
				      .arg(winms.start < 0 ?
				      uiStrings::sAbove().toLower() :
				      uiStrings::sBelow().toLower());
	}
    }
    else if ( !layerbased && !extrwin.isUdf() )
    {
	wintitl = tr("%1 in a time gate %2 relative to").arg(wintitl)
							.arg(timegate);
    }
    else
    {
	wintitl = tr("%1 between").arg(wintitl);
    }

    wintitl = toUiString("%1 %2").arg(wintitl).arg(lvl.name());
    if ( stoplvl )
    {
	wintitl = toUiString("%1 %2").arg(wintitl)
					.arg(!extrwin.isUdf() && !layerbased ?
					tr("and down to") : tr("and"));
	wintitl = toUiString("%1 %2").arg(wintitl)
					      .arg(stoplvl->name());
    }

    if ( multiz && !layerbased )
    {
	wintitl = tr( "%1 each %2ms").arg(wintitl).arg(winms.step);
    }
    else if ( layerbased && !extrwin.isUdf() )
    {
	wintitl = tr("%1 within %2").arg(wintitl).arg( timegate );
    }

    if ( layerbased )
	wintitl = tr("%1 - layer-based extraction").arg(wintitl);

    uiDataPointSet::Setup su( wintitl, false );
    uiDataPointSet* uidps = new uiDataPointSet( this, dps, su, 0 );
    uidps->showXY( false );
    Attrib::DescSet* ds =
	const_cast<Attrib::DescSet*>( &seisattrfld_->descSet() );
    ds->removeUnused( false, true );


    seisattrfld_->descSet().fillPar( uidps->storePars() );
    uidps->show();
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


bool uiStratSynthCrossplot::handleUnsaved()
{
    return seisattrfld_->handleUnsaved() && layseqattrfld_->handleUnsaved();
}


bool uiStratSynthCrossplot::rejectOK( CallBacker* )
{
    return handleUnsaved();
}

#undef mErrRet
#define mErrRet(s) { if ( !s.isEmpty() ) uiMSG().error(s); return false; }

bool uiStratSynthCrossplot::acceptOK( CallBacker* )
{
    if ( !errmsg_.isEmpty() )
	return true;

    const Attrib::DescSet& seisattrs = seisattrfld_->descSet();
    const Strat::LaySeqAttribSet& seqattrs = layseqattrfld_->attribSet();
    if ( !evfld_->getFromScreen() )
	mErrRet(uiStrings::sEmptyString())

    deepErase( extrgates_ );
    const Interval<float>& extrwin = evfld_->hasExtrWin()
				   ? evfld_->event().extrWin()
				   : Interval<float>::udf();
    const float zstep = evfld_->hasStep() ? evfld_->event().extrStep()
					  : mUdf(float);
    const Strat::Level& lvl = *evfld_->event().level();
    const Strat::Level* stoplvl = evfld_->event().downToLevel();
    DataPointSet* dps = getData( seisattrs, seqattrs, lvl, extrwin, zstep,
				 stoplvl );
    if ( !dps )
	return false;

    dps->setName( "Layer model" );
    DPM(DataPackMgr::PointID()).addAndObtain( dps );
    launchCrossPlot( *dps, lvl, stoplvl, extrwin, zstep );
    DPM(DataPackMgr::PointID()).release( dps->id() );
    return false;
}
