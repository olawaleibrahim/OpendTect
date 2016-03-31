/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Satyaki Maitra
 Date:          June 2008
________________________________________________________________________

-*/

#include "uistoredattrreplacer.h"

#include "attribdesc.h"
#include "attribdescset.h"
#include "attribparam.h"
#include "attribstorprovider.h"
#include "bufstringset.h"
#include "iopar.h"
#include "linekey.h"
#include "varlenarray.h"
#include <string.h>

#include "uiattrinpdlg.h"
#include "uimsg.h"


using namespace Attrib;

uiStoredAttribReplacer::uiStoredAttribReplacer( uiParent* parent,
						DescSet* attrset )
    : is2d_(attrset->is2D())
    , attrset_(attrset)
    , iopar_(0)
    , parent_(parent)
    , noofseis_(0)
    , noofsteer_(0)
{
    getStoredIds();

    for ( int idx=0; idx<storedids_.size(); idx++ )
    {
	if ( storedids_[idx].has2Ids() )
	    noofsteer_++;
	else
	    noofseis_++;
    }
}


uiStoredAttribReplacer::uiStoredAttribReplacer( uiParent* parent,
						IOPar* iopar, bool is2d )
    : is2d_(is2d)
    , attrset_(0)
    , iopar_(iopar)
    , parent_(parent)
    , noofseis_(0)
    , noofsteer_(0)
{
    usePar( *iopar );

    for ( int idx=0; idx<storedids_.size(); idx++ )
    {
	if ( storedids_[idx].has2Ids() )
	    noofsteer_++;
	else
	    noofseis_++;
    }
}


void uiStoredAttribReplacer::getUserRefs( const IOPar& iopar )
{
    for ( int idx=0; idx<iopar.size(); idx++ )
    {
	IOPar* descpar = iopar.subselect( idx );
	if ( !descpar ) continue;
	const char* defstring = descpar->find( "Definition" );
	if ( !defstring ) continue;
	BufferString useref;
	descpar->get( "UserRef", useref );
	int inpidx=0;
	while ( true )
	{
	    const char* key = IOPar::compKey( sKey::Input(), inpidx );
	    int descidx=-1;
	    if ( !descpar->get(key,descidx) ) break;
	    if ( descidx < 0 ) continue;
	    DescID descid( descidx, false );
	    for ( int stridx=0; stridx<storedids_.size(); stridx++ )
	    {
		if ( (storedids_[stridx].firstid_ == descid) ||
		     (storedids_[stridx].secondid_ == descid) )
		    storedids_[stridx].userrefs_.addIfNew( useref );
	    }
	    inpidx++;
	}
    }
}


void uiStoredAttribReplacer::getStoredIds( const IOPar& iopar )
{
    BufferStringSet storageidstr;
    for ( int idx=0; idx<iopar.size(); idx++ )
    {
	IOPar* descpar = iopar.subselect( idx );
	if ( !descpar ) continue;
	const char* defstring = descpar->find(
					Attrib::DescSet::definitionStr() );
	if ( !defstring ) continue;
	BufferString attribnm;
	Attrib::Desc::getAttribName( defstring, attribnm );
	if ( attribnm != "Storage" ) continue;

	const int len = strLength(defstring);
	int spacepos = 0; int equalpos = 0;
	for ( int stridx=0; stridx<len; stridx++ )
	{
	    if ( defstring[stridx] != '=')
		continue;

	    equalpos = stridx+1;
	    spacepos = stridx+2;
	    while ( spacepos<len && !iswspace(defstring[spacepos]) )
		spacepos++;
	    mAllocVarLenArr( char, storagestr, spacepos-equalpos+1 );
	    strncpy( storagestr, &defstring[equalpos], spacepos-equalpos);
	    storagestr[spacepos-equalpos] = 0;
	    if ( storageidstr.addIfNew(storagestr) )
	    {
		const char* storedref = descpar->find(
						Attrib::DescSet::userRefStr() );
		storedids_ += StoredEntry( DescID(idx,false),
					   LineKey(storagestr), storedref );
	    }
	    else
	    {
		for ( int idy=0; idy<storedids_.size(); idy++ )
		{
		    LineKey lk( storagestr );
		    if ( lk == storedids_[idy].lk_ )
		    {
			int outprevlisted =
				getOutPut(storedids_[idy].firstid_.asInt());
			int outnowlisted = getOutPut(idx);
			if ( outnowlisted != outprevlisted )
			    storedids_[idy].secondid_ = DescID( idx, false );

			break;
		    }
		}
	    }
	    break;
	}
    }
}


void uiStoredAttribReplacer::usePar( const IOPar& iopar )
{
    getStoredIds( iopar );
    getUserRefs( iopar );
}


