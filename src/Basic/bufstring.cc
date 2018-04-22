/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Bert
 * DATE     : Oct 2003
-*/


#include "bufstring.h"
#include "bufstringset.h"
#include "fixedstring.h"
#include "iopar.h"
#include "staticstring.h"
#include "separstr.h"
#include "string2.h"
#include "globexpr.h"
#include "uistring.h"
#include "odmemory.h"

#ifndef OD_NO_QT
# include <QString>
# include <QStringList>
#endif


BufferString::BufferString( size_type minlen, bool mknull )
    : minlen_(minlen)
    , len_(0)
    , buf_(0)
{
    if ( minlen_ < 1 )
	return;

    setBufSize( minlen_ );
    if ( len_ > 0 )
    {
	if ( len_ == 1 )
	    *buf_ = '\0';
	else
	    OD::sysMemSet( buf_, '\0', len_ );
    }
}


BufferString::BufferString( const BufferString& bs )
    : minlen_(bs.minlen_)
    , len_(0)
    , buf_(0)
{
    if ( !bs.buf_ || !bs.len_ ) return;

    mTryAlloc( buf_, char [bs.len_] );
    if ( buf_ )
    {
	len_ = bs.len_;
	strcpy( buf_, bs.buf_ );
    }
}


BufferString::BufferString( const mQtclass(QString)& qstr )
    : mBufferStringSimpConstrInitList
{
    add( qstr );
}


BufferString::~BufferString()
{
    destroy();
}


const char* BufferString::gtBuf() const
{
    if ( !buf_ )
	const_cast<BufferString*>(this)->init();

    return buf_;
}


char* BufferString::find( char c )
{ return firstOcc( getCStr(), c ); }
char* BufferString::findLast( char c )
{ return lastOcc( getCStr(), c ); }
char* BufferString::find( const char* s )
{ return firstOcc( getCStr(), s ); }
char* BufferString::findLast( const char* s )
{ return lastOcc( getCStr(), s ); }


BufferString& BufferString::setEmpty()
{
    if ( len_ != minlen_ )
	{ destroy(); init(); }
    else if ( !buf_ )
	{ pErrMsg("Probably working with deleted string"); }
    else
	buf_[0] = '\0';

    return *this;
}


BufferString& BufferString::assignTo( const char* s )
{
    if ( buf_ == s ) return *this;

    if ( !s ) s = "";
    setBufSize( (size_type)(strLength(s) + 1) );
    char* ptr = buf_;
    while ( *s ) *ptr++ = *s++;
    *ptr = '\0';
    return *this;
}


BufferString& BufferString::add( char s )
{
    char tmp[2];
    tmp[0] = s; tmp[1] = '\0';
    return add( tmp );
}


BufferString& BufferString::add( const char* s )
{
    if ( s && *s )
    {
	const size_type newsize = strLength(s) +
				     ( buf_ ? strLength(buf_) : 0 ) +1;
	setBufSize( newsize );

	char* ptr = buf_;
	while ( *ptr ) ptr++;
	while ( *s ) *ptr++ = *s++;
	*ptr = '\0';
    }
    return *this;
}


BufferString& BufferString::add( const mQtclass(QString)& qstr )
{
#ifdef OD_NO_QT
    return *this;
#else
    const QByteArray qba = qstr.toUtf8();
    return add( qba.constData() );
#endif
}


BufferString& BufferString::addPrecise( float f )
{
    return add( toStringPrecise( f ) );
}


BufferString& BufferString::addPrecise( double d )
{
    return add( toStringPrecise( d ) );
}


BufferString& BufferString::addLim( float f, size_type maxnrchars )
{
    return add( toStringLim( f, maxnrchars ) );
}


BufferString& BufferString::addLim( double d, size_type maxnrchars )
{
    return add( toStringLim( d, maxnrchars ) );
}


static const char* spc32 =
"                                ";
static const char* tab32 =
"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
static const char* nl32 =
"\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";


