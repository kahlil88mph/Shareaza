//
// ShellIcons.cpp (Linux engine host): file types without the Windows shell.
//
// MIME types come from /etc/mime.types and the freedesktop.org shared MIME
// database (globs2), read once. Unknown types are application/x-binary and
// type names are the extension, as on Windows when the registry has nothing.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include "StdAfx.h"
#include "Shareaza.h"
#include "ShellIcons.h"

#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>

CShellIcons ShellIcons;

namespace {
	class CMimeTable
	{
	public:
		static CMimeTable& Instance() { static CMimeTable t; return t; }
		CString Lookup(const CString& strExt)	// ".mp3" -> "audio/mpeg", or empty
		{
			CString strKey( strExt );
			strKey.TrimLeft( L'.' );
			strKey.MakeLower();
			const std::string sKey( (LPCSTR)CStringA( CW2A( strKey, CP_UTF8 ) ) );
			auto i = m_oTypes.find( sKey );
			return i == m_oTypes.end() ? CString() : CString( CA2W( i->second.c_str(), CP_UTF8 ) );
		}
	private:
		CMimeTable()
		{
			// shared-mime-info first (weight:type:glob), then mime.types fills gaps
			std::ifstream g( "/usr/share/mime/globs2" );
			std::string line;
			while ( std::getline( g, line ) )
			{
				if ( line.empty() || line[ 0 ] == '#' ) continue;
				const size_t a = line.find( ':' ), b = line.find( ':', a + 1 );
				if ( a == std::string::npos || b == std::string::npos ) continue;
				const std::string sType = line.substr( a + 1, b - a - 1 );
				std::string sGlob = line.substr( b + 1 );
				const size_t c = sGlob.find( ':' );	// optional flags
				if ( c != std::string::npos ) sGlob.resize( c );
				if ( sGlob.size() < 3 || sGlob.compare( 0, 2, "*." ) != 0 ) continue;
				std::string sExt = sGlob.substr( 2 );
				if ( sExt.find_first_of( "*?[" ) != std::string::npos ) continue;
				for ( char& ch : sExt ) ch = (char)tolower( (unsigned char)ch );
				m_oTypes.emplace( sExt, sType );	// highest weight comes first
			}
			std::ifstream m( "/etc/mime.types" );
			while ( std::getline( m, line ) )
			{
				if ( line.empty() || line[ 0 ] == '#' ) continue;
				std::istringstream w( line );
				std::string sType, sExt;
				w >> sType;
				while ( w >> sExt ) m_oTypes.emplace( sExt, sType );
			}
		}
		std::unordered_map< std::string, std::string > m_oTypes;
	};
}

CString CShellIcons::GetMIME(LPCTSTR pszType)
{
	const CString strMIME = CMimeTable::Instance().Lookup( pszType ? pszType : L"" );
	return strMIME.IsEmpty() ? CString( L"application/x-binary" ) : strMIME;
}

CString CShellIcons::GetName(LPCTSTR pszType)
{
	if ( pszType && *pszType )
		return CString( pszType + ( *pszType == L'.' ? 1 : 0 ) );
	return LoadString( IDS_STATUS_UNKNOWN );
}

CString CShellIcons::GetTypeString(LPCTSTR pszFile)
{
	LPCTSTR pszType = PathFindExtension( pszFile );
	return GetName( pszType ) + _T(" (") + GetMIME( pszType ) + _T(")");
}
