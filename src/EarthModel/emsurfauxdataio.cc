/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        K. Tingdahl
 Date:          Jun 2003
________________________________________________________________________

-*/

#include "emsurfauxdataio.h"

#include "ascstream.h"
#include "trckeyzsampling.h"
#include "datachar.h"
#include "datainterp.h"
#include "emhorizon3d.h"
#include "emsurfaceauxdata.h"
#include "emsurfacegeometry.h"
#include "parametricsurface.h"
#include "od_iostream.h"
#include "survinfo.h"
#include "iopar.h"
#include "file.h"
#include "uistrings.h"


namespace EM
{

const char* dgbSurfDataWriter::sKeyAttrName()	    { return "Attribute"; }
const char* dgbSurfDataWriter::sKeyIntDataChar()    { return "Int data"; }
const char* dgbSurfDataWriter::sKeyInt64DataChar()  { return "Long long data"; }
const char* dgbSurfDataWriter::sKeyFloatDataChar()  { return "Float data"; }
const char* dgbSurfDataWriter::sKeyFileType()	    {return "Surface aux data";}
const char* dgbSurfDataWriter::sKeyShift()	    { return "Shift"; }


dgbSurfDataWriter::dgbSurfDataWriter( const Horizon3D& surf,int dataidx,
				    const TrcKeySampling* sel, bool binary,
				    const char* filename )
    : Executor("Aux data writer")
    , stream_(0)
    , chunksize_(100)
    , dataidx_(dataidx)
    , surf_(surf)
    , sel_(sel)
    , sectionindex_(0)
    , binary_(binary)
    , nrdone_(0)
    , filename_(filename)
{
    IOPar par( "Surface Data" );
    par.set( sKeyAttrName(), surf.auxdata.auxDataName(dataidx_) );
    par.set( sKeyShift(), surf.auxdata.auxDataShift(dataidx_) );

    if ( binary_ )
    {
	BufferString dc;

	int idummy;
	DataCharacteristics(idummy).toString( dc );
	par.set( sKeyIntDataChar(), dc );

	od_int64 lldummy;
	DataCharacteristics(lldummy).toString( dc );
	par.set( sKeyInt64DataChar(), dc );

	float fdummy;
	DataCharacteristics(fdummy).toString( dc );
	par.set( sKeyFloatDataChar(), dc );
    }

    stream_ = new od_ostream( filename_ );
    if ( !stream_ || !stream_->isOK() )
	{ delete stream_; stream_ = 0; return; }

    ascostream astream( *stream_ );
    astream.putHeader( sKeyFileType() );
    par.putTo( astream );

    int nrnodes = 0;
    for ( int idx=0; idx<surf.nrSections(); idx++ )
    {
	SectionID sectionid = surf.sectionID(idx);
	const Geometry::BinIDSurface* meshsurf =
			surf.geometry().sectionGeometry(sectionid);
	nrnodes += meshsurf->nrKnots();
    }

    chunksize_ = nrnodes/100 + 1;
    if ( chunksize_ < 100 )
	chunksize_ = 100;

    totalnr_ = nrnodes;
}


dgbSurfDataWriter::~dgbSurfDataWriter()
{
    delete stream_;
}


bool dgbSurfDataWriter::writeDummyHeader( const char* fnm, const char* attrnm )
{
    od_ostream strm( fnm );
    if ( !strm.isOK() )
	return false;

    ascostream astream( strm );
    astream.putHeader( dgbSurfDataWriter::sKeyFileType() );
    IOPar par( "Surface Data" );
    par.set( dgbSurfDataWriter::sKeyAttrName(), attrnm );
    par.putTo( astream );
    return true;
}


#define mErrRetWrite(msg) \
{ errmsg_ = msg; File::remove(filename_.buf()); \
    return ErrorOccurred(); }


int dgbSurfDataWriter::nextStep()
{
    PosID posid( surf_.id() );
    for ( int idx=0; idx<chunksize_; idx++ )
    {
	while ( subids_.isEmpty() )
	{
	    if ( nrdone_ )
	    {
		sectionindex_++;
		if ( sectionindex_ >= surf_.nrSections() )
		    return Finished();
	    }
	    else
	    {
		if ( !writeInt(surf_.nrSections()) )
		    mErrRetWrite(tr("Error in writing data information"))
	    }

	    const SectionID sectionid = surf_.sectionID( sectionindex_ );
	    const Geometry::BinIDSurface* meshsurf =
				surf_.geometry().sectionGeometry( sectionid );
	    if ( !meshsurf ) continue;

	    const int nrnodes = meshsurf->nrKnots();
	    for ( int idy=0; idy<nrnodes; idy++ )
	    {
		const RowCol rc = meshsurf->getKnotRowCol(idy);
		const Coord3 coord = meshsurf->getKnot( rc, false );

		const BinID bid = SI().transform(coord);
		if ( sel_ && !sel_->includes(bid) )
		    continue;

		const RowCol emrc( bid.inl(), bid.crl() );
		const SubID subid = emrc.toInt64();
		posid.setSubID( subid );
		posid.setSectionID( sectionid );
		const float auxval =
		    surf_.auxdata.getAuxDataVal( dataidx_, posid );
		if ( mIsUdf(auxval) )
		{
		    nrdone_++;
		    continue;
		}

		subids_ += subid;
		values_ += auxval;
		nrdone_++;
	    }

	    if ( subids_.isEmpty() )
		return Finished();

	    if ( !writeInt(sectionid) || !writeInt(subids_.size()) )
		mErrRetWrite(tr("Error in writing data information"))
	}

	const int subidindex = subids_.size()-1;
	const SubID subid = subids_[subidindex];
	const float auxvalue = values_[subidindex];

	if ( !writeInt64(subid) || !writeFloat(auxvalue) )
	    mErrRetWrite(tr("Error in writing datavalues"))

	subids_.removeSingle( subidindex );
	values_.removeSingle( subidindex );
    }

    return MoreToDo();
}


BufferString dgbSurfDataWriter::createHovName( const char* base, int idx )
{
    BufferString res( base );
    res += "^"; res += idx; res += ".hov";
    return res;
}

#define mWriteData() \
    if ( !stream_ ) return false; \
    if ( binary_ ) \
	stream_->addBin( val ); \
    else \
	stream_->add( val ).add( od_newline ); \
    return true


bool dgbSurfDataWriter::writeInt( int val )
{ mWriteData(); }


bool dgbSurfDataWriter::writeInt64( od_int64 val )
{ mWriteData(); }


bool dgbSurfDataWriter::writeFloat( float val )
{ mWriteData(); }


od_int64 dgbSurfDataWriter::nrDone() const
{ return nrdone_; }


od_int64 dgbSurfDataWriter::totalNr() const
{ return totalnr_; }


uiString dgbSurfDataWriter::uiMessage() const
{ return errmsg_; }

uiString dgbSurfDataWriter::uiNrDoneText() const
{ return tr("Positions Written"); }



// Reader +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

dgbSurfDataReader::dgbSurfDataReader( const char* filename )
    : Executor("Aux data reader")
    , intinterpreter_(0)
    , int64interpreter_(0)
    , floatinterpreter_(0)
    , chunksize_(100)
    , dataidx_(-1)
    , surf_(0)
    , sectionindex_(0)
    , error_(true)
    , totalnr_(0)
    , nrdone_(0)
    , valsleftonsection_(0)
    , shift_(0)
    , stream_(0)
    , nrsections_(0)
{
    stream_ = new od_istream( filename );
    if ( !stream_ || !stream_->isOK() )
	{ delete stream_; stream_ = 0; return; }

    ascistream astream( *stream_ );
    if ( !astream.isOfFileType(dgbSurfDataWriter::sKeyFileType()) )
        return;

    const IOPar par( astream );
    if ( !par.get(dgbSurfDataWriter::sKeyAttrName(),dataname_) )
	return;

    if ( !par.get(dgbSurfDataWriter::sKeyAttrName(),datainfo_) )
	return;

    par.get( dgbSurfDataWriter::sKeyShift(), shift_ );

    BufferString dc;
    if ( par.get(dgbSurfDataWriter::sKeyIntDataChar(),dc) )
    {
	DataCharacteristics writtendatachar;
	writtendatachar.set( dc.buf() );
	intinterpreter_ = new DataInterpreter<int>( writtendatachar );

	if ( !par.get(dgbSurfDataWriter::sKeyInt64DataChar(),dc) )
	{
	    error_ = true;
	    errmsg_ = tr("Error in reading data characteristics (int64)");
	    return;
	}
	writtendatachar.set( dc.buf() );
	int64interpreter_ = new DataInterpreter<od_int64>( writtendatachar );

	if ( !par.get(dgbSurfDataWriter::sKeyFloatDataChar(),dc) )
	{
	    error_ = true;
	    errmsg_ = tr("Error in reading data characteristics (float)");
	    return;
	}
	writtendatachar.set( dc.buf() );
	floatinterpreter_ = new DataInterpreter<float>( writtendatachar );
    }

    error_ = false;
}


dgbSurfDataReader::~dgbSurfDataReader()
{
    delete stream_;
    delete intinterpreter_;
    delete int64interpreter_;
    delete floatinterpreter_;
}


const char* dgbSurfDataReader::dataName() const
{
    return dataname_[0] ? dataname_.buf() : 0;
}


float dgbSurfDataReader::shift() const
{ return shift_; }


const char* dgbSurfDataReader::dataInfo() const
{
    return datainfo_[0] ? datainfo_.buf() : 0;
}



void dgbSurfDataReader::setSurface( Horizon3D& surf )
{
    surf_ = &surf;
    dataidx_ = surf_->auxdata.addAuxData( dataname_.buf() );
    surf_->auxdata.setAuxDataShift( dataidx_, shift_ );
}

uiString dgbSurfDataReader::sHorizonData()
{
    return tr("Horizon data");
}

#ifdef __debug__
#   define mErrRetRead(msg) { \
    if ( !msg.isEmpty() ) errmsg_ = msg; \
    surf_->auxdata.removeAuxData(dataidx_); return ErrorOccurred(); }
