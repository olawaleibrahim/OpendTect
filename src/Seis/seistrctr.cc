/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : A.H. Bril
 * DATE     : 21-3-1996
 * FUNCTION : Seis trace translator
-*/


#include "seistrctr.h"
#include "keystrs.h"
#include "seistrc.h"
#include "seisinfo.h"
#include "seispacketinfo.h"
#include "seisselection.h"
#include "seisbuf.h"
#include "iopar.h"
#include "ioobj.h"
#include "iostrm.h"
#include "ioman.h"
#include "sorting.h"
#include "separstr.h"
#include "scaler.h"
#include "ptrman.h"
#include "survinfo.h"
#include "bufstringset.h"
#include "tracedata.h"
#include "trckeyzsampling.h"
#include "envvars.h"
#include "file.h"
#include "od_stream.h"
#include <math.h>


const char* SeisTrcTranslator::sKeyIs2D()	{ return "Is2D"; }
const char* SeisTrcTranslator::sKeyIsPS()	{ return "IsPS"; }
const char* SeisTrcTranslator::sKeyRegWrite()
					{ return "Enforce Regular Write"; }
const char* SeisTrcTranslator::sKeySIWrite()
					{ return "Enforce SurveyInfo Write"; }


SeisTrcTranslator::ComponentData::ComponentData( const SeisTrc& trc, int icomp,
						 const char* nm )
	: BasicComponentInfo(nm)
{
    datachar = trc.data().getInterpreter(icomp)->dataChar();
}


const char*
SeisTrcTranslatorGroup::getSurveyDefaultKey(const IOObj* ioobj) const
{
    if ( ioobj && SeisTrcTranslator::is2D( *ioobj ) )
	return IOPar::compKey( sKey::Default(), sKeyDefault2D() );

    if ( SI().survDataType()==SurveyInfo::Only2D )
	return IOPar::compKey( sKey::Default(), sKeyDefault2D() );

    return IOPar::compKey( sKey::Default(), sKeyDefault3D() );
}


SeisTrcTranslator::SeisTrcTranslator( const char* nm, const char* unm )
    : Translator(nm,unm)
    , conn_(0)
    , inpfor_(0)
    , nrout_(0)
    , inpcds_(0)
    , outcds_(0)
    , seldata_(0)
    , prevnr_(mUdf(int))
    , pinfo_(*new SeisPacketInfo)
    , trcblock_(*new SeisTrcBuf(false))
    , lastinlwritten_(SI().sampling(false).hsamp_.start_.inl())
    , read_mode(Seis::Prod)
    , is_prestack(false)
    , is_2d(false)
    , enforce_regular_write( !GetEnvVarYN("OD_NO_SEISWRITE_REGULARISATION") )
    , enforce_survinfo_write( GetEnvVarYN("OD_ENFORCE_SURVINFO_SEISWRITE") )
    , geomid_(mUdfGeomID)
    , headerdonenew_(false)
    , datareaddone_(false)
    , storbuf_(0)
    , trcscalebase_(0)
    , curtrcscalebase_(0)
    , compnms_(0)
    , warnings_(*new BufferStringSet)
{
}


SeisTrcTranslator::~SeisTrcTranslator()
{
    cleanUp();
    delete compnms_;
    delete &trcblock_;
    delete &pinfo_;
    delete &warnings_;
}


bool SeisTrcTranslator::is2D( const IOObj& ioobj, bool internal_only )
{
    const bool trok = *ioobj.group() == '2';
    return trok || internal_only ? trok
	: ioobj.pars().isTrue( sKeyIs2D() ) || Seis::is2DGeom( ioobj.pars() );
}


bool SeisTrcTranslator::isPS( const IOObj& ioobj )
{
    return *ioobj.group() == 'P' || *(ioobj.group()+3) == 'P';
}


bool SeisTrcTranslator::isLineSet( const IOObj& ioobj )
{
    return *ioobj.translator() == '2';
}


void SeisTrcTranslator::cleanUp()
{
    close();

    headerdonenew_ = false;
    datareaddone_ = false;
    deleteAndZeroPtr( storbuf_ );
    deepErase( cds_ );
    deepErase( tarcds_ );
    deleteAndZeroArrPtr( inpfor_ );
    deleteAndZeroArrPtr( inpcds_ );
    deleteAndZeroArrPtr( outcds_ );
    deleteAndZeroPtr( trcscalebase_ );
    curtrcscalebase_ = 0;
    nrout_ = 0;
    errmsg_.setEmpty();
}


