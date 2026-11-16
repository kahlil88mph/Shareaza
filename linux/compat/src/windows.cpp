//
// windows.cpp (Linux compatibility layer)
//
// POSIX implementations of the Win32 calls declared in include/windows.h.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include <windows.h>
#include <winsock2.h>
#include <tchar.h>
#include <compat/com.h>

#include <cerrno>
#include <clocale>
#include <ctime>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/syscall.h>

// ---------------------------------------------------------------------------
// Memory

namespace {
	struct GlobalHeader { SIZE_T nSize; SIZE_T nPad; };	// keeps the payload 16-byte aligned
	inline GlobalHeader* HeaderOf(HGLOBAL h) { return static_cast< GlobalHeader* >( h ) - 1; }
}

HGLOBAL GlobalAlloc(UINT uFlags, SIZE_T dwBytes)
{
	GlobalHeader* p = static_cast< GlobalHeader* >( ( uFlags & GMEM_ZEROINIT )
		? calloc( 1, sizeof( GlobalHeader ) + dwBytes )
		: malloc( sizeof( GlobalHeader ) + dwBytes ) );
	if ( ! p )
	{
		SetLastError( ERROR_NOT_ENOUGH_MEMORY );
		return NULL;
	}
	p->nSize = dwBytes;
	return p + 1;
}

HGLOBAL GlobalReAlloc(HGLOBAL hMem, SIZE_T dwBytes, UINT uFlags)
{
	if ( ! hMem )
		return GlobalAlloc( uFlags, dwBytes );
	const SIZE_T nOld = HeaderOf( hMem )->nSize;
	GlobalHeader* p = static_cast< GlobalHeader* >( realloc( HeaderOf( hMem ), sizeof( GlobalHeader ) + dwBytes ) );
	if ( ! p )
	{
		SetLastError( ERROR_NOT_ENOUGH_MEMORY );
		return NULL;
	}
	p->nSize = dwBytes;
	if ( ( uFlags & GMEM_ZEROINIT ) && dwBytes > nOld )
		memset( reinterpret_cast< BYTE* >( p + 1 ) + nOld, 0, dwBytes - nOld );
	return p + 1;
}

HGLOBAL GlobalFree(HGLOBAL hMem)
{
	if ( hMem )
		free( HeaderOf( hMem ) );
	return NULL;
}

SIZE_T GlobalSize(HGLOBAL hMem)
{
	return hMem ? HeaderOf( hMem )->nSize : 0;
}

// ---------------------------------------------------------------------------
// Errors

namespace { thread_local DWORD g_nLastError = 0; }

DWORD GetLastError() { return g_nLastError; }
void SetLastError(DWORD dwError) { g_nLastError = dwError; }

// ---------------------------------------------------------------------------
// Time

namespace {
	// 100 ns intervals between 1601-01-01 (FILETIME epoch) and 1970-01-01.
	const ULONGLONG FILETIME_UNIX_EPOCH = 116444736000000000ull;

	ULONGLONG MonotonicMs()
	{
		timespec ts;
		clock_gettime( CLOCK_MONOTONIC, &ts );
		return (ULONGLONG)ts.tv_sec * 1000u + (ULONGLONG)ts.tv_nsec / 1000000u;
	}

	void UnixToSystemTime(time_t t, long nMs, bool bLocal, LPSYSTEMTIME st)
	{
		tm v = {};
		if ( bLocal )
			localtime_r( &t, &v );
		else
			gmtime_r( &t, &v );
		st->wYear = (WORD)( v.tm_year + 1900 );
		st->wMonth = (WORD)( v.tm_mon + 1 );
		st->wDayOfWeek = (WORD)v.tm_wday;
		st->wDay = (WORD)v.tm_mday;
		st->wHour = (WORD)v.tm_hour;
		st->wMinute = (WORD)v.tm_min;
		st->wSecond = (WORD)v.tm_sec;
		st->wMilliseconds = (WORD)nMs;
	}
}

DWORD GetTickCount() { return (DWORD)MonotonicMs(); }
ULONGLONG GetTickCount64() { return MonotonicMs(); }

