/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        A. Huck
 Date:          Aug 2021
________________________________________________________________________

-*/


#include "uimnemonicsel.h"

#include "uilabel.h"


uiMnemonicsSel::uiMnemonicsSel( uiParent* p, const Setup& set )
    : uiLabeledComboBox(p,set.lbltxt_,"Mnemonic")
    , setup_(set)
    , mns_(set.mnsel_)
{
    init();
}


uiMnemonicsSel::uiMnemonicsSel( uiParent* p, Mnemonic::StdType typ )
    : uiLabeledComboBox(p,Setup::defLabel(),"Mnemonic")
    , mns_(typ)
{
    init();
}


uiMnemonicsSel::uiMnemonicsSel( uiParent* p, const MnemonicSelection* mns )
    : uiLabeledComboBox(p,Setup::defLabel(),"Mnemonic")
    , mns_(nullptr)
{
    if ( mns )
	mns_ = *mns;

    init();
}


uiMnemonicsSel::~uiMnemonicsSel()
{
}


uiMnemonicsSel* uiMnemonicsSel::clone() const
{
    Setup set( setup_ );
    set.mnsel( mns_ );
    auto* ret = new uiMnemonicsSel( const_cast<uiParent*>( parent() ), setup_ );
    if ( !altnms_.isEmpty() )
	ret->setNames( altnms_ );

    return ret;
}


void uiMnemonicsSel::init()
{
    setFromSelection();
}


void uiMnemonicsSel::setFromSelection()
{
    BufferStringSet mnsnames;
    for ( const auto* mn : mns_ )
	mnsnames.add( mn->name() );
    cb_->addItems( mnsnames );
    if ( !mnsnames.isEmpty() )
	cb_->setCurrentItem( mnsnames.first()->str() );
}


const Mnemonic* uiMnemonicsSel::mnemonic() const
{
    const BufferString curnm( cb_->text() );
    if ( altnms_.isEmpty() )
	return mns_.getByName( curnm, false );

    const int idx = altnms_.indexOf( curnm );
    return mns_.validIdx( idx ) ? mns_.get( idx ) : nullptr;
}


Mnemonic::StdType uiMnemonicsSel::propType() const
{
    const Mnemonic* mn = mnemonic();
    return mn ? mn->stdType() : Mnemonic::Other;
}


void uiMnemonicsSel::setNames( const BufferStringSet& nms )
{
    if ( !nms.isEmpty() && nms.size() != cb_->size() )
	return;

    const int curitmidx = cb_->currentItem();

    altnms_ = nms;
    if ( altnms_.isEmpty() )
	setFromSelection();
    else
    {
	cb_->setEmpty();
	cb_->addItems( altnms_ );
	cb_->setCurrentItem( 0 );
    }

    if ( curitmidx >= 0 && curitmidx < cb_->size() )
	cb_->setCurrentItem( curitmidx );
}


void uiMnemonicsSel::setMnemonic( const Mnemonic& mn )
{
    if ( !mns_.isPresent(&mn) )
	return;

    const Mnemonic* curmn = mnemonic();
    if ( curmn && curmn == &mn )
	return;

    cb_->setCurrentItem( mn.name() );
}
