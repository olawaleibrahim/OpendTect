/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : A.H. Bril
 * DATE     : Dec 2003
-*/

static const char* rcsID mUsedVar = "$Id$";

#include "safefileio.h"

#include "dateinfo.h"
#include "file.h"
#include "filepath.h"
#include "genc.h"
#include "oddirs.h"
#include "uistrings.h"


SafeFileIO::SafeFileIO( const char* fnm, bool l )
	: filenm_(fnm)
	, strm_(0)
	, locked_(l)
	, removebakonsuccess_(false)
	, usebakwhenmissing_(true)
	, lockretries_(10)
	, lockwaitincr_(0.5)
	, allowlockremove_(true)
{
    FilePath fp( filenm_ );
    const_cast<BufferString&>(filenm_) = fp.fullPath();
    const BufferString filenmonly( fp.fileName() );

    fp.setFileName( 0 );
    BufferString curfnm( ".lock." ); curfnm += filenmonly;
    const_cast<BufferString&>(lockfnm_) = FilePath(fp,curfnm).fullPath();
    curfnm = filenmonly; curfnm += ".bak";
    const_cast<BufferString&>(bakfnm_) = FilePath(fp,curfnm).fullPath();
    curfnm = filenmonly; curfnm += ".new";
    const_cast<BufferString&>(newfnm_) = FilePath(fp,curfnm).fullPath();
}


SafeFileIO::~SafeFileIO()
{
    if ( strm_ )
	{ pErrMsg("You forgot to close"); }
}


od_istream& SafeFileIO::istrm()
{
    if ( !strm_ )
	{ pErrMsg("no open stream" ); return od_istream::nullStream(); }
    if ( !strm_->forRead() )
	{ pErrMsg("no read stream" ); return od_istream::nullStream(); }
    return static_cast<od_istream&>( *strm_ );
}


od_ostream& SafeFileIO::ostrm()
{
    if ( !strm_ )
	{ pErrMsg("no open stream" ); return od_ostream::nullStream(); }
    if ( !strm_->forWrite() )
	{ pErrMsg("no write stream" ); return od_ostream::nullStream(); }
    return static_cast<od_ostream&>( *strm_ );
}


bool SafeFileIO::open( bool forread, bool ignorelock )
{
    return forread ? openRead( ignorelock ) : openWrite( ignorelock );
}


bool SafeFileIO::openRead( bool ignorelock )
{
    if ( strm_ )
	{ pErrMsg("Stream open before openRead"); closeFail(); }

    if ( locked_ && !ignorelock && !waitForLock() )
	return false;
    mkLock( true );

    if ( !File::checkDirectory(filenm_,true,errmsg_) )
	return false;

    const char* toopen = filenm_.buf();
    if ( File::isEmpty(toopen) )
    {
	if ( usebakwhenmissing_ )
	    toopen = bakfnm_.buf();
	const bool cannotread = !File::exists( toopen ) ||
				!File::isReadable( toopen ) ||
				File::isEmpty( toopen );
	if ( cannotread )
	{
	    uiString postfix;
	    if ( !File::exists(toopen) )
		postfix = tr("missing");
	    else if ( !File::isReadable(toopen) )
		postfix = tr("not readable");
	    else if (  File::isEmpty(toopen) )
		postfix = tr("empty" );

	    errmsg_ = tr("File '%1' is %2").arg( filenm_ ).arg( postfix );
	    rmLock();
	    return false;
	}
	else
	{
	    warnmsg_ = tr("Using backup file: '%1'").arg( bakfnm_ );
	}
    }

    strm_ = new od_istream( toopen );
    if ( !strm_ || !strm_->isOK() )
    {
	if ( strm_ )
	{
	    errmsg_ = strm_->errMsg();
	    deleteAndZeroPtr(strm_);
	}
	else
	{
	    errmsg_ = uiStrings::phrJoinStrings( uiStrings::sCantOpenInpFile(1),
				 toUiString(BufferString(": ",toopen)) );
	    errmsg_.append( uiStrings::sCheckPermissions(), true );
	}

	rmLock();
	return false;
    }

    return true;
}