bool SeisTrcTranslator::close()
{
    bool ret = true;
    if ( conn_ && !conn_->forRead() )
	ret = writeBlock();
    delete conn_; conn_ = 0;
    return ret;
}


bool SeisTrcTranslator::initRead( Conn* c, Seis::ReadMode rm )
{
    cleanUp();
    read_mode = rm;
    if ( !initConn(c) || !initRead_() )
    {
	delete conn_; conn_ = 0;
	return false;
    }

    pinfo_.zrg.start = insd_.start;
    pinfo_.zrg.step = insd_.step;
    pinfo_.zrg.stop = insd_.start + insd_.step * (innrsamples_-1);
    return true;
}


bool SeisTrcTranslator::initWrite( Conn* c, const SeisTrc& trc )
{
    cleanUp();

    innrsamples_ = outnrsamples_ = trc.size();
    if ( innrsamples_ < 1 )
    {
	errmsg_ = tr("Empty first trace");
	return false;
    }

    insd_ = outsd_ = trc.info().sampling;

    if ( seldata_ )
    {
	pinfo_.inlrg = seldata_->inlRange();
	pinfo_.crlrg = seldata_->crlRange();
	pinfo_.zrg = seldata_->zRange();
    }

    if ( !initConn(c) || !initWrite_(trc) )
    {
	deleteAndZeroPtr( conn_ );
	return false;
    }

    return true;
}


bool SeisTrcTranslator::commitSelections()
{
    errmsg_ = tr("No selected components found");
    const int sz = tarcds_.size();
    if ( sz < 1 ) return false;

    outsd_ = insd_; outnrsamples_ = innrsamples_;
    if ( seldata_ && !mIsUdf(seldata_->zRange().start) )
    {
	Interval<float> selzrg( seldata_->zRange() );
	const Interval<float> sizrg( SI().sampling(false).zsamp_ );
	if ( !mIsEqual(selzrg.start,sizrg.start,1e-8)
	  || !mIsEqual(selzrg.stop,sizrg.stop,1e-8) )
	{
	    outsd_.start = selzrg.start;
	    const float fnrsteps = (selzrg.stop-selzrg.start) / outsd_.step;
	    outnrsamples_ = mNINT32(fnrsteps) + 1;
	}
    }

    mAllocLargeVarLenArr( int, selnrs, sz );
    mAllocLargeVarLenArr( int, inpnrs, sz );
    int nrsel = 0;
    for ( int idx=0; idx<sz; idx++ )
    {
	int destidx = tarcds_[idx]->destidx;
	if ( destidx >= 0 )
	{
	    selnrs[nrsel] = destidx;
	    inpnrs[nrsel] = idx;
	    nrsel++;
	}
    }

    nrout_ = nrsel;
    if ( nrout_ < 1 ) nrout_ = 1;
    delete [] inpfor_; inpfor_ = new int [nrout_];
    if ( nrsel < 1 )
	inpfor_[0] = 0;
    else if ( nrsel == 1 )
	inpfor_[0] = inpnrs[0];
    else
    {
	sort_coupled( (int*)selnrs, (int*)inpnrs, nrsel );
	for ( int idx=0; idx<nrout_; idx++ )
	    inpfor_[idx] = inpnrs[idx];
    }

    delete [] inpcds_; inpcds_ = new ComponentData* [nrout_];
    delete [] outcds_; outcds_ = new TargetComponentData* [nrout_];
    for ( int idx=0; idx<nrout_; idx++ )
    {
	inpcds_[idx] = cds_[ selComp(idx) ];
	outcds_[idx] = tarcds_[ selComp(idx) ];
    }

    errmsg_.setEmpty();
    enforceBounds();

    float fsampnr = (outsd_.start - insd_.start) / insd_.step;
    samprg_.start = mNINT32( fsampnr );
    samprg_.stop = samprg_.start + outnrsamples_ - 1;

    const bool forread = forRead();
    const int nrcomps = nrSelComps();
    const ComponentData** cds = forread ? (const ComponentData**)inpcds_
					: (const ComponentData**)outcds_;
    const int ns = forread ? innrsamples_ : outnrsamples_;
    delete storbuf_;
    storbuf_ = new TraceData;
    for ( int iselc=0; iselc<nrcomps; iselc++ )
	storbuf_->addComponent( ns+1, cds[iselc]->datachar , true );

    if ( !storbuf_->allOk() )
    {
	errmsg_ = tr("Out of memory");
	deleteAndZeroPtr( storbuf_ );
	return false;
    }

    return commitSelections_();
}


