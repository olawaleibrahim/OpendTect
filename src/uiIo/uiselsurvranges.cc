/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Nanne Hemstra
 Date:          June 2001
 RCS:           $Id: uiselsurvranges.cc,v 1.1 2008-02-12 11:29:50 cvsbert Exp $
________________________________________________________________________

-*/

#include "uiselsurvranges.h"
#include "survinfo.h"
#include "uispinbox.h"
#include "uilabel.h"


uiSelZRange::uiSelZRange( uiParent* p, bool wstep )
	: uiGroup(p,"Z range selection")
	, stepfld_(0)
{
    StepInterval<float> zrg( SI().zRange(false) );
    zrg.scale( SI().zFactor() );
    StepInterval<int> irg( mNINT(zrg.start), mNINT(zrg.stop), mNINT(zrg.step) );

    startfld_ = new uiSpinBox( this, 0, "Z start" );
    startfld_->setInterval( irg );
    uiLabel* lbl = new uiLabel( this, "Z Range", startfld_ );
    stopfld_ = new uiSpinBox( this, 0, "Z stop" );
    stopfld_->setInterval( irg );
    stopfld_->attach( rightOf, startfld_ );

    if ( wstep )
    {
	stepfld_ = new uiSpinBox( this, 0, "Z step" );
	stepfld_->setInterval( StepInterval<int>(irg.step,100000,irg.step) );
	lbl = new uiLabel( this, "step", stepfld_ );
	lbl->attach( rightOf, stopfld_ );
    }

    const CallBack cb( mCB(this,uiSelZRange,valChg) );
    startfld_->valueChanged.notify( cb );
    stopfld_->valueChanged.notify( cb );
    setRange( SI().zRange(true) );
    setHAlignObj( startfld_ );
}


StepInterval<float> uiSelZRange::getRange() const
{
    StepInterval<float> zrg( startfld_->getValue(), stopfld_->getValue(), 
	    		     stepfld_ ? stepfld_->getValue() : 1 );
    zrg.scale( 1 / SI().zFactor() );
    if ( !stepfld_ )
	zrg.step = SI().zRange(true).step;
    return zrg;
}


void uiSelZRange::setRange( const StepInterval<float>& inpzrg )
{
    StepInterval<float> zrg( inpzrg );
    zrg.scale( SI().zFactor() );
    StepInterval<int> irg( mNINT(zrg.start), mNINT(zrg.stop), mNINT(zrg.step) );

    startfld_->setValue( mNINT(zrg.start) );
    stopfld_->setValue( mNINT(zrg.stop) );
    if ( stepfld_ )
	stepfld_->setValue( mNINT(zrg.step) );
}


void uiSelZRange::valChg( CallBacker* cb )
{
    if ( startfld_->getValue() > stopfld_->getValue() )
    {
	const bool chgstart = cb == stopfld_;
	if ( chgstart )
	    startfld_->setValue( stopfld_->getValue() );
	else
	    stopfld_->setValue( startfld_->getValue() );
    }
}


uiSelNrRange::uiSelNrRange( uiParent* p, uiSelNrRange::Type typ, bool wstep )
	: uiGroup(p,typ == Inl ? "Inline range selection"
		 : (typ == Crl ? "Crossline range selection"
			       : "Number range selection"))
	, stepfld_(0)
	, defstep_(1)
{
    StepInterval<int> rg( 1, mUdf(int), 1 );
    StepInterval<int> wrg( rg );
    const char* nm = "Number";
    if ( typ != Gen )
    {
	const HorSampling& hs( SI().sampling(false).hrg );
	rg = typ == Inl ? hs.inlRange() : hs.crlRange();
	const HorSampling& whs( SI().sampling(true).hrg );
	wrg = typ == Inl ? whs.inlRange() : whs.crlRange();
	nm = typ == Inl ? "Inline" : "Crossline";
	defstep_ = wrg.step;
    }

    startfld_ = new uiSpinBox( this, 0, BufferString(nm," start") );
    startfld_->setInterval( rg );
    uiLabel* lbl = new uiLabel( this, BufferString(nm," range"), startfld_ );
    stopfld_ = new uiSpinBox( this, 0, BufferString(nm," stop") );
    stopfld_->setInterval( rg );
    stopfld_->attach( rightOf, startfld_ );

    if ( wstep )
    {
	stepfld_ = new uiSpinBox( this, 0, BufferString(nm," step") );
	stepfld_->setInterval( StepInterval<int>(rg.step,100000,rg.step) );
	lbl = new uiLabel( this, "step", stepfld_ );
	lbl->attach( rightOf, stopfld_ );
    }

    const CallBack cb( mCB(this,uiSelNrRange,valChg) );
    startfld_->valueChanged.notify( cb );
    stopfld_->valueChanged.notify( cb );
    setRange( wrg );
    setHAlignObj( startfld_ );
}


void uiSelNrRange::valChg( CallBacker* cb )
{
    if ( startfld_->getValue() > stopfld_->getValue() )
    {
	const bool chgstart = cb == stopfld_;
	if ( chgstart )
	    startfld_->setValue( stopfld_->getValue() );
	else
	    stopfld_->setValue( startfld_->getValue() );
    }
}


StepInterval<int> uiSelNrRange::getRange() const
{
    return StepInterval<int>( startfld_->getValue(), stopfld_->getValue(),
	    		      stepfld_ ? stepfld_->getValue() : defstep_ );
}


void uiSelNrRange::setRange( const StepInterval<int>& rg )
{
    startfld_->setValue( rg.start );
    stopfld_->setValue( rg.stop );
    if ( stepfld_ )
	stepfld_->setValue( rg.step );
}


uiSelHRange::uiSelHRange( uiParent* p, bool wstep )
    : uiGroup(p,"Hor range selection")
    , inlfld_(new uiSelNrRange(this,uiSelNrRange::Inl,wstep))
    , crlfld_(new uiSelNrRange(this,uiSelNrRange::Crl,wstep))
{
    crlfld_->attach( alignedBelow, inlfld_ );
    setHAlignObj( inlfld_ );
}


HorSampling uiSelHRange::getSampling() const
{
    HorSampling hs( false );
    hs.set( inlfld_->getRange(), crlfld_->getRange() );
    return hs;
}


void uiSelHRange::setSampling( const HorSampling& hs )
{
    inlfld_->setRange( hs.inlRange() );
    crlfld_->setRange( hs.crlRange() );
}


uiSelSubvol::uiSelSubvol( uiParent* p, bool wstep )
    : uiGroup(p,"Sub vol selection")
    , hfld_(new uiSelHRange(this,wstep))
    , zfld_(new uiSelZRange(this,wstep))
{
    zfld_->attach( alignedBelow, hfld_ );
    setHAlignObj( hfld_ );
}


CubeSampling uiSelSubvol::getSampling() const
{
    CubeSampling cs( false );
    cs.hrg = hfld_->getSampling();
    cs.zrg = zfld_->getRange();
    return cs;
}


void uiSelSubvol::setSampling( const CubeSampling& cs )
{
    hfld_->setSampling( cs.hrg );
    zfld_->setRange( cs.zrg );
}


uiSelSubline::uiSelSubline( uiParent* p, bool wstep )
    : uiGroup(p,"Sub vol selection")
    , nrfld_(new uiSelNrRange(this,uiSelNrRange::Gen,wstep))
    , zfld_(new uiSelZRange(this,wstep))
{
    zfld_->attach( alignedBelow, nrfld_ );
    setHAlignObj( nrfld_ );
}
