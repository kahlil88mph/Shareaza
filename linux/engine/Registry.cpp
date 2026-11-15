//
// Registry.cpp (Linux engine host)
//
// CRegistry for Linux. On Windows the engine keeps its settings under
// HKEY_CURRENT_USER\Software\Shareaza\Shareaza; here they live in an INI
// file, $XDG_CONFIG_HOME/shareaza/Shareaza.ini (or the file named by
// $SHAREAZA_CONFIG). Sections and names match the registry's, so a .reg
// export from Windows maps over one to one. Each change rewrites the file
// atomically (write to a temporary file, then rename).
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include "StdAfx.h"
#include "Shareaza.h"
#include "Registry.h"

#include <map>
#include <mutex>
#include <string>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

namespace {

struct NoCaseLess
{
	bool operator()(const std::string& a, const std::string& b) const { return strcasecmp( a.c_str(), b.c_str() ) < 0; }
};
typedef std::map< std::string, std::string, NoCaseLess > Values;	// the registry is case-insensitive
typedef std::map< std::string, Values, NoCaseLess > Sections;

class CStore
{
public:
	static CStore& Instance() { static CStore s; return s; }

	bool Get(const std::string& sSection, const std::string& sName, std::string& sValue)
	{
		std::lock_guard< std::mutex > l( m_oLock );
		Load();
		auto i = m_oSections.find( sSection );
		if ( i == m_oSections.end() ) return false;
		auto j = i->second.find( sName );
		if ( j == i->second.end() ) return false;
		sValue = j->second;
		return true;
	}

	// Remove one value, or (sName empty) a section and every section under it.
	bool Remove(const std::string& sSection, const std::string& sName)
	{
		std::lock_guard< std::mutex > l( m_oLock );
		Load();
		bool bChanged = false;
		if ( sName.empty() )
		{
			const std::string sPrefix = sSection + "\\";
			for ( auto i = m_oSections.begin(); i != m_oSections.end(); )
			{
				if ( strcasecmp( i->first.c_str(), sSection.c_str() ) == 0 ||
					strncasecmp( i->first.c_str(), sPrefix.c_str(), sPrefix.size() ) == 0 )
				{
					i = m_oSections.erase( i );
					bChanged = true;
				}
				else
					++i;
			}
		}
		else
		{
			auto i = m_oSections.find( sSection );
			if ( i != m_oSections.end() ) bChanged = i->second.erase( sName ) != 0;
		}
		return bChanged ? Save() : true;
	}

	bool Set(const std::string& sSection, const std::string& sName, const std::string& sValue)
	{
		std::lock_guard< std::mutex > l( m_oLock );
		Load();
		std::string& v = m_oSections[ sSection ][ sName ];
		if ( v == sValue && ! sValue.empty() ) return true;
		v = sValue;
		return Save();
	}

private:
	CStore() : m_bLoaded( false ) {}

	std::string Path() const
	{
		if ( const char* p = getenv( "SHAREAZA_CONFIG" ) ) return p;
		std::string sDir;
		if ( const char* x = getenv( "XDG_CONFIG_HOME" ) ) sDir = x;
		else sDir = std::string( getenv( "HOME" ) ? getenv( "HOME" ) : "." ) + "/.config";
		return sDir + "/shareaza/Shareaza.ini";
	}

	// Values are stored escaped: \\ \n \r, so any string round-trips on one line.
	static std::string Escape(const std::string& s)
	{
		std::string r;
		for ( char c : s )
		{
			if ( c == '\\' ) r += "\\\\";
			else if ( c == '\n' ) r += "\\n";
			else if ( c == '\r' ) r += "\\r";
			else r += c;
		}
		return r;
	}
	static std::string Unescape(const std::string& s)
	{
		std::string r;
		for ( size_t i = 0; i < s.size(); ++i )
		{
			if ( s[ i ] == '\\' && i + 1 < s.size() )
			{
				const char c = s[ ++i ];
				r += ( c == 'n' ) ? '\n' : ( c == 'r' ) ? '\r' : c;
			}
			else r += s[ i ];
		}
		return r;
	}

	void Load()
	{
		if ( m_bLoaded ) return;
		m_bLoaded = true;
		FILE* f = fopen( Path().c_str(), "r" );
		if ( ! f ) return;
		std::string sSection;
		char* pLine = NULL;
		size_t nCap = 0;
		ssize_t n;
		while ( ( n = getline( &pLine, &nCap, f ) ) >= 0 )
		{
			std::string s( pLine, (size_t)n );
			while ( ! s.empty() && ( s.back() == '\n' || s.back() == '\r' ) ) s.pop_back();
			if ( s.empty() || s[ 0 ] == ';' || s[ 0 ] == '#' ) continue;
			if ( s[ 0 ] == '[' && s.back() == ']' )
			{
				sSection = s.substr( 1, s.size() - 2 );
				continue;
			}
			const size_t nEq = s.find( '=' );
			if ( nEq == std::string::npos ) continue;
			m_oSections[ sSection ][ Unescape( s.substr( 0, nEq ) ) ] = Unescape( s.substr( nEq + 1 ) );
		}
		free( pLine );
		fclose( f );
	}

