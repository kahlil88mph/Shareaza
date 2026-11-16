// Tests for the Win32 compatibility layer itself.

#include <windows.h>
#include <winsock2.h>
#include <tchar.h>

#include <cstdio>
#include <string>
#include <thread>

static int g_nFailed = 0, g_nPassed = 0;
#define CHECK(x) do { if ( x ) ++g_nPassed; else { ++g_nFailed; printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); } } while ( 0 )

static std::wstring Fmt(const wchar_t* f, ...)
{
	wchar_t b[ 256 ];
	va_list a; va_start( a, f );
	_vsnwprintf( b, 256, f, a );
	va_end( a );
	return b;
}

static std::string FmtA(const char* f, ...)
{
	char b[ 256 ];
	va_list a; va_start( a, f );
	_vsnprintf_win( b, 256, f, a );
	va_end( a );
	return b;
}

int main()
{
	// Windows widths
	CHECK( sizeof( DWORD ) == 4 && sizeof( LONG ) == 4 && sizeof( WORD ) == 2 );

	// printf with Windows semantics
	CHECK( Fmt( L"%s=%d", L"port", 6346 ) == L"port=6346" );
	CHECK( Fmt( L"[%S]", "narrow" ) == L"[narrow]" );
	CHECK( Fmt( L"%c%c", L'o', L'k' ) == L"ok" );
	CHECK( Fmt( L"%I64u", (unsigned long long)1 << 40 ) == L"1099511627776" );
	CHECK( Fmt( L"%lu|%lx", (DWORD)4000000000u, (DWORD)0xBEEF ) == L"4000000000|beef" );
	CHECK( Fmt( L"%-5s|%5.1f%%", L"ab", 2.25 ) == L"ab   |  2.2%" || Fmt( L"%-5s|%5.1f%%", L"ab", 2.25 ) == L"ab   |  2.3%" );
	CHECK( FmtA( "%s %S", "a", L"b" ) == "a b" );
	CHECK( FmtA( "%Iu", (size_t)77 ) == "77" );

	// UTF-8 and Windows-1252
	const char* pszUtf8 = "caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80";
	wchar_t w[ 32 ] = {};
	int n = MultiByteToWideChar( CP_UTF8, 0, pszUtf8, -1, w, 32 );
	CHECK( n == 9 && w[ 3 ] == 0xE9 && w[ 5 ] == 0x20AC && w[ 7 ] == 0x1F600 && w[ 8 ] == 0 );
	CHECK( MultiByteToWideChar( CP_UTF8, 0, pszUtf8, -1, NULL, 0 ) == 9 );
	char s[ 32 ] = {};
	n = WideCharToMultiByte( CP_UTF8, 0, w, -1, s, 32, NULL, NULL );
	CHECK( n == (int)strlen( pszUtf8 ) + 1 && strcmp( s, pszUtf8 ) == 0 );
	CHECK( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, "\xc3", 1, w, 32 ) == 0 );
	n = MultiByteToWideChar( CP_ACP, 0, "\x80\xe9", 2, w, 32 );
	CHECK( n == 2 && w[ 0 ] == 0x20AC && w[ 1 ] == 0xE9 );
	BOOL bDefault = FALSE;
	const wchar_t wsz[] = { 0x20AC, 0x4E2D, 0 };
	n = WideCharToMultiByte( CP_ACP, 0, wsz, 2, s, 32, NULL, &bDefault );
	CHECK( n == 2 && (BYTE)s[ 0 ] == 0x80 && s[ 1 ] == '?' && bDefault );

	// Global memory
	BYTE* p = (BYTE*)GlobalAlloc( GPTR, 100 );
	CHECK( p && GlobalSize( p ) == 100 && p[ 99 ] == 0 );
	GlobalFree( p );

	// Recursive critical sections across threads
	CRITICAL_SECTION cs;
	InitializeCriticalSection( &cs );
	EnterCriticalSection( &cs );
	EnterCriticalSection( &cs );
	bool bGot = true;
	std::thread( [&] { bGot = TryEnterCriticalSection( &cs ); } ).join();
	CHECK( ! bGot );
	LeaveCriticalSection( &cs );
	LeaveCriticalSection( &cs );
	std::thread( [&] { bGot = TryEnterCriticalSection( &cs ); if ( bGot ) LeaveCriticalSection( &cs ); } ).join();
	CHECK( bGot );
	DeleteCriticalSection( &cs );

	// Time
	const DWORD t0 = GetTickCount();
	Sleep( 20 );
	CHECK( GetTickCount() - t0 >= 19 );
	FILETIME ft; SYSTEMTIME st;
	GetSystemTimeAsFileTime( &ft );
	CHECK( FileTimeToSystemTime( &ft, &st ) && st.wYear >= 2024 );
	FILETIME ft2;
	SystemTimeToFileTime( &st, &ft2 );
	const ULONGLONG a = ( (ULONGLONG)ft.dwHighDateTime << 32 ) | ft.dwLowDateTime;
	const ULONGLONG b = ( (ULONGLONG)ft2.dwHighDateTime << 32 ) | ft2.dwLowDateTime;
	CHECK( a - b < 10000 );	// same instant, to the millisecond

	LONG v = 5;
	CHECK( InterlockedIncrement( &v ) == 6 && InterlockedCompareExchange( &v, 9, 6 ) == 6 && v == 9 );

	// FoldString MAP_COMPOSITE: accented Latin letters split into letter + mark
	{
		const wchar_t szIn[] = L"caf\x00e9 \x00C5\x0151!";
		const int nNeed = FoldString( MAP_COMPOSITE, szIn, -1, NULL, 0 );
		CHECK( nNeed == 12 );	// 8 characters, 3 of them split in two, plus the terminator
		wchar_t szOut[ 16 ] = {};
		CHECK( FoldString( MAP_COMPOSITE, szIn, -1, szOut, 16 ) == 12 );
		CHECK( std::wstring( szOut ) == L"cafe\x0301 A\x030Ao\x030B!" );
		CHECK( FoldString( MAP_COMPOSITE, szIn, -1, szOut, 5 ) == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER );
		CHECK( FoldString( MAP_FOLDDIGITS, szIn, -1, szOut, 16 ) == 0 && GetLastError() == ERROR_INVALID_FLAGS );
		CHECK( FoldString( MAP_COMPOSITE, L"\x4e2d", 1, szOut, 16 ) == 1 && szOut[ 0 ] == 0x4e2d );	// no decomposition
	}

	// Case-insensitive collation, and glibc's in6addr_any under the Windows type
	CHECK( _tcsicoll( L"apple", L"APPLE" ) == 0 && _tcsicoll( L"Apple", L"banana" ) < 0 && _tcsicoll( L"b", L"A" ) > 0 );
	IN6_ADDR oAny = in6addr_any;
	CHECK( IN6_IS_ADDR_UNSPECIFIED( &oAny ) && IN6_IS_ADDR_LOOPBACK( &in6addr_loopback ) );

	printf( "compat: %d passed, %d failed\n", g_nPassed, g_nFailed );
	return g_nFailed ? 1 : 0;
}