void Sleep(DWORD dwMilliseconds)
{
	if ( dwMilliseconds == INFINITE )
		for ( ;; ) pause();
	timespec ts = { (time_t)( dwMilliseconds / 1000 ), (long)( dwMilliseconds % 1000 ) * 1000000L };
	while ( nanosleep( &ts, &ts ) == -1 && errno == EINTR ) {}
}

void GetSystemTimeAsFileTime(LPFILETIME ft)
{
	timespec ts;
	clock_gettime( CLOCK_REALTIME, &ts );
	const ULONGLONG n = FILETIME_UNIX_EPOCH + (ULONGLONG)ts.tv_sec * 10000000u + (ULONGLONG)ts.tv_nsec / 100u;
	ft->dwLowDateTime = (DWORD)n;
	ft->dwHighDateTime = (DWORD)( n >> 32 );
}

void GetSystemTime(LPSYSTEMTIME st)
{
	timespec ts;
	clock_gettime( CLOCK_REALTIME, &ts );
	UnixToSystemTime( ts.tv_sec, ts.tv_nsec / 1000000L, false, st );
}

void GetLocalTime(LPSYSTEMTIME st)
{
	timespec ts;
	clock_gettime( CLOCK_REALTIME, &ts );
	UnixToSystemTime( ts.tv_sec, ts.tv_nsec / 1000000L, true, st );
}

BOOL FileTimeToSystemTime(const FILETIME* ft, LPSYSTEMTIME st)
{
	const ULONGLONG n = ( (ULONGLONG)ft->dwHighDateTime << 32 ) | ft->dwLowDateTime;
	const LONGLONG nUnix100ns = (LONGLONG)( n - FILETIME_UNIX_EPOCH );
	LONGLONG nSec = nUnix100ns / 10000000;
	LONGLONG nRem = nUnix100ns % 10000000;
	if ( nRem < 0 ) { nRem += 10000000; --nSec; }
	UnixToSystemTime( (time_t)nSec, (long)( nRem / 10000 ), false, st );
	return TRUE;
}

BOOL SystemTimeToFileTime(const SYSTEMTIME* st, LPFILETIME ft)
{
	tm v = {};
	v.tm_year = st->wYear - 1900;
	v.tm_mon = st->wMonth - 1;
	v.tm_mday = st->wDay;
	v.tm_hour = st->wHour;
	v.tm_min = st->wMinute;
	v.tm_sec = st->wSecond;
	const time_t t = timegm( &v );
	const ULONGLONG n = FILETIME_UNIX_EPOCH + (ULONGLONG)( (LONGLONG)t * 10000000 ) + (ULONGLONG)st->wMilliseconds * 10000u;
	ft->dwLowDateTime = (DWORD)n;
	ft->dwHighDateTime = (DWORD)( n >> 32 );
	return TRUE;
}

BOOL QueryPerformanceCounter(LARGE_INTEGER* p)
{
	timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	p->QuadPart = (LONGLONG)ts.tv_sec * 1000000000 + ts.tv_nsec;
	return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER* p)
{
	p->QuadPart = 1000000000;
	return TRUE;
}

// ---------------------------------------------------------------------------
// Critical sections (recursive mutexes, as on Windows)

void InitializeCriticalSection(LPCRITICAL_SECTION cs)
{
	pthread_mutexattr_t a;
	pthread_mutexattr_init( &a );
	pthread_mutexattr_settype( &a, PTHREAD_MUTEX_RECURSIVE );
	pthread_mutex_init( &cs->m_mutex, &a );
	pthread_mutexattr_destroy( &a );
	cs->m_owner = 0;
	cs->RecursionCount = 0;
}

BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION cs, DWORD)
{
	InitializeCriticalSection( cs );
	return TRUE;
}

void DeleteCriticalSection(LPCRITICAL_SECTION cs) { pthread_mutex_destroy( &cs->m_mutex ); }