	bool Save()
	{
		const std::string sPath = Path();
		const size_t nSlash = sPath.rfind( '/' );
		if ( nSlash != std::string::npos )
		{
			// mkdir -p of the parent
			for ( size_t i = 1; i <= nSlash; ++i )
				if ( i == nSlash || sPath[ i ] == '/' )
					mkdir( sPath.substr( 0, i ).c_str(), 0700 );
		}
		const std::string sTemp = sPath + ".tmp";
		FILE* f = fopen( sTemp.c_str(), "w" );
		if ( ! f ) return false;
		fputs( "; Shareaza engine settings (the Windows build keeps these in the registry)\n", f );
		for ( const auto& sec : m_oSections )
		{
			fprintf( f, "\n[%s]\n", sec.first.c_str() );
			for ( const auto& v : sec.second )
				fprintf( f, "%s=%s\n", Escape( v.first ).c_str(), Escape( v.second ).c_str() );
		}
		const bool bOk = fflush( f ) == 0 && fsync( fileno( f ) ) == 0;
		fclose( f );
		return bOk && rename( sTemp.c_str(), sPath.c_str() ) == 0;
	}

	std::mutex m_oLock;
	bool m_bLoaded;
	Sections m_oSections;
};

std::string Utf8(LPCTSTR p)
{
	return p ? std::string( (LPCSTR)CStringA( CW2A( p, CP_UTF8 ) ) ) : std::string();
}

// Registry path: section, with an optional subkey in front, as Registry.cpp builds it.
std::string SectionOf(LPCTSTR pszSection, LPCTSTR pszSubKey)
{
	std::string s = Utf8( pszSubKey );
	const std::string sSection = Utf8( pszSection );
	if ( ! sSection.empty() )
		s = s.empty() ? sSection : s + "\\" + sSection;
	return s;
}

} // namespace

CString CRegistry::GetString(LPCTSTR pszSection, LPCTSTR pszName, LPCTSTR pszDefault, LPCTSTR pszSubKey, BOOL /*bIgnoreHKCU*/)
{
	std::string v;
	if ( CStore::Instance().Get( SectionOf( pszSection, pszSubKey ), Utf8( pszName ), v ) )
		return CString( CA2W( v.c_str(), CP_UTF8 ) );
	return pszDefault ? CString( pszDefault ) : CString();
}

DWORD CRegistry::GetDword(LPCTSTR pszSection, LPCTSTR pszName, DWORD dwDefault, LPCTSTR pszSubKey)
{
	std::string v;
	if ( ! CStore::Instance().Get( SectionOf( pszSection, pszSubKey ), Utf8( pszName ), v ) || v.empty() )
		return dwDefault;
	char* pEnd = NULL;
	const unsigned long long n = strtoull( v.c_str(), &pEnd, 0 );
	return ( pEnd && *pEnd == 0 ) ? (DWORD)n : dwDefault;
}

BOOL CRegistry::SetString(LPCTSTR pszSection, LPCTSTR pszName, LPCTSTR pszValue, LPCTSTR pszSubKey)
{
	return CStore::Instance().Set( SectionOf( pszSection, pszSubKey ), Utf8( pszName ), Utf8( pszValue ) ) ? TRUE : FALSE;
}

BOOL CRegistry::SetDword(LPCTSTR pszSection, LPCTSTR pszName, DWORD nValue, LPCTSTR pszSubKey)
{
	return CStore::Instance().Set( SectionOf( pszSection, pszSubKey ), Utf8( pszName ), std::to_string( nValue ) ) ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// The two registry deletions Settings.cpp makes when upgrading old settings.
// Paths under REGISTRY_KEY map to INI sections; anything else is not ours.

namespace {
	bool ToSection(LPCTSTR pszKey, std::string& sSection)
	{
		CString strKey( pszKey );
		const CString strRoot( REGISTRY_KEY );
		if ( strKey.Left( strRoot.GetLength() ).CompareNoCase( strRoot ) != 0 )
			return false;
		strKey = strKey.Mid( strRoot.GetLength() );
		strKey.TrimLeft( L'\\' );
		sSection = Utf8( strKey );
		return true;
	}
}

LONG RegDeleteKey(HKEY hKey, LPCTSTR pszSubKey)
{
	std::string sSection;
	if ( hKey != HKEY_CURRENT_USER || ! ToSection( pszSubKey, sSection ) )
		return ERROR_ACCESS_DENIED;
	return CStore::Instance().Remove( sSection, std::string() ) ? ERROR_SUCCESS : ERROR_ACCESS_DENIED;
}

LONG SHDeleteValue(HKEY hKey, LPCTSTR pszSubKey, LPCTSTR pszValue)
{
	std::string sSection;
	if ( hKey != HKEY_CURRENT_USER || ! ToSection( pszSubKey, sSection ) || ! pszValue )
		return ERROR_ACCESS_DENIED;
	return CStore::Instance().Remove( sSection, Utf8( pszValue ) ) ? ERROR_SUCCESS : ERROR_ACCESS_DENIED;
}
