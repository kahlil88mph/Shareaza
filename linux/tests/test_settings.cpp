// The engine's own CSettings, loading defaults, saving to the Linux INI
// store (in place of the registry) and loading them back. HOME and the XDG
// folders point into a temporary directory, so nothing outside it is touched.

#include "StdAfx.h"
#include "Shareaza.h"
#include "Settings.h"
#include "Registry.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

static int g_nFailed = 0, g_nPassed = 0;
#define CHECK(x) do { if ( x ) ++g_nPassed; else { ++g_nFailed; printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); } } while ( 0 )

static bool IsDir(const std::string& s) { struct stat st; return stat( s.c_str(), &st ) == 0 && S_ISDIR( st.st_mode ); }

static std::string ReadFile(const std::string& s)
{
	std::ifstream f( s );
	std::stringstream b;
	b << f.rdbuf();
	return b.str();
}

int main()
{
	char szDir[] = "/tmp/shareaza-settings-XXXXXX";
	if ( ! mkdtemp( szDir ) ) return 2;
	const std::string sHome( szDir );
	setenv( "HOME", szDir, 1 );
	unsetenv( "XDG_CONFIG_HOME" ); unsetenv( "XDG_DATA_HOME" ); unsetenv( "XDG_DOWNLOAD_DIR" ); unsetenv( "SHAREAZA_CONFIG" );
	theApp.m_fnLog = []( WORD, const CString& ) {};

	// First run: defaults, and the folders Settings creates
	Settings.Load();
	CHECK( Settings.Library.TigerHeight == 9 );
	CHECK( Settings.Connection.InPort == 6346 );
	CHECK( Settings.Gnutella2.EnableToday == Settings.Gnutella2.EnableAlways );
	const std::string sUser = compat::ToNativePath( Settings.General.UserPath );
	CHECK( sUser == sHome + "/.config/Shareaza" );
	CHECK( IsDir( sUser + "/Data" ) );
	CHECK( compat::ToNativePath( Settings.Downloads.IncompletePath ) == sHome + "/.local/share/Shareaza/Incomplete" );
	CHECK( IsDir( sHome + "/.local/share/Shareaza/Incomplete" ) );
	CHECK( IsDir( sHome + "/Downloads/Shareaza" ) );

	// Change some values, save, check the file
	Settings.Connection.InPort = 7777;
	Settings.Library.TigerHeight = 11;
	Settings.Downloads.CompletePath = compat::ToEnginePath( ( sHome + "/Done" ).c_str() );
	Settings.Save();
	const std::string sIni = ReadFile( sHome + "/.config/shareaza/Shareaza.ini" );
	CHECK( sIni.find( "[Connection]" ) != std::string::npos && sIni.find( "InPort=7777" ) != std::string::npos );
	CHECK( sIni.find( "TigerHeight=11" ) != std::string::npos );

	// Scramble memory, load again: values come back from the file
	Settings.Connection.InPort = 1;
	Settings.Library.TigerHeight = 1;
	Settings.Load();
	CHECK( Settings.Connection.InPort == 7777 );
	CHECK( Settings.Library.TigerHeight == 11 );
	CHECK( compat::ToNativePath( Settings.Downloads.CompletePath ) == sHome + "/Done" && IsDir( sHome + "/Done" ) );

	// Strings with newlines and non-ASCII survive the INI escaping
	const CString strOdd( L"line1\nline2 \\ caf\x00e9 \x65e5\x672c" );
	CRegistry::SetString( L"Test", L"Odd", strOdd );
	CHECK( CRegistry::GetString( L"Test", L"Odd" ) == strOdd );
	CHECK( CRegistry::GetDword( L"Test", L"Missing", 42 ) == 42 );

	// Out-of-range values in the file are clamped by Normalize, as on Windows
	CRegistry::SetDword( L"Library", L"TigerHeight", 500 );
	Settings.Load();
	CHECK( Settings.Library.TigerHeight == 64 );

	system( ( "rm -rf '" + sHome + "'" ).c_str() );
	printf( "settings: %d passed, %d failed\n", g_nPassed, g_nFailed );
	return g_nFailed ? 1 : 0;
}