#else
    #define mErrRetRead(msg) { \
    surf_->auxdata.removeAuxData(dataidx_); return ErrorOccurred(); }
#endif


#ifdef __debug__
#   define mErrRetReadNoDeleteAux(msg) { \
    if ( !msg.isEmpty() ) errmsg_ = msg; \
    return ErrorOccurred(); }
#else
#define mErrRetReadNoDeleteAux(msg) { \
    return ErrorOccurred(); }
#endif


int dgbSurfDataReader::nextStep()
{
    if ( error_ ) mErrRetRead( uiString::emptyString() )

    PosID posid( surf_->id() );
    for ( int idx=0; idx<chunksize_; idx++ )
    {
	while ( !valsleftonsection_ )
	{
	    if ( nrdone_ )
	    {
		sectionindex_++;
		if ( sectionindex_ >= nrsections_ || nrsections_ < 0 )
		    return Finished();
	    }
	    else
	    {
		readInt( nrsections_ );
		if ( stream_->atEOF() )
		    return Finished();
		if ( nrsections_ < 0 )
		    mErrRetReadNoDeleteAux(
		    uiStrings::phrCannotRead( sHorizonData() ) )
	    }

	    int cursec = -1;
	    const bool res = !readInt(cursec) || !readInt(valsleftonsection_);
	    if ( stream_->atEOF() )
		return Finished();
	    if ( res || cursec<0 )
		mErrRetReadNoDeleteAux(
		uiStrings::phrCannotRead( sHorizonData() ) )

	    currentsection_ = mCast(EM::SectionID,cursec);
	    totalnr_ = 100;
	    chunksize_ = valsleftonsection_/totalnr_+1;
	    if ( chunksize_ < 100 )
	    {
		chunksize_ = mMIN(100,valsleftonsection_);
		totalnr_ = valsleftonsection_/chunksize_+1;
	    }
	}

	SubID subid;
	float val;
	if ( !readInt64(subid) || !readFloat(val) )
	    mErrRetReadNoDeleteAux( uiStrings::phrCannotRead( sHorizonData() ) )

	posid.setSubID( subid );
	posid.setSectionID( currentsection_ );
	surf_->auxdata.setAuxDataVal( dataidx_, posid, val );

	valsleftonsection_--;
    }

    nrdone_++;
    return MoreToDo();
}


#define mReadData(interpreter) \
    if ( !stream_ ) \
	return false; \
    if ( interpreter ) \
    { \
	char buf[sizeof(res)]; \
	if ( !stream_->getBin(buf,sizeof(res)) ) return false; \
	res = interpreter->get( buf, 0 ); \
    } \
    else \
    { if ( stream_->get(res).isBad() ) return false; } \
    return true;

bool dgbSurfDataReader::readInt( int& res )
{ mReadData(intinterpreter_) }

bool dgbSurfDataReader::readInt64( od_int64& res )
{ mReadData(int64interpreter_) }

bool dgbSurfDataReader::readFloat( float& res )
{ mReadData(floatinterpreter_) }


od_int64 dgbSurfDataReader::nrDone() const
{ return nrdone_; }


od_int64 dgbSurfDataReader::totalNr() const
{ return totalnr_; }


uiString dgbSurfDataReader::uiMessage() const
{ return errmsg_; }

uiString dgbSurfDataReader::uiNrDoneText() const
{ return tr("Positions Read"); }

} // namespace EM