BufferString& BufferString::addArr32Chars( const char* carr, size_type nr )
{
    if ( nr == 1 )
	return add( carr + 31 ); // shortcut for majority of calls
    else if ( nr < 1 )
	return *this;

    const size_type nr32 = nr / 32;
    for ( idx_type idx=0; idx<nr32; idx++ )
	add( carr );

    return add( carr + 31 - (nr%32) + 1 );
}


BufferString& BufferString::addSpace( size_type nr )
{
    return addArr32Chars( spc32, nr );
}


BufferString& BufferString::addTab( size_type nr )
{
    return addArr32Chars( tab32, nr );
}


BufferString& BufferString::addNewLine( size_type nr )
{
    return addArr32Chars( nl32, nr );
}


bool BufferString::setBufSize( size_type newlen )
{
    if ( newlen < minlen_ )
	newlen = minlen_;
    else if ( newlen == len_ )
	return true;

    if ( minlen_ > 1 )
    {
	size_type nrminlens = newlen / minlen_;
	if ( newlen % minlen_ ) nrminlens++;
	newlen = nrminlens * minlen_;
    }
    if ( buf_ && newlen == len_ )
	return true;

    char* oldbuf = buf_;
    mTryAlloc( buf_, char [newlen] );
    if ( !buf_ )
	{ buf_ = oldbuf; return false; }
    else if ( !oldbuf )
	*buf_ = '\0';
    else
    {
	size_type newsz = (oldbuf ? strLength( oldbuf ) : 0) + 1;
	if ( newsz > newlen )
	{
	    newsz = newlen;
	    oldbuf[newsz-1] = '\0';
	}
	OD::sysMemCopy( buf_, oldbuf, newsz );
    }

    delete [] oldbuf;
    len_ = newlen;

    return true;
}


void BufferString::setMinBufSize( size_type newlen )
{
    const_cast<size_type&>(minlen_) = newlen;
    setBufSize( len_ );
}


BufferString& BufferString::replace( char from, char to )
{
    if ( isEmpty() || from == to )
	return *this;

    char* ptr = getCStr();
    while ( *ptr )
    {
	if ( *ptr == from )
	    *ptr = to;
	ptr++;
    }
    return *this;
}


BufferString& BufferString::replace( const char* from, const char* to )
{
    if ( isEmpty() || !from || !*from )
	return *this;

    const size_type fromlen = strLength( from );

    char* ptrfound = find( from );
    while ( ptrfound )
    {
	BufferString rest( ptrfound + fromlen );
	*ptrfound = '\0';
	add( to );
	const idx_type curpos = size();
	add( rest );
	ptrfound = firstOcc( getCStr()+curpos, from );
    }
    return *this;
}


BufferString& BufferString::remove( char torem )
{
    if ( isEmpty() )
	return *this;

    char* writepos = getCStr();
    const char* chckpos = writepos;

    while ( *chckpos )
    {
	if ( *chckpos != torem )
	{
	    *writepos = *chckpos;
	    writepos++;
	}
	chckpos++;
    }
    *writepos = '\0';
    return *this;
}


BufferString& BufferString::trimBlanks()
{
    if ( isEmpty() )
	return *this;

    char* memstart = getCStr();
    char* ptr = memstart;
    mSkipBlanks( ptr );
    removeTrailingBlanks( ptr );

    if ( ptr == memstart )
	return *this;
    else if ( !*ptr )
	setEmpty();
    else
    {
	BufferString tmp( ptr );
	*this = tmp;
    }
    return *this;
}


BufferString& BufferString::insertAt( idx_type atidx, const char* string )
{
    const size_type cursz = size();
		// Had to do this to avoid weird compiler bug
    if ( atidx >= cursz )	// i.e. do not replace cursz with size() ...!
	{ replaceAt( atidx, string ); return *this; }
    if ( !string || !*string )
	return *this;

    if ( atidx < 0 )
    {
	const size_type lenstr = strLength( string );
	if ( atidx <= -lenstr )
	    return *this;
	string += -atidx;
	atidx = 0;
    }

    BufferString rest( buf_ + atidx );
    *(buf_ + atidx) = '\0';
    *this += string;
    *this += rest;
    return *this;
}