void SeisTrcTranslator::enforceBounds()
{
    // Ranges
    outsd_.step = insd_.step;
    float outstop = outsd_.start + (outnrsamples_ - 1) * outsd_.step;
    if ( outsd_.start < insd_.start )
	outsd_.start = insd_.start;
    const float instop = insd_.start + (innrsamples_ - 1) * insd_.step;
    if ( outstop > instop )
	outstop = instop;

    // Snap to samples
    float sampdist = (outsd_.start - insd_.start) / insd_.step;
    int startsamp = (int)(sampdist + 0.0001);
    if ( startsamp < 0 ) startsamp = 0;
    if ( startsamp > innrsamples_-1 ) startsamp = innrsamples_-1;
    sampdist = (outstop - insd_.start) / insd_.step;
    int endsamp = (int)(sampdist + 0.9999);
    if ( endsamp < startsamp ) endsamp = startsamp;
    if ( endsamp > innrsamples_-1 ) endsamp = innrsamples_-1;

    outsd_.start = insd_.start + startsamp * insd_.step;
    outnrsamples_ = endsamp - startsamp + 1;
}


bool SeisTrcTranslator::write( const SeisTrc& trc )
{
    if ( !inpfor_ && !commitSelections() )
	return false;

    if ( !inlCrlSorted() )
    {
	// No buffering: who knows what we'll get?
	dumpBlock();
	return writeTrc_( trc );
    }

    const bool haveprev = !mIsUdf( prevnr_ );
    const bool wrblk = haveprev && (is_2d ? prevnr_ > 99
				: prevnr_ != trc.info().binid.inl());
    if ( !is_2d )
	prevnr_ = trc.info().binid.inl();
    else if ( wrblk || !haveprev )
	prevnr_ = 1;
    else
	prevnr_++;

    if ( wrblk && !writeBlock() )
	return false;

    SeisTrc* newtrc; mTryAlloc( newtrc, SeisTrc(trc) );
    if ( !newtrc )
	{ errmsg_ = tr("Out of memory"); trcblock_.deepErase(); return false; }
    trcblock_.add( newtrc );

    return true;
}


bool SeisTrcTranslator::writeBlock()
{
    int sz = trcblock_.size();
    SeisTrc* firsttrc = sz ? trcblock_.get(0) : 0;
    if ( firsttrc )
	lastinlwritten_ = firsttrc->info().binid.inl();

    if ( sz && enforce_regular_write )
    {
	SeisTrcInfo::Fld keyfld = is_2d ? SeisTrcInfo::TrcNr
					: SeisTrcInfo::BinIDCrl;
	bool sort_asc = true;
	if ( is_2d )
	    sort_asc = trcblock_.get(0)->info().nr
		     < trcblock_.get(trcblock_.size()-1)->info().nr;
	    // for 2D we're buffering only 100 traces, not an entire line
	trcblock_.sort( sort_asc, keyfld );
	firsttrc = trcblock_.get( 0 );
	const int firstnr = is_2d ? firsttrc->info().nr
				 : firsttrc->info().binid.crl();
	int nrperpos = 1;
	if ( !is_2d )
	{
	    for ( int idx=1; idx<sz; idx++ )
	    {
		if ( trcblock_.get(idx)->info().binid.crl() != firstnr )
		    break;
		nrperpos++;
	    }
	}

	if ( !isPS() )
	    trcblock_.enforceNrTrcs( nrperpos, keyfld );
	sz = trcblock_.size();
    }

    if ( !enforce_survinfo_write )
	return dumpBlock();

    StepInterval<int> inlrg, crlrg;
    SI().sampling(true).hsamp_.get( inlrg, crlrg );
    const int firstafter = crlrg.stop + crlrg.step;
    int stp = crlrg.step;
    int bufidx = 0;
    SeisTrc* trc = bufidx < sz ? trcblock_.get(bufidx) : 0;
    BinID binid( lastinlwritten_, crlrg.start );
    SeisTrc* filltrc = 0;
    int nrwritten = 0;
    for ( ; binid.crl() != firstafter; binid.crl() += stp )
    {
	while ( trc && trc->info().binid.crl() < binid.crl() )
	{
	    bufidx++;
	    trc = bufidx < sz ? trcblock_.get(bufidx) : 0;
	}
	if ( trc )
	{
	    if ( !writeTrc_(*trc) )
		return false;
	}
	else
	{
	    if ( !filltrc )
		filltrc = getFilled( binid );
	    else
	    {
		filltrc->info().binid = binid;
		filltrc->info().coord = SI().transform(binid);
	    }
	    if ( !writeTrc_(*filltrc) )
		return false;
	}
	nrwritten++;
    }

    delete filltrc;
    trcblock_.deepErase();
    blockDumped( nrwritten );
    return true;
}


