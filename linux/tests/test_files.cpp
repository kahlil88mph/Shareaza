// Win32 file handles and file operations over POSIX, as the engine's
// download and library code uses them.

#include <windows.h>
#include <compat/afxcore.h>
#include <compat/afxfile.h>

#include <cstdio>
#include <string>

static int g_nFailed = 0, g_nPassed = 0;
#define CHECK(x) do { if ( x ) ++g_nPassed; else { ++g_nFailed; printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); } } while ( 0 )

static DWORD CALLBACK Progress(LARGE_INTEGER nTotal, LARGE_INTEGER nDone, LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD, HANDLE, HANDLE, LPVOID pData)
{
	LONGLONG* p = static_cast< LONGLONG* >( pData );
	p[ 0 ] = nTotal.QuadPart;
	p[ 1 ] = nDone.QuadPart;
	return p[ 2 ] && nDone.QuadPart >= p[ 2 ] ? PROGRESS_CANCEL : PROGRESS_CONTINUE;
}

int main()
{
	char szDir[] = "/tmp/shareaza-files-XXXXXX";
	if ( ! mkdtemp( szDir ) ) return 2;
	const CString strDir = compat::ToEnginePath( szDir );
	const CString strA = strDir + L"\\a.bin", strB = strDir + L"\\sub\\b.bin";

	// Create, write, seek, read, size, truncate
	HANDLE h = CreateFile( strA, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL );
	CHECK( h != INVALID_HANDLE_VALUE );
	DWORD n = 0;
	CHECK( WriteFile( h, "0123456789", 10, &n, NULL ) && n == 10 );
	CHECK( SetFilePointer( h, 3, NULL, FILE_BEGIN ) == 3 );
	char buf[ 16 ] = {};
	CHECK( ReadFile( h, buf, 4, &n, NULL ) && n == 4 && memcmp( buf, "3456", 4 ) == 0 );
	CHECK( ReadFile( h, buf, 16, &n, NULL ) && n == 3 );	// short read at end of file is success
	CHECK( ReadFile( h, buf, 16, &n, NULL ) && n == 0 );
	LONG nHigh = 1;	// a 64-bit seek: 4 GiB + 5 (sparse)
	CHECK( SetFilePointer( h, 5, &nHigh, FILE_BEGIN ) == 5 && nHigh == 1 && GetLastError() == ERROR_SUCCESS );
	CHECK( SetEndOfFile( h ) );
	DWORD nSizeHigh = 0;
	CHECK( GetFileSize( h, &nSizeHigh ) == 5 && nSizeHigh == 1 );
	OVERLAPPED ov = {};
	ov.Offset = 1;	// positioned read ignores the file pointer
	CHECK( ReadFile( h, buf, 2, &n, &ov ) && n == 2 && memcmp( buf, "12", 2 ) == 0 );
	CHECK( SetFilePointer( h, 10, NULL, FILE_BEGIN ) == 10 && SetEndOfFile( h ) && GetFileSize( h, NULL ) == 10 );
	DWORD nReturned;
	CHECK( DeviceIoControl( h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &nReturned, NULL ) );
	FILETIME ftWrite = {}, ftOld = {};
	CHECK( GetFileTime( h, NULL, NULL, &ftOld ) );
	SYSTEMTIME st = { 2001, 2, 0, 3, 4, 5, 6, 0 };
	SystemTimeToFileTime( &st, &ftWrite );
	CHECK( SetFileTime( h, NULL, NULL, &ftWrite ) );
	FILETIME ftBack = {};
	CHECK( GetFileTime( h, NULL, NULL, &ftBack ) && CompareFileTime( &ftBack, &ftWrite ) == 0 && CompareFileTime( &ftOld, &ftBack ) > 0 );
	CHECK( CloseHandle( h ) );
	CHECK( ! ReadFile( h, buf, 1, &n, NULL ) && GetLastError() == ERROR_INVALID_HANDLE );	// closed

	// Dispositions and errors
	CHECK( CreateFile( strA, GENERIC_READ, 0, NULL, CREATE_NEW, 0, NULL ) == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS );
	CHECK( CreateFile( strDir + L"\\none", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL ) == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND );
	h = CreateFile( strA, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL );
	CHECK( h != INVALID_HANDLE_VALUE && GetLastError() == ERROR_ALREADY_EXISTS );
	CloseHandle( h );
	CHECK( CreateFile( strDir, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL ) == INVALID_HANDLE_VALUE );	// folders need backup semantics
	h = CreateFile( strDir, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
	CHECK( h != INVALID_HANDLE_VALUE );
	CloseHandle( h );

	// Moves: no overwrite without the flag; progress and cancel on copies
	CHECK( CreateDirectory( strDir + L"\\sub", NULL ) );
	CHECK( CopyFile( strA, strB, TRUE ) );
	CHECK( ! MoveFile( strA, strB ) && GetLastError() == ERROR_ALREADY_EXISTS );
	CHECK( MoveFileEx( strA, strB, MOVEFILE_REPLACE_EXISTING ) && ! PathFileExists( strA ) && PathFileExists( strB ) );
	LONGLONG pProgress[ 3 ] = { 0, 0, 0 };
	CHECK( CopyFileEx( strB, strA, Progress, pProgress, NULL, COPY_FILE_FAIL_IF_EXISTS ) && pProgress[ 0 ] == 10 && pProgress[ 1 ] == 10 );
	CHECK( ! CopyFileEx( strB, strA, Progress, pProgress, NULL, COPY_FILE_FAIL_IF_EXISTS ) );
	DeleteFile( strA );
	pProgress[ 2 ] = 1;	// cancel after the first chunk: no partial file is left
	CHECK( ! CopyFileEx( strB, strA, Progress, pProgress, NULL, 0 ) && GetLastError() == ERROR_REQUEST_ABORTED && ! PathFileExists( strA ) );
	BOOL bCancel = TRUE;
	CHECK( ! CopyFileEx( strB, strA, NULL, NULL, &bCancel, 0 ) && ! PathFileExists( strA ) );

	// Directory listing with Windows wildcards (case-insensitive)
	CopyFile( strB, strDir + L"\\Movie.AVI", FALSE );
	CopyFile( strB, strDir + L"\\song.mp3", FALSE );
	WIN32_FIND_DATA fd;
	int nAll = 0, nAvi = 0, nDirs = 0;
	HANDLE hFind = FindFirstFile( strDir + L"\\*.*", &fd );
	CHECK( hFind != INVALID_HANDLE_VALUE );
	if ( hFind != INVALID_HANDLE_VALUE )
	{
		do
		{
			++nAll;
			if ( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) ++nDirs;
			else CHECK( fd.nFileSizeLow == 10 );
		}
		while ( FindNextFile( hFind, &fd ) );
		CHECK( GetLastError() == ERROR_NO_MORE_FILES );
		FindClose( hFind );
	}
	CHECK( nAll == 5 && nDirs == 3 );	// ".", "..", "sub", and the two files
	hFind = FindFirstFile( strDir + L"\\*.avi", &fd );
	if ( hFind != INVALID_HANDLE_VALUE ) { do ++nAvi; while ( FindNextFile( hFind, &fd ) ); FindClose( hFind ); }
	CHECK( nAvi == 1 && CString( fd.cFileName ) == L"Movie.AVI" );
	CHECK( FindFirstFile( strDir + L"\\*.none", &fd ) == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND );
	CHECK( FindFirstFile( strDir + L"\\missing\\*", &fd ) == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PATH_NOT_FOUND );
	CHECK( CString( PathFindExtension( L"\\a.b\\name.tar.gz" ) ) == L".gz" && *PathFindExtension( L"\\a.b\\name" ) == 0 );
	CHECK( SetFileAttributes( strDir + L"\\song.mp3", FILE_ATTRIBUTE_READONLY ) && ( GetFileAttributes( strDir + L"\\song.mp3" ) & FILE_ATTRIBUTE_READONLY ) );
	ULARGE_INTEGER nFree, nTotal;
	CHECK( GetDiskFreeSpaceEx( strDir, &nFree, &nTotal, NULL ) && nTotal.QuadPart >= nFree.QuadPart && nTotal.QuadPart > 0 );

	// Change notifications (inotify): a new file, and one in a new subfolder
	HANDLE hChange = FindFirstChangeNotification( strDir, TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME );
	CHECK( hChange != INVALID_HANDLE_VALUE );
	CHECK( WaitForSingleObject( hChange, 100 ) == WAIT_TIMEOUT );
	CopyFile( strB, strDir + L"\\new.bin", FALSE );
	CHECK( WaitForSingleObject( hChange, 2000 ) == WAIT_OBJECT_0 );
	CHECK( WaitForSingleObject( hChange, 0 ) == WAIT_OBJECT_0 );	// stays signalled until re-armed
	Sleep( 300 );	// let the watcher drain the burst of events from the copy
	CHECK( FindNextChangeNotification( hChange ) && WaitForSingleObject( hChange, 100 ) == WAIT_TIMEOUT );
	CreateDirectory( strDir + L"\\deep", NULL );
	Sleep( 300 );	// the new folder gets its own watch
	FindNextChangeNotification( hChange );
	CopyFile( strB, strDir + L"\\deep\\x.bin", FALSE );
	CHECK( WaitForSingleObject( hChange, 2000 ) == WAIT_OBJECT_0 );
	CHECK( FindCloseChangeNotification( hChange ) );
	CHECK( FindFirstChangeNotification( strDir + L"\\missing", FALSE, FILE_NOTIFY_CHANGE_FILE_NAME ) == INVALID_HANDLE_VALUE );

	// Full paths
	wchar_t sz[ 512 ];
	CHECK( GetFullPathName( L"\\a\\.\\b\\..\\c", 512, sz, NULL ) == 4 && CString( sz ) == L"\\a\\c" );
	CHECK( CString( PathFindFileName( L"\\x\\y\\file.txt" ) ) == L"file.txt" );

	system( ( std::string( "rm -rf '" ) + szDir + "'" ).c_str() );
	printf( "files: %d passed, %d failed\n", g_nPassed, g_nFailed );
	return g_nFailed ? 1 : 0;
}