BufferString& BufferString::replaceAt( idx_type atidx, const char* string,
					bool cut )
{
    const size_type strsz = string ? strLength(string) : 0;
    size_type cursz = size();
    const size_type nrtopad = atidx - cursz;
    if ( nrtopad > 0 )
    {
	const size_type newsz = cursz + nrtopad + strsz + 1;
	setBufSize( newsz );
	mDoArrayPtrOperation( char, buf_+cursz, = ' ', atidx, ++ );
	buf_[atidx] = '\0';
	cursz = newsz;
    }

    if ( strsz > 0 )
    {
	if ( atidx + strsz >= cursz )
	{
	    cursz = atidx + strsz + 1;
	    setBufSize( cursz );
	    buf_[cursz-1] = '\0';
	}

	for ( idx_type idx=0; idx<strsz; idx++ )
	{
	    const idx_type replidx = atidx + idx;
	    if ( replidx >= 0 )
		buf_[replidx] = *(string + idx);
	}
    }

    if ( cut )
    {
	setBufSize( atidx + strsz + 1 );
	buf_[atidx + strsz] = '\0';
    }

    return *this;
}


BufferString& BufferString::toLower()
{
    char* ptr = getCStr();
    while ( *ptr )
    {
	*ptr = tolower(*ptr);
        ptr++;
    }
    return *this;
}


BufferString& BufferString::toUpper( bool onlyfirstchar )
{
    char* ptr = getCStr();
    while ( *ptr )
    {
	*ptr = toupper(*ptr);
	if ( onlyfirstchar )
	    break;
        ptr++;
    }
    return *this;
}


BufferString& BufferString::embed( char s, char e )
{
    char sbuf[2]; sbuf[0] = s; sbuf[1] = '\0';
    char ebuf[2]; ebuf[0] = e; ebuf[1] = '\0';
    *this = BufferString( sbuf, str(), ebuf );
    return *this;
}


BufferString& BufferString::unEmbed( char s, char e )
{
    if ( isEmpty() )
	return *this;

    char* bufptr = getCStr();
    char* startptr = bufptr;
    if ( *startptr == s )
	startptr++;

    char* endptr = startptr;
    if ( !*endptr )
	setEmpty();
    else
    {
	while ( *endptr ) endptr++;
	endptr--;
	if ( *endptr == e )
	    *endptr = '\0';

	if ( startptr != bufptr )
	{
	    BufferString tmp( startptr );
	    *this = tmp;
	}
    }

    return *this;
}


BufferString& BufferString::clean( BufferString::CleanType ct )
{
    cleanupString( getCStr(), ct > NoSpaces,
			      ct == NoSpaces || ct == NoSpecialChars,
			      ct != OnlyAlphaNum );
    return *this;
}


static BufferString emptybufferstring( 1, true );

const BufferString& BufferString::empty()
{
    return emptybufferstring;
}


void BufferString::init()
{
    len_ = minlen_;
    if ( len_ < 1 )
	buf_ = 0;
    else
    {
	mTryAlloc( buf_, char[len_] );
	if ( buf_ )
	    *buf_ = '\0';
    }
}


void BufferString::fill( char* output, size_type maxnrchar ) const
{
    if ( !output || maxnrchar < 1 )
	return;

    if ( !buf_ || maxnrchar < 2 )
	*output = 0;
    else
	strncpy( output, buf_, maxnrchar );
}


StringPair::StringPair( const char* compstr )
{
    SeparString sepstr( compstr, separator() );
    first() = sepstr[0];
    second() = sepstr[1];
}


const OD::String& StringPair::getCompString() const
{
    mDeclStaticString( ret );
    ret = first();
    if ( !second().isEmpty() )
	ret.add( separator() ).add( second() );
    return ret;
}