bool SeisTrcTranslator::dumpBlock()
{
    bool rv = true;
    const int sz = trcblock_.size();
    for ( int idx=0; idx<sz; idx++ )
    {
	if ( !writeTrc_(*trcblock_.get(idx)) )
	    { rv = false; break; }
    }
    trcblock_.deepErase();
    blockDumped( sz );
    return rv;
}


void SeisTrcTranslator::prepareComponents( SeisTrc& trc, int actualsz ) const
{
    for ( int idx=0; idx<nrout_; idx++ )
    {
        TraceData& td = trc.data();
	if ( !tarcds_.validIdx(idx) ) break;
        if ( td.nrComponents() <= idx )
            td.addComponent( actualsz, tarcds_[ inpfor_[idx] ]->datachar );
        else
        {
            td.setComponent( tarcds_[ inpfor_[idx] ]->datachar, idx );
            td.reSize( actualsz, idx );
        }
    }
}


bool SeisTrcTranslator::forRead() const
{
    return conn_ ? conn_->forRead() : true;
}



void SeisTrcTranslator::addComp( const DataCharacteristics& dc,
				 const char* nm, int dtype )
{
    BufferString str( "Component " );
    if ( !nm || !*nm )
    {
	if ( compnms_ && cds_.size() < compnms_->size() )
	    nm = compnms_->get(cds_.size()).buf();
	else
	{
	    str += cds_.size() + 1;
	    nm = str.buf();
	}
    }

    ComponentData* newcd = new ComponentData( nm );
    newcd->datachar = dc;
    newcd->datatype = dtype;
    cds_ += newcd;
    bool isl = newcd->datachar.littleendian_;
    newcd->datachar.littleendian_ = __islittle__;
    tarcds_ += new TargetComponentData( *newcd, cds_.size()-1 );
    newcd->datachar.littleendian_ = isl;
}


bool SeisTrcTranslator::initConn( Conn* c )
{
    close(); errmsg_.setEmpty();
    if ( !c )
	{ errmsg_ = tr("Translator: No connection established"); return false; }

    mDynamicCastGet(StreamConn*,strmconn,c)
    if ( strmconn )
    {
	const char* fnm = strmconn->odStream().fileName();
	if ( c->isBad() && !File::isDirectory(fnm) )
	{
	    errmsg_ = tr( "Cannot open file: %1" ).arg( fnm );
	    return false;
	}
    }

    delete conn_; conn_ = c;
    return true;
}


SeisTrc* SeisTrcTranslator::getEmpty()
{
    DataCharacteristics dc;
    if ( outcds_ )
	dc = outcds_[0]->datachar;
    else if ( !tarcds_.isEmpty() && inpfor_ )
	dc = tarcds_[selComp()]->datachar;

    return new SeisTrc( 0, dc );
}


void SeisTrcTranslator::setComponentNames( const BufferStringSet& bss )
{
    delete compnms_; compnms_ = new BufferStringSet( bss );
}


void SeisTrcTranslator::getComponentNames( BufferStringSet& bss ) const
{
    bss.erase();
    if ( cds_.size() == 1 )	//TODO display comp name only if more than 1
    {
	bss.add("");
	return;
    }

    for ( int idx=0; idx<cds_.size(); idx++ )
	bss.add( cds_[idx]->name() );
}


bool SeisTrcTranslator::read( SeisTrc& trc )
{
    if ( !headerdonenew_ && !readInfo(trc.info()) )
	return false;

    if ( (!datareaddone_ && !readData()) || !copyDataToTrace(trc) )
	return false;

    headerdonenew_ = false;
    datareaddone_ = false;

    return true;
}


