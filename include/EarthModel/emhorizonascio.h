#pragma once

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Kristofer Tingdahl
 Date:		4-11-2002
________________________________________________________________________


-*/

#include "earthmodelmod.h"
#include "tableascio.h"
#include "od_istream.h"


namespace EM
{

/*!
\brief Ascii I/O for Horizon3D.
*/

mExpClass(EarthModel) Horizon3DAscIO : public Table::AscIO
{
public:
				Horizon3DAscIO( const Table::FormatDesc& fd,
						const char* filenm )
				    : Table::AscIO(fd)
				    , udfval_(mUdf(float))
				    , finishedreadingheader_(false)
				    , strm_(filenm) {}

    static Table::FormatDesc*   getDesc();
    static void			updateDesc(Table::FormatDesc&,
					   const BufferStringSet&);
    static void			createDescBody(Table::FormatDesc*,
					   const BufferStringSet&);

    bool			isXY() const;
    bool			isOK() const { return strm_.isOK(); }
    int				getNextLine(Coord&,TypeSet<float>&);
    const UnitOfMeasure*	getSelZUnit() const;

    static const char*		sKeyFormatStr();
    static const char*		sKeyAttribFormatStr();

protected:

    od_istream			strm_;
    float			udfval_;
    bool			finishedreadingheader_;

};


/*!
\brief Ascii I/O for Horizon2D.
*/

mExpClass(EarthModel) Horizon2DAscIO : public Table::AscIO
{
public:
				Horizon2DAscIO( const Table::FormatDesc& fd,
						const char* filenm )
				    : Table::AscIO(fd)
				    , udfval_(mUdf(float))
				    , finishedreadingheader_(false)
				    , strm_(filenm)		    {}

    static Table::FormatDesc*   getDesc();
    static void			updateDesc(Table::FormatDesc&,
					   const BufferStringSet&);
    static void			createDescBody(Table::FormatDesc*,
					   const BufferStringSet&);

    static bool			isFormatOK(const Table::FormatDesc&,
					   BufferString&);
    int				getNextLine(BufferString& lnm,Coord& crd,
					    int& trcnr,float& spnm,
					    TypeSet<float>& data);
    bool			isTraceNr() const;
    bool			isOK() const { return strm_.isOK(); }

protected:

    od_istream			strm_;
    float			udfval_;
    bool			finishedreadingheader_;

};

} // namespace EM

