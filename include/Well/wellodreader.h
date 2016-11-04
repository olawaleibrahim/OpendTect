#pragma once

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert Bril
 Date:		Aug 2003
________________________________________________________________________


-*/

#include "wellreadaccess.h"
#include "wellio.h"
#include "sets.h"
#include "ranges.h"
#include "od_iosfwd.h"
class BufferStringSet;
class IOObj;


namespace Well
{

/*!\brief Reads Well::Data from OpendTect file store  */

mExpClass(Well) odReader : public odIO
			 , public Well::ReadAccess
{ mODTextTranslationClass(Well::odReader)
public:

			odReader(const IOObj&,Data&,uiString& errmsg);
			odReader(const char* fnm,Data&,uiString& errmsg);

    virtual bool	getInfo() const;
    virtual bool	getTrack() const;
    virtual bool	getLogs() const;
    virtual bool	getMarkers() const;	//needs to read Track too
    virtual bool	getD2T() const;
    virtual bool	getCSMdl() const;
    virtual bool	getDispProps() const;
    virtual bool	getLog(const char* lognm) const;
    virtual void	getLogInfo(BufferStringSet& lognms) const;

    virtual const uiString& errMsg() const	{ return odIO::errMsg(); }

    bool		getInfo(od_istream&) const;
    bool		addLog(od_istream&) const;
    bool		getMarkers(od_istream&) const;
    bool		getD2T(od_istream&) const;
    bool		getCSMdl(od_istream&) const;
    bool		getDispProps(od_istream&) const;

protected:

    void		readLogData(Log&,od_istream&,int) const;
    bool		getTrack(od_istream&) const;
    bool		doGetD2T(od_istream&,bool csmdl) const;
    bool		doGetD2T(bool) const;
    void		getLogInfo(BufferStringSet&,TypeSet<int>&) const;

    static Log*		rdLogHdr(od_istream&,int&,int);

    void		setInpStrmOpenErrMsg(od_istream&) const;
    void		setStrmOperErrMsg(od_istream&,const uiString&) const;

};

}; // namespace Well
