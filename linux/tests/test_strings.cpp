// The engine's Strings.cpp address and text helpers on Linux.

#include "StdAfx.h"
#include "Shareaza.h"
#include "ShellIcons.h"

#include <cstdio>

static int g_nFailed = 0, g_nPassed = 0;
#define CHECK(x) do { if ( x ) ++g_nPassed; else { ++g_nFailed; printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); } } while ( 0 )

int main()
{
	theApp.m_fnLog = []( WORD, const CString& ) {};

	SOCKADDR_IN v4 = {};
	v4.sin_family = AF_INET;
	v4.sin_port = htons( 6346 );
	v4.sin_addr.s_addr = htonl( 0xC0A80102 );	// IN_ADDR keeps Windows' s_addr spelling
	CHECK( HostToString( &v4 ) == L"192.168.1.2:6346" );
	CHECK( v4.sin_addr.S_un.S_un_b.s_b1 == 192 && v4.sin_addr.S_un.S_un_b.s_b4 == 2 );

	SOCKADDR_IN6 v6 = {};
	CHECK( IPv6FromString( L"[2001:db8::1]:6347", &v6 ) && ntohs( v6.sin6_port ) == 6347 );
	CHECK( HostToString( &v6 ) == L"[2001:db8::1]:6347" );
	CHECK( IPv6ToString( &v6.sin6_addr, true ) == L"[2001:db8::1]" );
	CHECK( IPv6ToString( &v6.sin6_addr, false ) == L"2001:db8::1" );
	IN6_ADDR a = {};
	CHECK( IPv6FromString( L"fe80::1%3", &a ) && a.u.Byte[ 0 ] == 0xfe && a.u.Byte[ 15 ] == 1 );
	CHECK( ! IPv6FromString( L"not-an-address", &v6 ) );
	CHECK( ! IPv6FromString( L"[::1]:99999", &v6 ) );
	IN_ADDR z = {};
	CHECK( IPv4Or6ToString( &z, &a ) == L"[fe80::1]" );
	CHECK( IPv4Or6ToString( &v4.sin_addr, NULL ) == L"192.168.1.2" );

	// URL escaping and UTF-8 helpers
	CHECK( URLEncode( L"a b&c\x00e9" ) == L"a%20b%26c%C3%A9" );
	CHECK( URLDecode( L"a%20b%26c%C3%A9" ) == L"a b&c\x00e9" );
	const wchar_t szEmoji[] = { L'x', 0x1F600, 0 };
	CHECK( UTF8Decode( UTF8Encode( CStringW( szEmoji ) ) ) == szEmoji );
	CHECK( UTF8Encode( CStringW( szEmoji ) ) == "x\xf0\x9f\x98\x80" );

	// Case-insensitive search and lower-casing beyond ASCII
	CHECK( _tcsistr( L"Hello World", L"WORLD" ) != NULL && _tcsistr( L"abc", L"x" ) == NULL );
	CString strLower( L"\x00C9T\x00C9 \x0130" );
	ToLower( strLower );
	CHECK( strLower == L"\x00e9t\x00e9 i" );
	CString strWide( szEmoji );
	ToLower( strWide );	// characters outside the BMP pass through
	CHECK( strWide == szEmoji );

	// File types for HTTP uploads (Content-Type)
	CHECK( ShellIcons.GetMIME( L".mp3" ) == L"audio/mpeg" );
	CHECK( ShellIcons.GetMIME( L".HTML" ) == L"text/html" );
	CHECK( ShellIcons.GetMIME( L".no-such-type" ) == L"application/x-binary" );
	CHECK( ShellIcons.GetTypeString( L"\\x\\song.mp3" ) == L"mp3 (audio/mpeg)" );
	CHECK( ShellIcons.Get( L"song.mp3", 16 ) == -1 );

	// List column comparison (moved from CLiveList::SortProc for the search results)
	CHECK( CompareColumnText( L"10.0.0.2", L"9.255.255.255" ) > 0 );	// IP addresses by address
	CHECK( CompareColumnText( L"10.0.0.0/255.0.0.0", L"10.0.0.0/255.255.0.0" ) < 0 );	// then by mask
	CHECK( CompareColumnText( L"2.00 MB", L"900 KB" ) > 0 );		// sizes by value
	CHECK( CompareColumnText( L"1.5 GB", L"1600 MB" ) < 0 );
	CHECK( CompareColumnText( L"(12 sources)", L"(3 sources)" ) > 0 );	// source counts
	CHECK( CompareColumnText( L"Q10", L"Q9" ) > 0 );				// queue positions
	CHECK( CompareColumnText( L"10", L"9" ) > 0 && CompareColumnText( L"7", L"7" ) == 0 );
	CHECK( CompareColumnText( L"abc", L"ABD" ) < 0 && CompareColumnText( L"Zed", L"alpha" ) > 0 );	// text, ignoring case
	CHECK( CompareColumnText( L"x10", L"x9" ) < 0 );				// mixed text is not a number
	CHECK( CompareColumnText( L"3", L"20", TRUE ) < 0 );
	CHECK( IsColumnNumber( L"12.5 KB" ) && IsColumnNumber( L"300k~" ) && ! IsColumnNumber( L"" ) && ! IsColumnNumber( L"song.mp3" ) );

	printf( "strings: %d passed, %d failed\n", g_nPassed, g_nFailed );
	return g_nFailed ? 1 : 0;
}