static bool hasSpace( const char* txt )
{
    const int sz = strLength( txt );
    for ( int idx=0; idx<sz; idx++ )
	if ( iswspace(txt[idx]) )
	    return true;

    return false;
}


void uiStoredAttribReplacer::setStoredKey( IOPar* par, const char* key )
{
    if ( !par || !key ) return;
    const char* defstring = par->find( "Definition" );
    BufferString defstr( defstring );
    char* maindefstr = defstr.find( "id=" );
    if ( !maindefstr )
	return;

    maindefstr += 3;
    BufferString tempstr( maindefstr );
    char* spaceptr = tempstr.find( ' ' );
    BufferString finalpartstr( spaceptr );
    *maindefstr = '\0';
    const bool usequotes = hasSpace( key );
    if ( usequotes ) defstr += "\"";
    defstr += key;
    if ( usequotes ) defstr += "\"";
    defstr += finalpartstr;

    par->set( "Definition", defstr );
}


int uiStoredAttribReplacer::getOutPut( int descid )
{
    IOPar* descpar = iopar_->subselect( descid );
    if ( !descpar ) return -1;

    const char* defstring = descpar->find( "Definition" );
    BufferString defstr( defstring );

    char* outputptr = defstr.find( "output=" );
    outputptr +=7;
    BufferString outputstr( outputptr );
    char* spaceptr = outputstr.find( ' ' );
    if ( spaceptr )
	*spaceptr ='\0';
    return outputstr.toInt();
}

void uiStoredAttribReplacer::setSteerPar( StoredEntry storeentry,
					  const char* key, const char* userref )
{
    if ( !key || !userref )
    {
	uiMSG().error( tr("No valid steering input selected") );
	return;
    }

    const int output = getOutPut( storeentry.firstid_.asInt() );
    if ( output==0 )
    {
	IOPar* inlpar = iopar_->subselect( storeentry.firstid_.asInt() );
	setStoredKey( inlpar, key );
	BufferString steerref = userref;
	steerref += "_inline_dip";
	inlpar->set( "UserRef", steerref );
	BufferString inlidstr; inlidstr+= storeentry.firstid_.asInt();
	iopar_->mergeComp( *inlpar, inlidstr );

	IOPar* crlpar = iopar_->subselect( storeentry.secondid_.asInt());
	setStoredKey( crlpar, key );
	steerref = userref;
	steerref += "_crline_dip";
	crlpar->set( "UserRef", steerref );
	BufferString crlidstr; crlidstr+= storeentry.secondid_.asInt();
	iopar_->mergeComp( *crlpar, crlidstr );
    }
    else
    {
	IOPar* inlpar = iopar_->subselect( storeentry.secondid_.asInt());
	setStoredKey( inlpar, key );
	BufferString steerref = userref;
	steerref += "_inline_dip";
	inlpar->set( "UserRef", steerref );
	BufferString inlidstr; inlidstr+= storeentry.secondid_.asInt();
	iopar_->mergeComp( *inlpar, inlidstr );

	IOPar* crlpar = iopar_->subselect( storeentry.firstid_.asInt() );
	setStoredKey( crlpar, key );
	steerref = userref;
	steerref += "_crline_dip";
	crlpar->set( "UserRef", steerref );
	BufferString crlidstr; crlidstr+= storeentry.firstid_.asInt();
	iopar_->mergeComp( *crlpar, crlidstr );
    }
}


void uiStoredAttribReplacer::go()
{
    handleOneGoInputRepl();
    if ( attrset_ ) attrset_->removeUnused( true, false );
}


