/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Salil Agarwal
 Date:          27 August 2012
________________________________________________________________________

-*/
static const char* rcsID mUnusedVar = "$Id: ziparchiveinfo.cc,v 1.2 2012-08-31 05:39:11 cvssalil Exp $";

#include "ziparchiveinfo.h"


ZipArchiveInfo::ZipArchiveInfo( BufferString& fnm )
{
    readZipArchive( fnm );
}

void ZipArchiveInfo::readZipArchive( BufferString& fnm )
{
    BufferString headerbuff, headerfnm;
    bool sigcheck;
    unsigned int ptrlocation;
    StreamData isd = StreamProvider( fnm ).makeIStream();
    isd.istrm;
    if ( isd.usable() ) std::cout<<"input stream working \n";
    std::istream& src = *isd.istrm;
    ziphd_.readEndOfCntrlDirHeader( src );
    src.seekg( ziphd_.offsetofcntrldir_, std::ios::beg );
    ptrlocation = src.tellg();
    for ( int i = 0; i < ziphd_.totalfiles_; i++ )
    {
	src.read( headerbuff.buf(), 46);
	mCntrlFileHeaderSigCheck( headerbuff, 0 );
	if ( !sigcheck )
	    std::cout<<"Error::Signature not matched"<<"\n";
	src.seekg( ptrlocation + 46 );
	src.read( headerfnm.buf(), *( (short*) (headerbuff.buf() + 28) ) );
	FileInfo* fi = new FileInfo( headerfnm, 
				*( (unsigned int*) (headerbuff.buf() + 22) ),
				*( (unsigned int*) (headerbuff.buf() + 26) ),
				*( (unsigned int*) (headerbuff.buf() + 42) ) );
	files_ += fi;
	ptrlocation = ptrlocation + *( (short*) (headerbuff.buf() + 28) )
				  + *( (short*) (headerbuff.buf() + 30) )
				  + *( (short*) (headerbuff.buf() + 32) );
	src.seekg( ptrlocation );
    }
    
    isd.close();

}

void ZipArchiveInfo::getAllFnms( BufferStringSet& fnms )
{
    for( int i = 0; i < ziphd_.totalfiles_; i++ )
	fnms.add( files_[i]->fnm_ );
}

unsigned int ZipArchiveInfo::getFCompSize( BufferString& fnm )
{
    for( int i = 0; i < ziphd_.totalfiles_; i++ )
	if ( fnm.matches( files_[i]->fnm_ ) )
	    return files_[i]->compsize_;
    return 0;
}

unsigned int ZipArchiveInfo::getFCompSize( int idx )
{
	    return files_[idx]->compsize_;
}

unsigned int ZipArchiveInfo::getFUnCompSize( BufferString& fnm )
{
    for( int i = 0; i < ziphd_.totalfiles_; i++ )
	if ( fnm.matches( files_[i]->fnm_ ) )
	    return files_[i]->uncompsize_;
    return 0;
}

unsigned int ZipArchiveInfo::getFUnCompSize( int idx )
{
    return files_[idx]->uncompsize_;
}

unsigned int ZipArchiveInfo::getLocalHeaderOffset( BufferString& fnm )
{
    for( int i = 0; i < ziphd_.totalfiles_; i++ )
	if ( fnm.matches( files_[i]->fnm_, true ) )
	    return files_[i]->localheaderoffset_;
    return 0;
}

unsigned int ZipArchiveInfo::getLocalHeaderOffset( int idx )
{
    return files_[idx]->localheaderoffset_;
}