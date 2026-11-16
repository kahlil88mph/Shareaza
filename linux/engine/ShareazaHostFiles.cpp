//
// ShareazaHostFiles.cpp (Linux engine host)
//
// The file helpers Shareaza.cpp provides on Windows: OnRename, DeleteFileEx
// with the trash, and retrying delayed deletes. Separate from
// ShareazaHost.cpp because they call into transfers and the library.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include "StdAfx.h"
#include "Shareaza.h"
#include "Downloads.h"
#include "LibraryBuilder.h"
#include "Uploads.h"

#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

void CRegistry_EnumValues(LPCTSTR pszSection, CList< std::pair< CString, CString > >& oValues);

void CShareazaApp::OnRename(LPCTSTR pszSource, LPCTSTR pszTarget)
{
	LibraryBuilder.Remove( pszSource );
	Uploads.OnRename( pszSource, pszTarget );
	Downloads.OnRename( pszSource, pszTarget );
	// (Windows also tells its media player window about deleted files.)
}

namespace {
	std::string DataHome()
	{
		if ( const char* p = getenv( "XDG_DATA_HOME" ) ) if ( *p ) return p;
		const char* h = getenv( "HOME" );
		return std::string( h ? h : "." ) + "/.local/share";
	}

	std::string PercentEncode(const std::string& s)
	{
		static const char x[] = "0123456789ABCDEF";
		std::string r;
		for ( unsigned char c : s )
		{
			if ( isalnum( c ) || strchr( "/-_.~", c ) ) r += (char)c;
			else { r += '%'; r += x[ c >> 4 ]; r += x[ c & 15 ]; }
		}
		return r;
	}

	// The freedesktop.org Trash: the file goes to Trash/files/NAME with
	// Trash/info/NAME.trashinfo recording where it came from. Only the home
	// trash is used; a file on another filesystem is deleted instead.
	bool MoveToTrash(const std::string& sPath)
	{
		const std::string sTrash = DataHome() + "/Trash";
		mkdir( DataHome().c_str(), 0700 );
		mkdir( sTrash.c_str(), 0700 );
		mkdir( ( sTrash + "/files" ).c_str(), 0700 );
		mkdir( ( sTrash + "/info" ).c_str(), 0700 );
		std::string sName = sPath.substr( sPath.rfind( '/' ) + 1 );
		const std::string sBase = sName;
		for ( int i = 2; ; ++i )
		{
			// Claim the name by creating the .trashinfo exclusively.
			const std::string sInfo = sTrash + "/info/" + sName + ".trashinfo";
			const int fd = open( sInfo.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600 );
			if ( fd >= 0 )
			{
				char szDate[ 32 ];
				time_t t = time( NULL );
				struct tm v;
				strftime( szDate, sizeof( szDate ), "%Y-%m-%dT%H:%M:%S", localtime_r( &t, &v ) );
				const std::string sText = "[Trash Info]\nPath=" + PercentEncode( sPath ) + "\nDeletionDate=" + szDate + "\n";
				const bool bWritten = write( fd, sText.data(), sText.size() ) == (ssize_t)sText.size();
				close( fd );
				if ( bWritten && rename( sPath.c_str(), ( sTrash + "/files/" + sName ).c_str() ) == 0 )
					return true;
				unlink( sInfo.c_str() );
				return false;
			}
			if ( errno != EEXIST || i > 9999 ) return false;
			sName = sBase + "." + std::to_string( i );
		}
	}
}

BOOL DeleteFileEx(LPCTSTR szFileName, BOOL bShared, BOOL bToRecycleBin, BOOL bEnableDelayed)
{
	CString strPath( szFileName );
	if ( strPath.Left( 4 ) == L"\\\\?\\" ) strPath = strPath.Mid( 4 );

	if ( bShared )
		theApp.OnRename( strPath, NULL );	// stop uploads

	const DWORD dwAttr = GetFileAttributes( strPath );
	if ( dwAttr != INVALID_FILE_ATTRIBUTES && ( dwAttr & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
	{
		const std::string sPath = compat::ToNativePath( strPath );
		if ( ! bToRecycleBin || ! MoveToTrash( sPath ) )
			unlink( sPath.c_str() );

		if ( GetFileAttributes( strPath ) != INVALID_FILE_ATTRIBUTES )
		{
			// Still there: try again at the next start
			if ( bEnableDelayed )
			{
				CString sJob;
				sJob.Format( _T("%d%d"), bShared, bToRecycleBin );
				theApp.WriteProfileString( _T("Delete"), strPath, sJob );
			}
			return FALSE;
		}
	}

	// Cancel delayed deletion (if any)
	theApp.WriteProfileString( _T("Delete"), strPath, NULL );
	return TRUE;
}

void PurgeDeletes()
{
	CList< std::pair< CString, CString > > oJobs;
	CRegistry_EnumValues( _T("Delete"), oJobs );
	for ( POSITION pos = oJobs.GetHeadPosition(); pos; )
	{
		const std::pair< CString, CString >& oJob = oJobs.GetNext( pos );
		if ( oJob.second.IsEmpty() ) continue;	// already done
		const BOOL bShared = oJob.second.GetLength() > 0 && oJob.second[ 0 ] == L'1';
		const BOOL bToRecycleBin = oJob.second.GetLength() > 1 && oJob.second[ 1 ] == L'1';
		DeleteFileEx( oJob.first, bShared, bToRecycleBin, TRUE );
	}
}