void uiStoredAttribReplacer::handleOneGoInputRepl()
{
    if ( storedids_.isEmpty() ) return;

    BufferStringSet seisinprefs;
    TypeSet<int> seisinpidx;
    BufferStringSet steerinprefs;
    TypeSet<int> steerinpidx;
    for ( int idx=0; idx<storedids_.size(); idx++ )
    {
	if ( storedids_[idx].has2Ids() )
	{
	    steerinprefs.add( storedids_[idx].storedref_ );
	    steerinpidx += idx;
	}
	else
	{
	    seisinprefs.add( storedids_[idx].storedref_ );
	    seisinpidx += idx;
	}
    }

    uiAttrInpDlg dlg( parent_, seisinprefs, steerinprefs, is2d_ );
    if ( !dlg.go() )
    {
	if ( attrset_ ) attrset_->removeAll( true );
	return;
    }
    else if ( attrset_ )
    {
	Desc* ad = attrset_->getDesc( storedids_[0].firstid_ );
	if ( !ad )
	{
	    uiMSG().error( tr("Cannot replace stored entries") );
	    return;
	}
    }

    for ( int idx=0; idx<seisinprefs.size(); idx++ )
    {
	if ( attrset_ )
	{
	    Desc* ad = attrset_->getDesc( storedids_[seisinpidx[idx]].firstid_);
	    ad->changeStoredID( dlg.getSeisKey(idx) );
	    ad->setUserRef( dlg.getSeisRef(idx) );
	}
	else
	{
	    if ( !iopar_ ) return;
	    IOPar* descpar = iopar_->subselect(
				storedids_[seisinpidx[idx]].firstid_.asInt() );
	    setStoredKey( descpar, dlg.getSeisKey(idx) );
	    descpar->set( "UserRef", dlg.getSeisRef(idx) );
	    BufferString idstr;
	    idstr+= storedids_[seisinpidx[idx]].firstid_.asInt();
	    iopar_->mergeComp( *descpar, idstr );
	}
    }

    for ( int idx=0; idx<steerinprefs.size(); idx++ )
    {
	if ( attrset_ )
	{
	    StoredEntry storeentry = storedids_[steerinpidx[idx]];
	    const int ouputidx = attrset_->getDesc(
		DescID( storeentry.firstid_.asInt(), false ))->selectedOutput();
	    Desc* adsteerinl = 0;
	    Desc* adsteercrl = 0;
	    if ( ouputidx == 0 )
	    {
		adsteerinl = attrset_->getDesc(
			DescID(storeentry.firstid_.asInt(),false) );
		adsteercrl = attrset_->getDesc(
			DescID(storeentry.secondid_.asInt(),false) );
	    }
	    else
	    {
		adsteerinl = attrset_->getDesc(
			DescID(storeentry.secondid_.asInt(),false));
		adsteercrl = attrset_->getDesc(
			DescID(storeentry.firstid_.asInt(),false));
	    }

	    if ( adsteerinl && adsteercrl )
	    {
		adsteerinl->changeStoredID( dlg.getSteerKey(idx) );
		BufferString bfstr = dlg.getSteerRef(idx);
		bfstr += "_inline_dip";
		adsteerinl->setUserRef( bfstr.buf() );

		adsteercrl->changeStoredID( dlg.getSteerKey(idx) );
		bfstr = dlg.getSteerRef(idx);
		bfstr += "_crline_dip";
		adsteercrl->setUserRef( bfstr.buf() );
	    }
	}
	else
	    setSteerPar( storedids_[steerinpidx[idx]], dlg.getSteerKey(idx),
			 dlg.getSteerRef(idx) );
    }
}


//Not sure we ever want to use it?
void uiStoredAttribReplacer::handleMultiInput()
{
    BufferStringSet usrrefs;
    for ( int idnr=0; idnr<storedids_.size(); idnr++ )
    {
	usrrefs.erase();
	StoredEntry storeentry = storedids_[idnr];
	const DescID storedid = storeentry.firstid_;
	const bool issteer = storeentry.has2Ids();

	getUserRef( storedid, usrrefs );
	if ( usrrefs.isEmpty() )
	    continue;

	BufferString prevref = storeentry.storedref_;
	if ( stringEndsWith( "_inline_dip", prevref.buf() ) )
	{
	    int newsz = prevref.size() - strLength("_inline_dip");
	    mDeclareAndTryAlloc( char*, tmpbuf, char[newsz+1] );
	    if ( tmpbuf )
	    {
		OD::memCopy( tmpbuf, prevref.buf(), newsz );
		tmpbuf[newsz]='\0';
		prevref.setEmpty();
		prevref = tmpbuf;
	    }
	}

	uiAttrInpDlg dlg( parent_, usrrefs, issteer, is2d_, prevref.buf() );
	if ( !dlg.go() )
	{
	    if ( attrset_ ) attrset_->removeAll( true );
	    return;
	}

	if ( !issteer )
	{
	    if ( attrset_ )
	    {
		Desc* ad = attrset_->getDesc( storedid );
		BufferString seisref = dlg.getSeisRef(0);
		if ( seisref.isEmpty() )
		    removeDescsWithBlankInp( storedid );
		else
		{
		    ad->changeStoredID( dlg.getSeisKey(0) );
		    ad->setUserRef( seisref );
		}
	    }
	    else
	    {
		if ( !iopar_ ) return;
		IOPar* descpar = iopar_->subselect( storedid.asInt() );
		setStoredKey( descpar, dlg.getSeisKey(0) );
		descpar->set( "UserRef", dlg.getSeisRef(0) );
		BufferString idstr; idstr+= storedid.asInt();
		iopar_->mergeComp( *descpar, idstr );
	    }
	}
	else
	{
	    if ( attrset_ )
	    {
		BufferString steerref = dlg.getSteerRef(0);
		if ( steerref.isEmpty() )
		{
		    removeDescsWithBlankInp( storedid );
		    continue;
		}

		const int ouputidx = attrset_->getDesc(
			DescID(storeentry.firstid_.asInt(),false))->
			    selectedOutput();
		Desc* adsteerinl = 0;
		Desc* adsteercrl = 0;
		if ( ouputidx == 0 )
		{
		    adsteerinl = attrset_->getDesc(
			    DescID(storeentry.firstid_.asInt(),false) );
		    adsteercrl = attrset_->getDesc(
			    DescID(storeentry.secondid_.asInt(),false) );
		}
		else
		{
		    adsteerinl = attrset_->getDesc(
			    DescID(storeentry.secondid_.asInt(),false));
		    adsteercrl = attrset_->getDesc(
			    DescID(storeentry.firstid_.asInt(),false));
		}

		if ( adsteerinl && adsteercrl )
		{
		    adsteerinl->changeStoredID( dlg.getSteerKey(0) );
		    BufferString bfstr = dlg.getSteerRef(0);
		    bfstr += "_inline_dip";
		    adsteerinl->setUserRef( bfstr.buf() );

		    adsteercrl->changeStoredID( dlg.getSteerKey(0) );
		    bfstr = dlg.getSteerRef(0);
		    bfstr += "_crline_dip";
		    adsteercrl->setUserRef( bfstr.buf() );
		}
	    }
	    else
		setSteerPar( storeentry, dlg.getSteerKey(0),
			     dlg.getSteerRef(0) );
	}
    }

    if ( attrset_ )
	attrset_->cleanUpDescsMissingInputs();
}