bool SafeFileIO::openWrite( bool ignorelock )
{
    if ( strm_ )
	{ pErrMsg("Stream open before openWrite"); closeFail(); }

    if ( locked_ && !ignorelock && !waitForLock() )
	return false;
    mkLock( false );

    if ( !File::checkDirectory(newfnm_,false,errmsg_) )
	return false;

    if ( File::exists(newfnm_) )
	File::remove( newfnm_ );

    strm_ = new od_ostream( newfnm_ );
    if ( !strm_ || !strm_->isOK() )
    {
	if ( strm_ )
	{
	    errmsg_ = strm_->errMsg();
	    deleteAndZeroPtr(strm_);
	}
	else
	{
	    errmsg_ = uiStrings::phrJoinStrings(
			       uiStrings::sCantOpenOutpFile(1),
			       uiStrings::phrColonString(toUiString(newfnm_)) );
	    errmsg_.append( uiStrings::sCheckPermissions(), true );
	}

	rmLock();
	return false;
    }

    return true;
}


bool SafeFileIO::commitWrite()
{
    if ( File::isEmpty(newfnm_) )
    {
	errmsg_ = tr("File '%1' not overwritten with empty new file")
		    .arg( filenm_ );
	return false;
    }

    if ( File::exists(bakfnm_) )
	File::remove( bakfnm_ );

    if ( File::exists(filenm_) && !File::rename(filenm_,bakfnm_) )
    {
	warnmsg_ = tr("Cannot create backup file: %1").arg( bakfnm_ );
    }
    if ( !File::rename(newfnm_,filenm_) )
    {
	errmsg_ = tr("Changes in '%1' could not be commited ")
		    .arg( filenm_ );
	return false;
    }
    if ( removebakonsuccess_ )
	File::remove( bakfnm_ );

    return true;
}


bool SafeFileIO::remove()
{
    if ( locked_ )
	waitForLock();

    if ( File::exists(newfnm_) )
	File::remove( newfnm_ );
    if ( File::exists(bakfnm_) )
	File::remove( bakfnm_ );
    if ( File::exists(filenm_) )
	File::remove( filenm_ );

    return File::exists(filenm_);
}


bool SafeFileIO::doClose( bool keeplock, bool docommit )
{
    bool isread = false; bool iswrite = false;
    if ( strm_ )
    {
	isread = strm_->forRead();
	iswrite = strm_->forWrite();
	delete strm_; strm_ = 0;
    }
    bool res = true;
    if ( isread || !docommit )
    {
	if ( !isread && File::exists(newfnm_) )
	    File::remove( newfnm_ );
    }
    else if ( iswrite )
	res = commitWrite();

    if ( !keeplock )
	rmLock();

    return res;
}


bool SafeFileIO::haveLock() const
{
    //TODO read the lock file to see whether date and time are feasible
    return File::exists( lockfnm_ );
}


bool SafeFileIO::waitForLock() const
{
    bool havelock = haveLock();
    if ( !havelock )
	return true;

    for ( int idx=0; havelock && idx<lockretries_; idx++ )
    {
	sleepSeconds( lockwaitincr_ );
	havelock = haveLock();
    }

    if ( !havelock )
	return true;

    if ( allowlockremove_ )
    {
	File::remove( lockfnm_ );
	return true;
    }

    errmsg_ = tr("File '%1' is currently locked.").arg( filenm_ );

    return false;
}


void SafeFileIO::mkLock( bool forread )
{
    if ( !locked_ ) return;

    od_ostream strm( lockfnm_ );
    if ( strm.isOK() )
    {
	DateInfo di; BufferString datestr; di.getUsrDisp( datestr, true );
	strm << "Type: " << (forread ? "Read\n" : "Write\n");
	strm << "Date: " << datestr << " (" << di.key() << ")\n";
	strm << "Host: " << GetLocalHostName() << od_newline;
	strm << "Process: " << GetPID() << '\n';
	const char* ptr = GetPersonalDir();
	strm << "User's HOME: " << (ptr ? ptr : "<none>") << od_newline;
	ptr = GetSoftwareUser();
	strm << "DTECT_USER: " << (ptr ? ptr : "<none>") << od_endl;
    }
}


void SafeFileIO::rmLock()
{
    if ( locked_ && File::exists(lockfnm_) )
	File::remove( lockfnm_ );
}