bool SeisTrcTranslator::copyDataToTrace( SeisTrc& trc )
{
    if ( !datareaddone_ )
	return false;

    prepareComponents( trc, outnrsamples_ );
    if ( curtrcscalebase_ )
	trc.convertToFPs();

    const bool matchingdc = *trc.data().getInterpreter(0) ==
			    *storbuf_->getInterpreter(0);
    for ( int iselc=0; iselc<nrSelComps(); iselc++ )
    {
	if ( matchingdc && outnrsamples_ > 1 && !curtrcscalebase_ )
	{
	    OD::memCopy( trc.data().getComponent(iselc)->data(),
		    storbuf_->getComponent(iselc)->data(),
		    outnrsamples_ * storbuf_->bytesPerSample( iselc ) );
	}
	else
	{
	    for ( int isamp=0; isamp<outnrsamples_; isamp++ )
	    {
		//Parallel !!
		float inpval = storbuf_->getValue( isamp, iselc );
		if ( curtrcscalebase_ )
		    inpval = (float)curtrcscalebase_->scale(inpval);

		trc.set( isamp, inpval, iselc );
	    }
	}
    }

    return true;
}


SeisTrc* SeisTrcTranslator::getFilled( const BinID& binid )
{
    if ( !outcds_ )
	return 0;

    SeisTrc* newtrc = new SeisTrc;
    newtrc->info().binid = binid;
    newtrc->info().coord = SI().transform( binid );

    newtrc->data().delComponent(0);
    for ( int idx=0; idx<nrout_; idx++ )
    {
	newtrc->data().addComponent( outnrsamples_,
				     outcds_[idx]->datachar, true );
	newtrc->info().sampling = outsd_;
    }

    return newtrc;
}


bool SeisTrcTranslator::getRanges( const MultiID& ky, TrcKeyZSampling& cs,
				   const char* lk )
{
    PtrMan<IOObj> ioobj = IOM().get( ky );
    return ioobj ? getRanges( *ioobj, cs, lk ) : false;
}


bool SeisTrcTranslator::getRanges( const IOObj& ioobj, TrcKeyZSampling& cs,
				   const char* lnm )
{
    PtrMan<Translator> transl = ioobj.createTranslator();
    mDynamicCastGet(SeisTrcTranslator*,tr,transl.ptr());
    if ( !tr ) return false;
    PtrMan<Seis::SelData> sd = 0;
    if ( lnm && *lnm )
    {
	sd = Seis::SelData::get( Seis::Range );
	sd->setGeomID( Survey::GM().getGeomID(lnm) );
	tr->setSelData( sd );
    }

    mDynamicCastGet(const IOStream*,iostrmptr,&ioobj)
    if ( !iostrmptr || !iostrmptr->isMultiConn() )
    {
	Conn* cnn = ioobj.getConn( Conn::Read );
	if ( !cnn || !tr->initRead(cnn,Seis::PreScan) )
	    return false;

	const SeisPacketInfo& pinf = tr->packetInfo();
	cs.hsamp_.set( pinf.inlrg, pinf.crlrg );
	cs.zsamp_ = pinf.zrg;
    }
    else
    {
	IOStream& iostrm = *const_cast<IOStream*>( iostrmptr );
	iostrm.resetConnIdx();
	cs.setEmpty();
	do
	{
	    PtrMan<Translator> translator = ioobj.createTranslator();
	    mDynamicCastGet(SeisTrcTranslator*,seistr,translator.ptr());
	    Conn* conn = iostrm.getConn( Conn::Read );
	    if ( !seistr || !conn || !seistr->initRead(conn,Seis::PreScan) )
		return !cs.isEmpty();

	    const SeisPacketInfo& pinf = seistr->packetInfo();
	    TrcKeyZSampling newcs( false );
	    newcs.hsamp_.set( pinf.inlrg, pinf.crlrg );
	    newcs.zsamp_ = pinf.zrg;
	    cs.include( newcs );
	} while ( iostrm.toNextConnIdx() );
    }

    return true;
}


void SeisTrcTranslator::usePar( const IOPar& iop )
{
    iop.getYN( sKeyIs2D(), is_2d );
    iop.getYN( sKeyIsPS(), is_prestack );
    iop.getYN( sKeyRegWrite(), enforce_regular_write );
    iop.getYN( sKeySIWrite(), enforce_survinfo_write );
}


bool SeisTrcTranslator::haveWarnings() const
{
    return !warnings_.isEmpty();
}


void SeisTrcTranslator::addWarn( int nr, const char* msg )
{
    if ( !msg || !*msg || warnnrs_.isPresent(nr) ) return;
    warnnrs_ += nr;
    warnings_.add( msg );
}
