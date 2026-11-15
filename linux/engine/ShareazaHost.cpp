//
// ShareazaHost.cpp (Linux engine host): the headless CShareazaApp.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include "StdAfx.h"
#include "Shareaza.h"
#include "Registry.h"

#include <sys/random.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <ctime>

namespace { CString BinaryPath(); }

CShareazaApp theApp;

CShareazaApp::CShareazaApp()
	: m_bBusy( 0 )
	, m_bInteractive( false )
	, m_bLive( false )
	, m_bClosing( false )
	, m_bLimitedConnections( false )
	, m_nLastInput( 0 )
	, m_pPacketWnd( NULL )
{
	m_strBinaryPath = BinaryPath();
	// Version of the upstream release this port tracks (Prev-I 2.7.11.3).
	m_nVersion[ 0 ] = 2; m_nVersion[ 1 ] = 7; m_nVersion[ 2 ] = 11; m_nVersion[ 3 ] = 3;
	m_sVersion.Format( L"%u.%u.%u.%u", m_nVersion[ 0 ], m_nVersion[ 1 ], m_nVersion[ 2 ], m_nVersion[ 3 ] );
	m_sVersionLong = m_sVersion + L" Linux 64-bit";
	m_sSmartAgent = L"Shareaza " + m_sVersion;
	m_sBuildDate = CString( __DATE__ );
	m_pBTVersion[ 0 ] = 'S'; m_pBTVersion[ 1 ] = 'Z';
	m_pBTVersion[ 2 ] = (BYTE)m_nVersion[ 0 ]; m_pBTVersion[ 3 ] = (BYTE)m_nVersion[ 1 ];
	m_fnLog = []( WORD nType, const CString& strLog )
	{
		static const char* const szLevel[] = { "error", "warning", "notice", "info", "debug" };
		const WORD nLevel = nType & MSG_SEVERITY_MASK;
		fprintf( stderr, "[%s] %s\n", nLevel < 5 ? szLevel[ nLevel ] : "?", (LPCSTR)CStringA( CW2A( strLog, CP_UTF8 ) ) );
	};
}

CShareazaApp::~CShareazaApp()
{
}

bool CShareazaApp::IsLogDisabled(WORD nType) const
{
	// Settings.General.LogLevel joins in once Settings is ported; until
	// then everything up to MSG_INFO is logged.
	return ( nType & MSG_SEVERITY_MASK ) > MSG_INFO;
}

void CShareazaApp::Message(WORD nType, UINT nID, ...)
{
	if ( IsLogDisabled( nType ) )
		return;
	CString strFormat;
	LoadString( strFormat, nID );
	CString strTemp;
	va_list pArgs;
	va_start( pArgs, nID );
	strTemp.FormatV( strFormat, pArgs );
	va_end( pArgs );
	PrintMessage( nType, strTemp );
}

void CShareazaApp::Message(WORD nType, LPCTSTR pszFormat, ...)
{
	if ( IsLogDisabled( nType ) )
		return;
	CString strTemp;
	va_list pArgs;
	va_start( pArgs, pszFormat );
	strTemp.FormatV( pszFormat, pArgs );
	va_end( pArgs );
	PrintMessage( nType, strTemp );
}

void CShareazaApp::PrintMessage(WORD nType, const CString& strLog)
{
	if ( m_fnLog )
		m_fnLog( nType, strLog );
}

// GeoIP lookups: not wired up yet in the Linux build.
CString CShareazaApp::GetCountryCode(IN_ADDR) const { return CString(); }
CString CShareazaApp::GetCountryName(IN_ADDR) const { return CString(); }
CString CShareazaApp::GetCountryCode(IN6_ADDR) const { return CString(); }
CString CShareazaApp::GetCountryName(IN6_ADDR) const { return CString(); }

namespace {
	CString EnvPath(const char* szVar, const char* szFallback)
	{
		const char* p = getenv( szVar );
		CStringA s;
		if ( p && *p )
			s = p;
		else
		{
			const char* h = getenv( "HOME" );
			s = CStringA( h ? h : "" ) + "/" + szFallback;
		}
		return compat::ToEnginePath( s );
	}

	CString BinaryPath()
	{
		char sz[ 4096 ];
		const ssize_t n = readlink( "/proc/self/exe", sz, sizeof( sz ) - 1 );
		sz[ n > 0 ? n : 0 ] = 0;
		return compat::ToEnginePath( sz );
	}
}

CString CShareazaApp::GetDocumentsFolder() const { return EnvPath( "XDG_DOCUMENTS_DIR", "Documents" ); }
CString CShareazaApp::GetDownloadsFolder() const { return EnvPath( "XDG_DOWNLOAD_DIR", "Downloads" ); }
CString CShareazaApp::GetAppDataFolder() const { return EnvPath( "XDG_CONFIG_HOME", ".config" ); }
CString CShareazaApp::GetLocalAppDataFolder() const { return EnvPath( "XDG_DATA_HOME", ".local/share" ); }
CDatabase* CShareazaApp::GetDatabase() const { return NULL; }

CString CShareazaApp::GetProgramFilesFolder() const
{
	if ( const char* p = getenv( "SHAREAZA_DATA_DIR" ) )
		return compat::ToEnginePath( p );
	return m_strBinaryPath.Left( m_strBinaryPath.ReverseFind( L'\\' ) );
}