void EnterCriticalSection(LPCRITICAL_SECTION cs)
{
	pthread_mutex_lock( &cs->m_mutex );
	cs->m_owner = pthread_self();
	++cs->RecursionCount;
}

BOOL TryEnterCriticalSection(LPCRITICAL_SECTION cs)
{
	if ( pthread_mutex_trylock( &cs->m_mutex ) != 0 )
		return FALSE;
	cs->m_owner = pthread_self();
	++cs->RecursionCount;
	return TRUE;
}

void LeaveCriticalSection(LPCRITICAL_SECTION cs)
{
	if ( --cs->RecursionCount == 0 )
		cs->m_owner = 0;
	pthread_mutex_unlock( &cs->m_mutex );
}

DWORD GetCurrentThreadId() { return (DWORD)syscall( SYS_gettid ); }
DWORD GetCurrentProcessId() { return (DWORD)getpid(); }

// ---------------------------------------------------------------------------
// Character conversion

namespace {
	// Windows-1252 bytes 0x80-0x9F; 0 marks the five undefined bytes, which
	// Windows maps to the matching C1 control code.
	const wchar_t g_cp1252[ 32 ] = {
		0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
		0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
		0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
		0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178 };

	inline wchar_t From1252(unsigned char c)
	{
		if ( c >= 0x80 && c < 0xA0 && g_cp1252[ c - 0x80 ] )
			return g_cp1252[ c - 0x80 ];
		return c;
	}

	inline int To1252(wchar_t c)
	{
		if ( c < 0x80 || ( c >= 0xA0 && c < 0x100 ) )
			return (int)c;
		for ( int i = 0; i < 32; ++i )
			if ( g_cp1252[ i ] == c )
				return 0x80 + i;
		if ( c >= 0x80 && c < 0xA0 && ! g_cp1252[ c - 0x80 ] )
			return (int)c;
		return -1;
	}

	// Decode UTF-8 into code points. Invalid sequences give U+FFFD and set bBad.
	void DecodeUtf8(const unsigned char* p, size_t n, std::wstring& out, bool& bBad)
	{
		size_t i = 0;
		while ( i < n )
		{
			const unsigned c = p[ i ];
			unsigned cp, need;
			if ( c < 0x80 ) { out += (wchar_t)c; ++i; continue; }
			else if ( ( c & 0xE0 ) == 0xC0 ) { cp = c & 0x1F; need = 1; }
			else if ( ( c & 0xF0 ) == 0xE0 ) { cp = c & 0x0F; need = 2; }
			else if ( ( c & 0xF8 ) == 0xF0 ) { cp = c & 0x07; need = 3; }
			else { out += (wchar_t)0xFFFD; bBad = true; ++i; continue; }
			size_t j = 1;
			for ( ; j <= need && i + j < n; ++j )
			{
				if ( ( p[ i + j ] & 0xC0 ) != 0x80 ) break;
				cp = ( cp << 6 ) | ( p[ i + j ] & 0x3F );
			}
			if ( j <= need )
			{
				out += (wchar_t)0xFFFD; bBad = true; i += j; continue;
			}
			static const unsigned nMin[ 4 ] = { 0, 0x80, 0x800, 0x10000 };
			if ( cp < nMin[ need ] || cp > 0x10FFFF || ( cp >= 0xD800 && cp <= 0xDFFF ) )
			{
				out += (wchar_t)0xFFFD; bBad = true;
			}
			else
				out += (wchar_t)cp;
			i += need + 1;
		}
	}