BufferStringSet::BufferStringSet( size_type nelem, const char* s )
{
    for ( size_type idx=0; idx<nelem; idx++ )
	add( s );
}



BufferStringSet::BufferStringSet( const char* arr[], size_type len )
{
    add( arr, len );
}


bool BufferStringSet::operator ==( const BufferStringSet& bss ) const
{
    if ( size() != bss.size() )
	return false;

    const size_type sz = size();
    for ( size_type idx=0; idx<sz; idx++ )
	if ( get(idx) != bss.get(idx) )
	    return false;

    return true;
}


BufferStringSet::size_type BufferStringSet::indexOf( const char* str,
						     CaseSensitivity cs ) const
{
    if ( str )
    {
	const size_type sz = size();
	for ( size_type idx=0; idx<sz; idx++ )
	{
	    const BufferString& bs = get( idx );
	    if ( bs.isEqual(str,cs) )
		return idx;
	}
    }
    return -1;
}


BufferStringSet::size_type BufferStringSet::indexOf( const GlobExpr& ge ) const
{
    const size_type sz = size();
    for ( size_type idx=0; idx<sz; idx++ )
    {
	if ( ge.matches( strs_[idx]->str() ) )
	    return idx;
    }
    return -1;
}


BufferString BufferStringSet::getDispString( size_type maxnritems,
					     bool quoted ) const
{
    const size_type sz = size();
    BufferString ret;
    if ( sz < 1 )
	{ ret.set( "-" ); return ret; }

    if ( maxnritems < 1 || maxnritems > sz )
	maxnritems = sz;

#   define mAddItm2Str(idx) \
    if ( quoted ) \
	ret.add( BufferString(get(idx)).quote() ); \
    else \
	ret.add( get(idx) )

    mAddItm2Str(0);
    for ( size_type idx=1; idx<maxnritems; idx++ )
    {
	ret.add( idx == sz-1 ? " and " : ", " );
	mAddItm2Str( idx );
    }

    if ( sz > maxnritems )
	ret.add( ", ..." );

    return ret;
}


BufferStringSet::size_type BufferStringSet::nearestMatch( const char* s,
					bool caseinsens ) const
{
    const size_type sz = size();
    if ( sz < 2 )
	return sz - 1;
    if ( !s ) s = "";

    const CaseSensitivity cs = caseinsens ? CaseInsensitive : CaseSensitive;
    TypeSet<size_type> candidates;
    if ( FixedString(s).size() > 1 )
    {
	for ( size_type idx=0; idx<sz; idx++ )
	    if ( get(idx).startsWith(s,cs) )
		candidates += idx;
	if ( candidates.isEmpty() )
	{
	    const BufferString matchstr( "*", s, "*" );
	    for ( size_type idx=0; idx<sz; idx++ )
		if ( get(idx).matches(matchstr,cs) )
		    candidates += idx;
	}
    }
    if ( candidates.isEmpty() )
	for ( size_type idx=0; idx<sz; idx++ )
	    candidates += idx;

    unsigned int mindist = mUdf(unsigned int); size_type minidx = -1;
    for ( size_type idx=0; idx<candidates.size(); idx++ )
    {
	const size_type myidx = candidates[idx];
	const unsigned int curdist
		= get(myidx).getLevenshteinDist( s, !caseinsens );
	if ( idx == 0 || curdist < mindist  )
	    { mindist = curdist; minidx = myidx; }
    }
    return minidx;
}


BufferString BufferStringSet::commonStart() const
{
    BufferString ret;
    const size_type sz = size();
    if ( sz < 1 )
	return ret;

    ret.set( get(0) );

    for ( idx_type idx=1; idx<sz; idx++ )
    {
	size_type retsz = ret.size();
	if ( retsz < 1 )
	    return ret;

	const BufferString& cur = get( idx );
	const size_type cursz = cur.size();
	if ( cursz < 1 )
	    { ret.setEmpty(); break; }
	if ( cursz < retsz )
	    { ret[cursz-1] = '\0'; retsz = cursz; }
	for ( idx_type ich=retsz-1; ich>-1; ich-- )
	{
	    if ( ret[ich] != cur[ich] )
		ret[ich] = '\0';
	}
    }

    return ret;
}