BOOL CreateDirectory(LPCTSTR szPath)
{
	// Like Shareaza.cpp's: create every missing folder along the path.
	CString strPath( szPath );
	strPath.TrimRight( L'\\' );
	if ( strPath.IsEmpty() ) return FALSE;
	if ( PathIsDirectory( strPath ) ) return TRUE;
	for ( int nPos = strPath.Find( L'\\', 1 ); nPos != -1; nPos = strPath.Find( L'\\', nPos + 1 ) )
		CreateDirectoryW( strPath.Left( nPos ), NULL );
	return CreateDirectoryW( strPath, NULL ) || PathIsDirectory( strPath );
}

// ---------------------------------------------------------------------------

BOOL LoadString(CString& str, UINT nID)
{
	return str.LoadString( nID );
}

CString LoadString(UINT nID)
{
	CString str;
	str.LoadString( nID );
	return str;
}

CString GetErrorString(DWORD dwError)
{
	return CString( CA2W( strerror( (int)dwError ), CP_UTF8 ) );
}

// No main window: interface notifications are dropped. A host program
// (the remote API) observes engine state directly instead.
BOOL PostMainWndMessage(UINT, WPARAM, LPARAM)
{
	return FALSE;
}

namespace compat {
	void RandomBytes(void* p, size_t n)
	{
		BYTE* b = static_cast< BYTE* >( p );
		while ( n )
		{
			const ssize_t r = getrandom( b, n, 0 );
			if ( r < 0 )
			{
				if ( errno == EINTR ) continue;
				abort();	// no entropy source: refuse to run with predictable numbers
			}
			b += r;
			n -= (size_t)r;
		}
	}
}

template <> __int8 GetRandomNum<__int8>(const __int8& min, const __int8& max)
{
	return (__int8)GetRandomNum< unsigned __int8 >( (unsigned __int8)( min - SCHAR_MIN ), (unsigned __int8)( max - SCHAR_MIN ) ) + SCHAR_MIN;
}
template <> __int16 GetRandomNum<__int16>(const __int16& min, const __int16& max)
{
	return (__int16)( GetRandomNum< unsigned __int16 >( (unsigned __int16)( min - SHRT_MIN ), (unsigned __int16)( max - SHRT_MIN ) ) + SHRT_MIN );
}
template <> __int32 GetRandomNum<__int32>(const __int32& min, const __int32& max)
{
	return (__int32)( (__int64)GetRandomNum< unsigned __int32 >( (unsigned __int32)( (__int64)min - INT_MIN ), (unsigned __int32)( (__int64)max - INT_MIN ) ) + INT_MIN );
}
template <> __int64 GetRandomNum<__int64>(const __int64& min, const __int64& max)
{
	return (__int64)( GetRandomNum< unsigned __int64 >( (unsigned __int64)min - (unsigned __int64)LLONG_MIN, (unsigned __int64)max - (unsigned __int64)LLONG_MIN ) + (unsigned __int64)LLONG_MIN );
}

// ---------------------------------------------------------------------------
// Profile functions. WriteProfileString with a NULL value deletes the entry,
// as CWinApp does; the store keeps it as an empty string.

CString CShareazaApp::GetProfileString(LPCTSTR pszSection, LPCTSTR pszEntry, LPCTSTR pszDefault)
{
	return CRegistry::GetString( pszSection, pszEntry, pszDefault );
}

BOOL CShareazaApp::WriteProfileString(LPCTSTR pszSection, LPCTSTR pszEntry, LPCTSTR pszValue)
{
	return CRegistry::SetString( pszSection, pszEntry, pszValue ? pszValue : L"" );
}

UINT CShareazaApp::GetProfileInt(LPCTSTR pszSection, LPCTSTR pszEntry, int nDefault)
{
	return (UINT)CRegistry::GetInt( pszSection, pszEntry, nDefault );
}

BOOL CShareazaApp::WriteProfileInt(LPCTSTR pszSection, LPCTSTR pszEntry, int nValue)
{
	return CRegistry::SetInt( pszSection, pszEntry, nValue );
}

// ---------------------------------------------------------------------------
// MsgBox: there is nobody to ask. Log the question and give the answer the
// caller stored as its default (pnDefault, numbered as in StdAfx.h), or the
// safe answer: OK, Cancel, No or Ignore.

INT_PTR MsgBox(LPCTSTR lpszText, UINT nType, UINT, DWORD* pnDefault, DWORD)
{
	theApp.Message( MSG_NOTICE, L"(no interface) %s", lpszText ? lpszText : L"" );
	const DWORD nStored = pnDefault ? *pnDefault : 0;
	switch ( nType & 0x0F )
	{
	case MB_OKCANCEL:          return nStored == 1 ? IDOK : IDCANCEL;
	case MB_ABORTRETRYIGNORE:  return nStored == 1 ? IDABORT : nStored == 2 ? IDRETRY : IDIGNORE;
	case MB_YESNOCANCEL:       return nStored == 2 ? IDYES : nStored == 1 ? IDNO : IDCANCEL;
	case MB_YESNO:             return nStored == 2 ? IDYES : IDNO;
	case MB_RETRYCANCEL:       return nStored == 1 ? IDRETRY : IDCANCEL;
	case MB_CANCELTRYCONTINUE: return nStored == 2 ? IDTRYAGAIN : nStored == 3 ? IDCONTINUE : IDCANCEL;
	default:                   return IDOK;
	}
}

INT_PTR MsgBox(UINT nIDPrompt, UINT nType, UINT nIDHelp, DWORD* pnDefault, DWORD nTimer)
{
	return MsgBox( (LPCTSTR)LoadString( nIDPrompt ), nType, nIDHelp, pnDefault, nTimer );
}