	void EncodeUtf8(const wchar_t* p, size_t n, std::string& out)
	{
		for ( size_t i = 0; i < n; ++i )
		{
			unsigned cp = (unsigned)p[ i ];
			if ( cp > 0x10FFFF || ( cp >= 0xD800 && cp <= 0xDFFF ) )
				cp = 0xFFFD;
			if ( cp < 0x80 )
				out += (char)cp;
			else if ( cp < 0x800 )
			{
				out += (char)( 0xC0 | ( cp >> 6 ) );
				out += (char)( 0x80 | ( cp & 0x3F ) );
			}
			else if ( cp < 0x10000 )
			{
				out += (char)( 0xE0 | ( cp >> 12 ) );
				out += (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
				out += (char)( 0x80 | ( cp & 0x3F ) );
			}
			else
			{
				out += (char)( 0xF0 | ( cp >> 18 ) );
				out += (char)( 0x80 | ( ( cp >> 12 ) & 0x3F ) );
				out += (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
				out += (char)( 0x80 | ( cp & 0x3F ) );
			}
		}
	}
}

UINT GetACP() { return 1252; }
UINT GetOEMCP() { return 437; }

int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR psz, int cb, LPWSTR pwsz, int cch)
{
	if ( ! psz || cb == 0 || cch < 0 )
	{
		SetLastError( ERROR_INVALID_PARAMETER );
		return 0;
	}
	const size_t n = ( cb < 0 ) ? strlen( psz ) + 1 : (size_t)cb;
	std::wstring w;
	if ( CodePage == CP_UTF8 )
	{
		bool bBad = false;
		DecodeUtf8( reinterpret_cast< const unsigned char* >( psz ), n, w, bBad );
		if ( bBad && ( dwFlags & MB_ERR_INVALID_CHARS ) )
		{
			SetLastError( ERROR_NO_UNICODE_TRANSLATION );
			return 0;
		}
	}
	else
	{
		w.reserve( n );
		for ( size_t i = 0; i < n; ++i )
			w += From1252( (unsigned char)psz[ i ] );
	}
	if ( cch == 0 )
		return (int)w.size();
	if ( (size_t)cch < w.size() )
	{
		SetLastError( ERROR_INSUFFICIENT_BUFFER );
		return 0;
	}
	wmemcpy( pwsz, w.data(), w.size() );
	return (int)w.size();
}

int WideCharToMultiByte(UINT CodePage, DWORD, LPCWSTR pwsz, int cch, LPSTR psz, int cb,
	LPCSTR pszDefault, LPBOOL pbUsedDefault)
{
	if ( ! pwsz || cch == 0 || cb < 0 )
	{
		SetLastError( ERROR_INVALID_PARAMETER );
		return 0;
	}
	const size_t n = ( cch < 0 ) ? wcslen( pwsz ) + 1 : (size_t)cch;
	std::string s;
	if ( pbUsedDefault )
		*pbUsedDefault = FALSE;
	if ( CodePage == CP_UTF8 )
		EncodeUtf8( pwsz, n, s );
	else
	{
		s.reserve( n );
		const char cDefault = pszDefault ? *pszDefault : '?';
		for ( size_t i = 0; i < n; ++i )
		{
			const int c = To1252( pwsz[ i ] );
			if ( c < 0 )
			{
				s += cDefault;
				if ( pbUsedDefault )
					*pbUsedDefault = TRUE;
			}
			else
				s += (char)c;
		}
	}
	if ( cb == 0 )
		return (int)s.size();
	if ( (size_t)cb < s.size() )
	{
		SetLastError( ERROR_INSUFFICIENT_BUFFER );
		return 0;
	}
	memcpy( psz, s.data(), s.size() );
	return (int)s.size();
}

// ---------------------------------------------------------------------------
// Case conversion

wchar_t* _wcslwr(wchar_t* s) { for ( wchar_t* p = s; *p; ++p ) *p = towlower( *p ); return s; }
wchar_t* _wcsupr(wchar_t* s) { for ( wchar_t* p = s; *p; ++p ) *p = towupper( *p ); return s; }
char* _strlwr(char* s) { for ( char* p = s; *p; ++p ) *p = (char)tolower( (unsigned char)*p ); return s; }
char* _strupr(char* s) { for ( char* p = s; *p; ++p ) *p = (char)toupper( (unsigned char)*p ); return s; }

// ---------------------------------------------------------------------------
// printf format translation
//
// Windows:  wide format: %s %c wide,  %S %C narrow,  %hs narrow, %ls %ws wide
//           narrow format: %s %c narrow, %S %C wide, %hs narrow, %ls %ws wide
//           %ld/%lu/%lx are 32-bit (long is 32-bit), %I64d is 64-bit, %Id is pointer-sized
// glibc:    %s %c narrow, %ls %lc wide (in both narrow and wide formats)
//           %ld is 64-bit on LP64

namespace compat {

template< typename Ch >
static void TranslateFormatT(const Ch* in, Ch* out, size_t nOut, bool bWide)
{
	size_t o = 0;
	auto put = [&]( Ch c ) { if ( o + 1 < nOut ) out[ o++ ] = c; };
	while ( *in )
	{
		if ( *in != '%' ) { put( *in++ ); continue; }
		put( *in++ );
		if ( *in == '%' ) { put( *in++ ); continue; }
		// flags, width, precision
		while ( *in > 0 && *in < 128 && strchr( "-+ #0", (char)*in ) ) put( *in++ );
		while ( *in == '*' || ( *in >= '0' && *in <= '9' ) ) put( *in++ );
		if ( *in == '.' )
		{
			put( *in++ );
			while ( *in == '*' || ( *in >= '0' && *in <= '9' ) ) put( *in++ );
		}
		// length
		enum { None, Short, Long, LongLong, Size, Wide } len = None;
		for ( ;; )
		{
			if ( in[ 0 ] == 'I' && in[ 1 ] == '6' && in[ 2 ] == '4' ) { len = LongLong; in += 3; }
			else if ( in[ 0 ] == 'I' && in[ 1 ] == '3' && in[ 2 ] == '2' ) { in += 3; }
			else if ( in[ 0 ] == 'I' ) { len = Size; ++in; }
			else if ( in[ 0 ] == 'l' && in[ 1 ] == 'l' ) { len = LongLong; in += 2; }
			else if ( in[ 0 ] == 'h' && in[ 1 ] == 'h' ) { put( 'h' ); put( 'h' ); in += 2; }
			else if ( in[ 0 ] == 'l' ) { len = Long; ++in; }
			else if ( in[ 0 ] == 'w' ) { len = Wide; ++in; }
			else if ( in[ 0 ] == 'h' ) { len = Short; ++in; }
			else if ( in[ 0 ] == 'L' || in[ 0 ] == 'j' || in[ 0 ] == 'z' || in[ 0 ] == 't' ) { put( *in++ ); }
			else break;
		}
		const Ch t = *in;
		if ( ! t ) break;
		++in;
		switch ( t )
		{
		case 's': case 'c':
			if ( len == Short ) { }
			else if ( len == Long || len == Wide || bWide ) put( 'l' );
			put( t );
			break;
		case 'S': case 'C':
			if ( len == Short ) { }
			else if ( len == Long || len == Wide || ! bWide ) put( 'l' );
			put( t == 'S' ? 's' : 'c' );
			break;
		case 'd': case 'i': case 'o': case 'u': case 'x': case 'X':
			if ( len == LongLong ) { put( 'l' ); put( 'l' ); }
			else if ( len == Size ) put( 'z' );
			else if ( len == Short ) put( 'h' );
			// Long: Windows long is 32-bit, so no modifier
			put( t );
			break;
		case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case 'a': case 'A':
			if ( len == Long ) put( 'l' );	// %lf: double in scanf, same as %f in printf
			put( t );
			break;
		case 'p': case 'n':
			put( t );
			break;
		default:
			put( t );
		}
	}
	out[ o < nOut ? o : nOut - 1 ] = 0;
}

void TranslateFormat(const wchar_t* in, wchar_t* out, size_t nOut, bool bWide) { TranslateFormatT( in, out, nOut, bWide ); }
void TranslateFormat(const char* in, char* out, size_t nOut, bool bWide) { TranslateFormatT( in, out, nOut, bWide ); }

} // namespace compat

int _vsnwprintf(wchar_t* buf, size_t count, const wchar_t* fmt, va_list args)
{
	std::vector< wchar_t > f( wcslen( fmt ) * 2 + 16 );
	compat::TranslateFormat( fmt, f.data(), f.size(), true );
	return vswprintf( buf, count, f.data(), args );
}

int _snwprintf(wchar_t* buf, size_t count, const wchar_t* fmt, ...)
{
	va_list a;
	va_start( a, fmt );
	const int n = _vsnwprintf( buf, count, fmt, a );
	va_end( a );
	return n;
}

int _vscwprintf(const wchar_t* fmt, va_list args)
{
	// vswprintf cannot report the needed size, so grow until it fits.
	std::vector< wchar_t > f( wcslen( fmt ) * 2 + 16 );
	compat::TranslateFormat( fmt, f.data(), f.size(), true );
	for ( size_t n = 256; n < ( 1u << 26 ); n *= 2 )
	{
		std::vector< wchar_t > b( n );
		va_list c;
		va_copy( c, args );
		const int r = vswprintf( b.data(), n, f.data(), c );
		va_end( c );
		if ( r >= 0 )
			return r;
	}
	return -1;
}

int _vsnprintf_win(char* buf, size_t count, const char* fmt, va_list args)
{
	std::vector< char > f( strlen( fmt ) * 2 + 16 );
	compat::TranslateFormat( fmt, f.data(), f.size(), false );
	return vsnprintf( buf, count, f.data(), args );
}

int _vscprintf(const char* fmt, va_list args)
{
	std::vector< char > f( strlen( fmt ) * 2 + 16 );
	compat::TranslateFormat( fmt, f.data(), f.size(), false );
	return vsnprintf( NULL, 0, f.data(), args );
}

// scanf with Windows semantics (%lu is 32-bit, %I64u is 64-bit).
int compat_swscanf(const wchar_t* buf, const wchar_t* fmt, ...)
{
	std::vector< wchar_t > f( wcslen( fmt ) * 2 + 16 );
	compat::TranslateFormat( fmt, f.data(), f.size(), true );
	va_list a;
	va_start( a, fmt );
	const int n = vswscanf( buf, f.data(), a );
	va_end( a );
	return n;
}

// Winsock error state maps onto errno.
int WSAGetLastError() { return errno; }
void WSASetLastError(int nError) { errno = nError; }

const IID IID_IUnknown = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

// ---------------------------------------------------------------------------
// Locale: towlower, wcscasecmp and friends only know ASCII in the "C" locale.
// Use C.UTF-8 for character classes, before any other static initialiser.

namespace {
	struct LocaleInit
	{
		LocaleInit()
		{
			if ( ! setlocale( LC_CTYPE, "C.UTF-8" ) )
				setlocale( LC_CTYPE, "" );
		}
	};
	__attribute__(( init_priority( 101 ) )) LocaleInit g_oLocaleInit;
}

BOOL GetStringTypeW(DWORD dwInfoType, LPCWSTR p, int n, LPWORD pType)
{
	if ( ! p || ! pType ) return FALSE;
	if ( n < 0 ) n = (int)wcslen( p ) + 1;
	for ( int i = 0; i < n; ++i )
	{
		const unsigned c = (unsigned)p[ i ];
		WORD t = 0;
		if ( dwInfoType == CT_CTYPE3 )
		{
			if ( iswalpha( p[ i ] ) ) t |= C3_ALPHA;
			if ( ( c >= 0x0300 && c <= 0x036F ) || ( c >= 0x3099 && c <= 0x309A ) ) t |= C3_NONSPACING | C3_DIACRITIC;
			if ( c == 0x309B || c == 0x309C || c == 0xFF9E || c == 0xFF9F ) t |= C3_DIACRITIC;
			if ( c >= 0x3041 && c <= 0x309F ) t |= C3_HIRAGANA | C3_ALPHA;
			if ( ( c >= 0x30A0 && c <= 0x30FF ) || ( c >= 0x31F0 && c <= 0x31FF ) ) t |= C3_KATAKANA | C3_ALPHA;
			if ( c >= 0xFF66 && c <= 0xFF9F ) t |= C3_KATAKANA | C3_HALFWIDTH | C3_ALPHA;
			if ( ( c >= 0x4E00 && c <= 0x9FFF ) || ( c >= 0x3400 && c <= 0x4DBF ) || ( c >= 0xF900 && c <= 0xFAFF ) ||
				( c >= 0x20000 && c <= 0x3FFFF ) || c == 0x3005 || c == 0x3007 )
				t |= C3_IDEOGRAPH | C3_ALPHA;
			if ( c >= 0xFF01 && c <= 0xFF5E ) t |= C3_FULLWIDTH;
			if ( iswpunct( p[ i ] ) ) t |= C3_SYMBOL;
		}
		else if ( dwInfoType == CT_CTYPE1 )
		{
			if ( iswupper( p[ i ] ) ) t |= 0x0001;
			if ( iswlower( p[ i ] ) ) t |= 0x0002;
			if ( iswdigit( p[ i ] ) ) t |= 0x0004;
			if ( iswspace( p[ i ] ) ) t |= 0x0008;
			if ( iswpunct( p[ i ] ) ) t |= 0x0010;
			if ( iswcntrl( p[ i ] ) ) t |= 0x0020;
			if ( p[ i ] == L' ' || p[ i ] == L'\t' ) t |= 0x0040;
			if ( iswxdigit( p[ i ] ) ) t |= 0x0080;
			if ( iswalpha( p[ i ] ) ) t |= 0x0100;
		}
		pType[ i ] = t;
	}
	return TRUE;
}

DWORD CharLowerBuffW(LPWSTR p, DWORD n) { for ( DWORD i = 0; i < n; ++i ) p[ i ] = towlower( p[ i ] ); return n; }
DWORD CharUpperBuffW(LPWSTR p, DWORD n) { for ( DWORD i = 0; i < n; ++i ) p[ i ] = towupper( p[ i ] ); return n; }
LPWSTR CharLowerW(LPWSTR p) { return _wcslwr( p ); }
LPWSTR CharUpperW(LPWSTR p) { return _wcsupr( p ); }

int compat_sprintf_s(char* buf, size_t n, const char* fmt, ...)
{
	va_list a; va_start( a, fmt );
	const int r = _vsnprintf_win( buf, n, fmt, a );
	va_end( a );
	return r;
}

int compat_sscanf(const char* buf, const char* fmt, ...)
{
	std::vector< char > f( strlen( fmt ) * 2 + 16 );
	compat::TranslateFormat( fmt, f.data(), f.size(), false );
	va_list a; va_start( a, fmt );
	const int r = vsscanf( buf, f.data(), a );
	va_end( a );
	return r;
}

// ---------------------------------------------------------------------------
// WSAStringToAddress / WSAAddressToString

INT WSAStringToAddressW(LPWSTR pszAddress, INT nFamily, LPWSAPROTOCOL_INFOW, LPSOCKADDR pAddress, LPINT pnLength)
{
	if ( ! pszAddress || ! pAddress || ! pnLength ) { WSASetLastError( EINVAL ); return SOCKET_ERROR; }
	char sz[ 128 ];
	const int n = WideCharToMultiByte( CP_UTF8, 0, pszAddress, -1, sz, sizeof( sz ), NULL, NULL );
	if ( n <= 0 ) { WSASetLastError( EINVAL ); return SOCKET_ERROR; }
	std::string s( sz );
	unsigned long nPort = 0;
	if ( nFamily == AF_INET6 )
	{
		uint32_t nScope = 0;
		if ( ! s.empty() && s[ 0 ] == '[' )
		{
			const size_t e = s.find( ']' );
			if ( e == std::string::npos ) { WSASetLastError( EINVAL ); return SOCKET_ERROR; }
			if ( e + 1 < s.size() )
			{
				if ( s[ e + 1 ] != ':' ) { WSASetLastError( EINVAL ); return SOCKET_ERROR; }
				nPort = strtoul( s.c_str() + e + 2, NULL, 10 );
			}
			s = s.substr( 1, e - 1 );
		}
		const size_t nPct = s.find( '%' );
		if ( nPct != std::string::npos ) { nScope = (uint32_t)strtoul( s.c_str() + nPct + 1, NULL, 10 ); s.resize( nPct ); }
		if ( *pnLength < (INT)sizeof( SOCKADDR_IN6 ) ) { *pnLength = sizeof( SOCKADDR_IN6 ); WSASetLastError( EFAULT ); return SOCKET_ERROR; }
		SOCKADDR_IN6* p = reinterpret_cast< SOCKADDR_IN6* >( pAddress );
		memset( p, 0, sizeof( *p ) );
		if ( inet_pton( AF_INET6, s.c_str(), &p->sin6_addr ) != 1 || nPort > 65535 ) { WSASetLastError( EINVAL ); return SOCKET_ERROR; }
		p->sin6_family = AF_INET6;
		p->sin6_port = htons( (u_short)nPort );
		p->sin6_scope_id = nScope;
		*pnLength = sizeof( SOCKADDR_IN6 );
		return 0;
	}
	if ( nFamily == AF_INET )
	{
		const size_t nColon = s.find( ':' );
		if ( nColon != std::string::npos ) { nPort = strtoul( s.c_str() + nColon + 1, NULL, 10 ); s.resize( nColon ); }
		if ( *pnLength < (INT)sizeof( SOCKADDR_IN ) ) { *pnLength = sizeof( SOCKADDR_IN ); WSASetLastError( EFAULT ); return SOCKET_ERROR; }
		SOCKADDR_IN* p = reinterpret_cast< SOCKADDR_IN* >( pAddress );
		memset( p, 0, sizeof( *p ) );
		if ( inet_pton( AF_INET, s.c_str(), &p->sin_addr ) != 1 || nPort > 65535 ) { WSASetLastError( EINVAL ); return SOCKET_ERROR; }
		p->sin_family = AF_INET;
		p->sin_port = htons( (u_short)nPort );
		*pnLength = sizeof( SOCKADDR_IN );
		return 0;
	}
	WSASetLastError( EAFNOSUPPORT );
	return SOCKET_ERROR;
}

INT WSAAddressToStringW(LPSOCKADDR pAddress, DWORD, LPWSAPROTOCOL_INFOW, LPWSTR pszOut, LPDWORD pnOut)
{
	if ( ! pAddress || ! pnOut ) { WSASetLastError( EINVAL ); return SOCKET_ERROR; }
	char sz[ 128 ], szAddr[ INET6_ADDRSTRLEN ];
	if ( pAddress->sa_family == AF_INET6 )
	{
		const SOCKADDR_IN6* p = reinterpret_cast< const SOCKADDR_IN6* >( pAddress );
		inet_ntop( AF_INET6, &p->sin6_addr, szAddr, sizeof( szAddr ) );
		std::string s( szAddr );
		if ( p->sin6_scope_id ) s += "%" + std::to_string( p->sin6_scope_id );
		if ( p->sin6_port ) s = "[" + s + "]:" + std::to_string( ntohs( p->sin6_port ) );
		snprintf( sz, sizeof( sz ), "%s", s.c_str() );
	}
	else if ( pAddress->sa_family == AF_INET )
	{
		const SOCKADDR_IN* p = reinterpret_cast< const SOCKADDR_IN* >( pAddress );
		inet_ntop( AF_INET, &p->sin_addr, szAddr, sizeof( szAddr ) );
		if ( p->sin_port ) snprintf( sz, sizeof( sz ), "%s:%u", szAddr, (unsigned)ntohs( p->sin_port ) );
		else snprintf( sz, sizeof( sz ), "%s", szAddr );
	}
	else { WSASetLastError( EAFNOSUPPORT ); return SOCKET_ERROR; }
	const DWORD nNeed = (DWORD)strlen( sz ) + 1;	// characters, including the NUL
	if ( ! pszOut || *pnOut < nNeed ) { *pnOut = nNeed; WSASetLastError( EFAULT ); return SOCKET_ERROR; }
	for ( DWORD i = 0; i < nNeed; ++i ) pszOut[ i ] = (wchar_t)(unsigned char)sz[ i ];
	*pnOut = nNeed;
	return 0;
}