void uiStoredAttribReplacer::getUserRef( const DescID& storedid,
					 BufferStringSet& usrref ) const
{
    if ( attrset_ )
    {
	for ( int idx=0; idx<attrset_->size(); idx++ )
	{
	    const DescID descid = attrset_->getID( idx );
	    Desc* ad = attrset_->getDesc( descid );
	    if ( !ad || ad->isStored() || ad->isHidden() ) continue;

	    if ( hasInput(*ad,storedid) )
		usrref.addIfNew( ad->userRef() );
	}
    }
    else
    {
	for ( int idx=0; idx<storedids_.size(); idx++ )
	{
	    if ( storedids_[idx].firstid_ == storedid ||
		 storedids_[idx].secondid_ == storedid )
		usrref = storedids_[idx].userrefs_;
	}
    }
}


void uiStoredAttribReplacer::getStoredIds()
{
    TypeSet<LineKey> linekeys;
    for ( int idx=0; idx<attrset_->size(); idx++ )
    {
	const DescID descid = attrset_->getID( idx );
	Desc* ad = attrset_->getDesc( descid );
        if ( !ad || !ad->isStored() ) continue;

	const ValParam* keypar = ad->getValParam( StorageProvider::keyStr() );
	LineKey lk( keypar->getStringValue() );
	if ( !linekeys.addIfNew(lk) )
	{
	    for ( int idy=0; idy<storedids_.size(); idy++ )
	    {
		if ( lk == storedids_[idy].lk_ )
		{
		    int outprevlisted = attrset_->getDesc(
				storedids_[idy].firstid_)->selectedOutput();
		    int outnowlisted = ad->selectedOutput();
		    if ( outnowlisted != outprevlisted )
			storedids_[idy].secondid_ = descid;

		    break;
		}
	    }
	}
	else
	    storedids_ += StoredEntry( descid, lk, ad->userRef() );
    }
}


bool uiStoredAttribReplacer::hasInput( const Desc& desc,
				       const DescID& id ) const
{
    for ( int idx=0; idx<desc.nrInputs(); idx++ )
    {
	const Desc* inp = desc.getInput( idx );
	if ( !inp )
	{
	    if ( desc.inputSpec(idx).enabled_ )
		return false;
	    else
		continue;
	}

	if ( inp->id() == id ) return true;
    }

    for ( int idx=0; idx<desc.nrInputs(); idx++ )
    {
	const Desc* inp = desc.getInput( idx );
	if ( inp && inp->isHidden() && inp->nrInputs() )
	return hasInput( *inp, id );
    }
    return false;
}


void uiStoredAttribReplacer::removeDescsWithBlankInp(
						const Attrib::DescID& storedid )
{
    for ( int idx=attrset_->size()-1; idx>=0; idx-- )
    {
	const DescID descid = attrset_->getID( idx );
	Desc* ad = attrset_->getDesc( descid );
	if ( !ad || ad->isStored() ) continue;

	if ( hasInput(*ad,storedid) )
	    attrset_->removeDesc( descid );
    }
}