bool BufferStringSet::isSubsetOf( const BufferStringSet& bss ) const
{
    for ( size_type idx=0; idx<size(); idx++ )
    {
	if ( !bss.isPresent(strs_[idx]->buf()) )
	    return false;
    }

    return true;
}


BufferStringSet& BufferStringSet::add( const char* s )
{
    strs_.add( new BufferString(s) );
    return *this;
}


BufferStringSet& BufferStringSet::add( const OD::String& s )
{
    return add( s.str() );
}


BufferStringSet& BufferStringSet::add( const mQtclass(QString)& qstr )
{
    return add( BufferString(qstr) );
}


BufferStringSet& BufferStringSet::add( const BufferStringSet& bss,
				       bool allowdup )
{
    for ( size_type idx=0; idx<bss.size(); idx++ )
    {
	const char* s = bss.get( idx );
	if ( allowdup || !isPresent(s) )
	    add( s );
    }
    return *this;
}


BufferStringSet& BufferStringSet::add( const char* arr[], size_type len )
{
    if ( len < 0 )
	for ( size_type idx=0; arr[idx]; idx++ )
	    add( arr[idx] );
    else
	for ( size_type idx=0; idx<len; idx++ )
	    add( arr[idx] );

    return *this;
}


bool BufferStringSet::addIfNew( const char* s )
{
    if ( !isPresent(s) )
	{ add( s ); return true; }
    return false;
}


bool BufferStringSet::addIfNew( const OD::String& s )
{
    return addIfNew( s.buf() );
}


BufferStringSet& BufferStringSet::addToAll( const char* str, bool infront )
{
    if ( !str || !*str )
	return *this;

    for ( size_type idx=0; idx<size(); idx++ )
    {
	BufferString& itm = get( idx );
	if ( infront )
	    itm.insertAt( 0, str );
	else
	    itm.add( str );
    }

    return *this;
}


BufferStringSet::size_type BufferStringSet::maxLength() const
{
    size_type ret = 0;
    for ( size_type idx=0; idx<size(); idx++ )
    {
	const size_type len = get(idx).size();
	if ( len > ret )
	    ret = len;
    }
    return ret;
}


void BufferStringSet::sort( bool caseinsens, bool asc )
{
    size_type* idxs = getSortIndexes( caseinsens, asc );
    useIndexes( idxs );
    delete [] idxs;
}


void BufferStringSet::useIndexes( const size_type* idxs )
{
    const size_type sz = size();
    if ( !idxs || sz < 2 )
	return;

    ObjectSet<BufferString> tmp;
    for ( size_type idx=0; idx<sz; idx++ )
	tmp.add( strs_[idx] );

    strs_.plainErase();

    for ( size_type idx=0; idx<sz; idx++ )
	strs_.add( tmp[ idxs[idx] ] );
}


BufferStringSet::size_type* BufferStringSet::getSortIndexes(
					bool caseinsens, bool asc ) const
{
    const size_type sz = size();
    if ( sz < 1 )
	return 0;

    mGetIdxArr( size_type, idxs, sz );
    if ( !idxs || sz < 2 )
	return idxs;

    BufferStringSet uppercaseset;
    const BufferStringSet* tosort = this;
    if ( caseinsens )
    {
	tosort = &uppercaseset;
	for ( size_type idx=0; idx<sz; idx++ )
	{
	    BufferString newbs( get(idx) );
	    const size_type len = newbs.size();
	    char* buf = newbs.getCStr();
	    for ( size_type ich=0; ich<len; ich++ )
		buf[ich] = (char)toupper(buf[ich]);
	    uppercaseset.add( newbs );
	}
    }

    for ( size_type d=sz/2; d>0; d=d/2 )
	for ( size_type i=d; i<sz; i++ )
	    for ( size_type j=i-d;
		  j>=0 && tosort->get(idxs[j]) > tosort->get(idxs[j+d]); j-=d )
		Swap( idxs[j+d], idxs[j] );

    if ( !asc )
    {
	const size_type hsz = sz/2;
	for ( size_type idx=0; idx<hsz; idx++ )
	    Swap( idxs[idx], idxs[sz-idx-1] );
    }
    return idxs;
}


bool BufferStringSet::hasUniqueNames( CaseSensitivity sens ) const
{
    const idx_type lastidx = size() - 1;
    for ( idx_type idx=0; idx<lastidx; idx++ )
	if ( firstDuplicateOf(idx,sens,idx+1) >= 0 )
	    return false;
    return true;
}


BufferStringSet::size_type BufferStringSet::firstDuplicateOf(
	    size_type idx2find, CaseSensitivity sens, size_type startat ) const
{
    const size_type sz = size();
    if ( idx2find < 0 || idx2find >= sz )
	return -1;

    const BufferString& tofind = get( idx2find );
    for ( idx_type idx=startat; idx<sz; idx++ )
    {
	if ( idx == idx2find )
	    continue;

	if ( sens == CaseInsensitive )
	{
	    if ( caseInsensitiveEqual(tofind.str(),get(idx).str()) )
		return idx;
	}
	else if ( get(idx) == tofind )
	    return idx;

    }

    return -1;
}


void BufferStringSet::fillPar( IOPar& iopar ) const
{
    BufferString key;
    for ( size_type idx=0; idx<size(); idx++ )
    {
	key.set( idx );
	iopar.set( key, get(idx) );
    }
}


void BufferStringSet::usePar( const IOPar& iopar )
{
    BufferString key;
    for ( size_type idx=0; ; idx++ )
    {
	key.set( idx );
	const size_type idxof = iopar.indexOf( key );
	if ( idxof < 0 )
	    break;

	add( iopar.getValue(idx) );
    }
}


void BufferStringSet::fill( uiStringSet& res ) const
{
    res.setEmpty();

    for ( size_type idx=0; idx<size(); idx++ )
	res += toUiString( get(idx) );
}


void BufferStringSet::use( const uiStringSet& from )
{
    setEmpty();

    for ( size_type idx=0; idx<from.size(); idx++ )
	add( toString(from.get(idx)) );
}


void BufferStringSet::fill( mQtclass(QStringList)& res ) const
{
#ifndef OD_NO_QT
    res.clear();

    for ( size_type idx=0; idx<size(); idx++ )
	res.append( get(idx).str() );
#endif
}


void BufferStringSet::use( const mQtclass(QStringList)& from )
{
#ifndef OD_NO_QT
    setEmpty();

    for ( mQtclass(QStringList)::const_iterator iter = from.constBegin();
				      iter != from.constEnd(); ++iter )
	add( (*iter).toLocal8Bit().constData() );
#endif
}


BufferString BufferStringSet::cat( const char* sepstr ) const
{
    BufferString ret;
    for ( size_type idx=0; idx<size(); idx++ )
    {
	if ( idx )
	    ret.add( sepstr );
	ret.add( get(idx) );
    }
    return ret;
}


void BufferStringSet::unCat( const char* inpstr, const char* sepstr )
{
    const size_type sepstrsz = FixedString(sepstr).size();
    if ( sepstrsz < 1 )
	{ add( inpstr ); return; }

    BufferString str( inpstr );
    char* ptr = str.getCStr();

    while ( ptr && *ptr )
    {
	char* sepptr = ::firstOcc( ptr, sepstr );
	if ( sepptr )
	{
	    *sepptr = '\0';
	    sepptr += sepstrsz;
	}
	add( ptr );
	ptr = sepptr;
    }

    if ( ptr && *ptr )
	add( ptr );
}


uiStringSet BufferStringSet::getUiStringSet() const
{
    uiStringSet uistrset;
    for ( idx_type idx=0; idx<size(); idx++ )
	uistrset.add( toUiString(get(idx)) );
    return uistrset;
}
