//
// afxfile.cpp (Linux compatibility layer): CFile and CArchive.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include <compat/afxfile.h>
#include <compat/handles.h>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

const HANDLE CFile::hFileNull = INVALID_HANDLE_VALUE;

namespace compat {

std::string ToNativePath(LPCTSTR pszPath)
{
	CStringW s( pszPath );
	if ( s.Left( 4 ) == L"\\\\?\\" )
		s = s.Mid( 4 );
	s.Replace( L'\\', L'/' );
	return std::string( (LPCSTR)CStringA( CW2A( s, CP_UTF8 ) ) );
}

CString ToEnginePath(const char* pszPath)
{
	CString s( CA2W( pszPath ? pszPath : "", CP_UTF8 ) );
	s.Replace( L'/', L'\\' );
	return s;
}

} // namespace compat

BOOL PathFileExistsW(LPCWSTR pszPath)
{
	struct stat st;
	return pszPath && ::stat( compat::ToNativePath( pszPath ).c_str(), &st ) == 0;
}

BOOL PathIsDirectoryW(LPCWSTR pszPath)
{
	struct stat st;
	return pszPath && ::stat( compat::ToNativePath( pszPath ).c_str(), &st ) == 0 && S_ISDIR( st.st_mode );
}

BOOL CreateDirectoryW(LPCWSTR pszPath, LPSECURITY_ATTRIBUTES)
{
	if ( ! pszPath ) return FALSE;
	if ( ::mkdir( compat::ToNativePath( pszPath ).c_str(), 0755 ) == 0 ) return TRUE;
	SetLastError( errno == EEXIST ? ERROR_ALREADY_EXISTS : errno == ENOENT ? ERROR_PATH_NOT_FOUND : ERROR_ACCESS_DENIED );
	return FALSE;
}

BOOL RemoveDirectoryW(LPCWSTR pszPath) { return pszPath && ::rmdir( compat::ToNativePath( pszPath ).c_str() ) == 0; }
BOOL DeleteFileW(LPCWSTR pszPath) { return pszPath && ::unlink( compat::ToNativePath( pszPath ).c_str() ) == 0; }
// As on Windows: fails if the target exists, and moves files across filesystems.
BOOL MoveFileW(LPCWSTR a, LPCWSTR b)
{
	return MoveFileWithProgressW( a, b, NULL, NULL, MOVEFILE_COPY_ALLOWED );
}

BOOL CopyFileW(LPCWSTR a, LPCWSTR b, BOOL bFailIfExists)
{
	if ( ! a || ! b ) return FALSE;
	const int in = ::open( compat::ToNativePath( a ).c_str(), O_RDONLY | O_CLOEXEC );
	if ( in < 0 ) return FALSE;
	const int out = ::open( compat::ToNativePath( b ).c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | ( bFailIfExists ? O_EXCL : O_TRUNC ), 0644 );
	if ( out < 0 ) { ::close( in ); return FALSE; }
	char buf[ 65536 ];
	bool bOk = true;
	for ( ssize_t n; ( n = ::read( in, buf, sizeof( buf ) ) ) != 0; )
	{
		if ( n < 0 ) { if ( errno == EINTR ) continue; bOk = false; break; }
		for ( ssize_t w = 0; w < n; )
		{
			const ssize_t r = ::write( out, buf + w, (size_t)( n - w ) );
			if ( r < 0 ) { if ( errno == EINTR ) continue; bOk = false; break; }
			w += r;
		}
		if ( ! bOk ) break;
	}
	::close( in );
	return ( ::close( out ) == 0 && bOk ) ? TRUE : FALSE;
}

DWORD GetFullPathNameW(LPCWSTR pszFileName, DWORD nBufferLength, LPWSTR pszBuffer, LPWSTR* ppFilePart)
{
	if ( ! pszFileName ) return 0;
	CString strPath( pszFileName );
	strPath.Replace( L'/', L'\\' );
	if ( strPath.IsEmpty() ) return 0;
	if ( strPath[ 0 ] != L'\\' )
	{
		char szCwd[ 4096 ];
		if ( ! getcwd( szCwd, sizeof( szCwd ) ) ) return 0;
		strPath = compat::ToEnginePath( szCwd ) + L"\\" + strPath;
	}
	// Normalise "." and ".." components.
	std::vector< CString > oParts;
	int nPos = 0;
	for ( CString strPart = strPath.Tokenize( L"\\", nPos ); nPos != -1; strPart = strPath.Tokenize( L"\\", nPos ) )
	{
		if ( strPart == L"." ) continue;
		if ( strPart == L".." ) { if ( ! oParts.empty() ) oParts.pop_back(); continue; }
		oParts.push_back( strPart );
	}
	CString strFull;
	for ( const CString& p : oParts ) strFull += L"\\" + p;
	if ( strFull.IsEmpty() ) strFull = L"\\";
	if ( (DWORD)strFull.GetLength() + 1 > nBufferLength )
		return (DWORD)strFull.GetLength() + 1;
	wcscpy( pszBuffer, strFull );
	if ( ppFilePart ) *ppFilePart = wcsrchr( pszBuffer, L'\\' ) + 1;
	return (DWORD)strFull.GetLength();
}

LPWSTR PathFindFileNameW(LPCWSTR pszPath)
{
	if ( ! pszPath ) return NULL;
	LPCWSTR p = pszPath;
	for ( LPCWSTR q = pszPath; *q; ++q )
		if ( ( *q == L'\\' || *q == L'/' ) && q[ 1 ] ) p = q + 1;
	return const_cast< LPWSTR >( p );
}

namespace {
	int CauseFromErrno(int e)
	{
		switch ( e )
		{
		case ENOENT:  return CFileException::fileNotFound;
		case ENOTDIR: return CFileException::badPath;
		case EMFILE: case ENFILE: return CFileException::tooManyOpenFiles;
		case EACCES: case EPERM: case EROFS: return CFileException::accessDenied;
		case ENOSPC:  return CFileException::diskFull;
		case EIO:     return CFileException::hardIO;
		default:      return CFileException::genericException;
		}
	}
}

BOOL CFile::Open(LPCTSTR pszFileName, UINT nOpenFlags, CFileException* pError)
{
	if ( m_fd >= 0 )
		Close();
	int flags = O_CLOEXEC;
	switch ( nOpenFlags & 3 )
	{
	case modeRead: flags |= O_RDONLY; break;
	case modeWrite: flags |= O_WRONLY; break;
	default: flags |= O_RDWR; break;
	}
	if ( nOpenFlags & modeCreate )
	{
		flags |= O_CREAT;
		if ( ! ( nOpenFlags & modeNoTruncate ) )
			flags |= O_TRUNC;
	}
	m_strFileName = pszFileName;
	m_fd = ::open( compat::ToNativePath( pszFileName ).c_str(), flags, 0644 );
	if ( m_fd < 0 )
	{
		const int e = errno;
		SetLastError( e == ENOENT ? ERROR_FILE_NOT_FOUND : e == EACCES ? ERROR_ACCESS_DENIED : (DWORD)e );
		if ( pError )
		{
			pError->m_cause = CauseFromErrno( e );
			pError->m_lOsError = e;
			pError->m_strFileName = pszFileName;
		}
		return FALSE;
	}
	m_hFile = (HANDLE)(INT_PTR)m_fd;
	return TRUE;
}

UINT CFile::Read(void* lpBuf, UINT nCount)
{
	BYTE* p = static_cast< BYTE* >( lpBuf );
	UINT nDone = 0;
	while ( nDone < nCount )
	{
		const ssize_t n = ::read( m_fd, p + nDone, nCount - nDone );
		if ( n < 0 )
		{
			if ( errno == EINTR ) continue;
			AfxThrowFileException( CauseFromErrno( errno ), errno, m_strFileName );
		}
		if ( n == 0 ) break;
		nDone += (UINT)n;
	}
	return nDone;
}

void CFile::Write(const void* lpBuf, UINT nCount)
{
	const BYTE* p = static_cast< const BYTE* >( lpBuf );
	while ( nCount )
	{
		const ssize_t n = ::write( m_fd, p, nCount );
		if ( n < 0 )
		{
			if ( errno == EINTR ) continue;
			AfxThrowFileException( CauseFromErrno( errno ), errno, m_strFileName );
		}
		p += n;
		nCount -= (UINT)n;
	}
}

ULONGLONG CFile::Seek(LONGLONG lOff, UINT nFrom)
{
	const off_t n = ::lseek( m_fd, (off_t)lOff, nFrom == begin ? SEEK_SET : nFrom == current ? SEEK_CUR : SEEK_END );
	if ( n < 0 )
		AfxThrowFileException( CFileException::badSeek, errno, m_strFileName );
	return (ULONGLONG)n;
}

ULONGLONG CFile::GetPosition() const
{
	return (ULONGLONG)::lseek( m_fd, 0, SEEK_CUR );
}

ULONGLONG CFile::GetLength() const
{
	struct stat st;
	return ::fstat( m_fd, &st ) == 0 ? (ULONGLONG)st.st_size : 0;
}

void CFile::SetLength(ULONGLONG n)
{
	if ( ::ftruncate( m_fd, (off_t)n ) != 0 )
		AfxThrowFileException( CauseFromErrno( errno ), errno, m_strFileName );
}

void CFile::Flush()
{
	if ( m_fd >= 0 ) ::fsync( m_fd );
}

void CFile::Close()
{
	if ( m_fd >= 0 )
	{
		::close( m_fd );
		m_fd = -1;
	}
	m_hFile = hFileNull;
}

void CFile::Abort()
{
	Close();
}

CString CFile::GetFileName() const
{
	const int n = std::max( m_strFileName.ReverseFind( L'\\' ), m_strFileName.ReverseFind( L'/' ) );
	return m_strFileName.Mid( n + 1 );
}

// ---------------------------------------------------------------------------
// CArchive

CArchive::CArchive(CFile* pFile, UINT nMode, int nBufSize, void*)
	: m_nObjectSchema( -1 )
	, m_pFile( pFile )
	, m_bLoading( ( nMode & load ) != 0 )
	, m_buf( (size_t)( nBufSize < 512 ? 512 : nBufSize ) )
	, m_nPos( 0 )
	, m_nLen( 0 )
{
	if ( pFile ) m_strFileName = pFile->GetFilePath();
}

CArchive::~CArchive()
{
	if ( m_pFile && ! m_bLoading )
	{
		try { Flush(); } catch ( CException* e ) { e->Delete(); }
	}
}

void CArchive::Flush()
{
	if ( ! m_bLoading && m_nPos )
	{
		m_pFile->Write( m_buf.data(), (UINT)m_nPos );
		m_nPos = 0;
	}
}

void CArchive::Close()
{
	Flush();
	m_pFile = NULL;
}

void CArchive::Abort()
{
	m_nPos = m_nLen = 0;
	m_pFile = NULL;
}

UINT CArchive::Read(void* lpBuf, UINT nMax)
{
	BYTE* p = static_cast< BYTE* >( lpBuf );
	UINT nDone = 0;
	while ( nDone < nMax )
	{
		if ( m_nPos == m_nLen )
		{
			m_nPos = 0;
			m_nLen = m_pFile->Read( m_buf.data(), (UINT)m_buf.size() );
			if ( ! m_nLen ) break;
		}
		const size_t n = std::min( (size_t)( nMax - nDone ), m_nLen - m_nPos );
		memcpy( p + nDone, m_buf.data() + m_nPos, n );
		m_nPos += n;
		nDone += (UINT)n;
	}
	return nDone;
}

void CArchive::Fill(void* p, UINT n)
{
	if ( Read( p, n ) != n )
		AfxThrowArchiveException( CArchiveException::endOfFile, m_strFileName );
}

void CArchive::Write(const void* lpBuf, UINT nMax)
{
	const BYTE* p = static_cast< const BYTE* >( lpBuf );
	while ( nMax )
	{
		if ( m_nPos == m_buf.size() ) Flush();
		const size_t n = std::min( (size_t)nMax, m_buf.size() - m_nPos );
		memcpy( m_buf.data() + m_nPos, p, n );
		m_nPos += n;
		p += n;
		nMax -= (UINT)n;
	}
}

// MFC's count encoding (CArchive::WriteCount/ReadCount): WORD, or 0xFFFF then DWORD.
void CArchive::WriteCount(DWORD_PTR n)
{
	if ( n < 0xFFFF )
		*this << (WORD)n;
	else
	{
		*this << (WORD)0xFFFF;
		if ( n < 0xFFFFFFFF )
			*this << (DWORD)n;
		else
		{
			*this << (DWORD)0xFFFFFFFF;
			*this << (ULONGLONG)n;
		}
	}
}

DWORD_PTR CArchive::ReadCount()
{
	WORD w; *this >> w;
	if ( w != 0xFFFF ) return w;
	DWORD d; *this >> d;
	if ( d != 0xFFFFFFFF ) return d;
	ULONGLONG q; *this >> q;
	return (DWORD_PTR)q;
}

namespace {
	// MFC string length prefix (AfxWriteStringLength / AfxReadStringLength).
	void WriteStringLength(CArchive& ar, size_t n, bool bUnicode)
	{
		if ( bUnicode )
		{
			ar << (BYTE)0xFF;
			ar << (WORD)0xFFFE;
		}
		if ( n < 0xFF )
			ar << (BYTE)n;
		else
		{
			ar << (BYTE)0xFF;
			if ( n < 0xFFFE )
				ar << (WORD)n;
			else
			{
				ar << (WORD)0xFFFF;
				if ( n < 0xFFFFFFFF )
					ar << (DWORD)n;
				else
				{
					ar << (DWORD)0xFFFFFFFF;
					ar << (ULONGLONG)n;
				}
			}
		}
	}

	size_t ReadStringLength(CArchive& ar, int& nCharSize)
	{
		nCharSize = 1;
		BYTE b; ar >> b;
		if ( b < 0xFF ) return b;
		WORD w; ar >> w;
		if ( w == 0xFFFE )
		{
			nCharSize = 2;
			ar >> b;
			if ( b < 0xFF ) return b;
			ar >> w;
		}
		if ( w < 0xFFFF ) return w;
		DWORD d; ar >> d;
		if ( d < 0xFFFFFFFF ) return d;
		ULONGLONG q; ar >> q;
		return (size_t)q;
	}

	// UTF-32 <-> UTF-16 for the on-disk format.
	std::u16string ToUtf16(const wchar_t* p, size_t n)
	{
		std::u16string s;
		s.reserve( n );
		for ( size_t i = 0; i < n; ++i )
		{
			uint32_t c = (uint32_t)p[ i ];
			if ( c >= 0x10000 && c <= 0x10FFFF )
			{
				c -= 0x10000;
				s += (char16_t)( 0xD800 + ( c >> 10 ) );
				s += (char16_t)( 0xDC00 + ( c & 0x3FF ) );
			}
			else
				s += (char16_t)( c > 0xFFFF ? 0xFFFD : c );
		}
		return s;
	}

	CStringW FromUtf16(const char16_t* p, size_t n)
	{
		CStringW s;
		s.Preallocate( (int)n );
		for ( size_t i = 0; i < n; ++i )
		{
			uint32_t c = p[ i ];
			if ( c >= 0xD800 && c < 0xDC00 && i + 1 < n && p[ i + 1 ] >= 0xDC00 && p[ i + 1 ] < 0xE000 )
			{
				c = 0x10000 + ( ( c - 0xD800 ) << 10 ) + ( p[ i + 1 ] - 0xDC00 );
				++i;
			}
			s.AppendChar( (wchar_t)c );
		}
		return s;
	}
}

CArchive& CArchive::operator<<(const CStringW& s)
{
	const std::u16string u = ToUtf16( s.GetString(), (size_t)s.GetLength() );
	WriteStringLength( *this, u.size(), true );
	Write( u.data(), (UINT)( u.size() * 2 ) );
	return *this;
}

CArchive& CArchive::operator<<(const CStringA& s)
{
	WriteStringLength( *this, (size_t)s.GetLength(), false );
	Write( s.GetString(), (UINT)s.GetLength() );
	return *this;
}

CArchive& CArchive::operator>>(CStringW& s)
{
	int nCharSize;
	const size_t n = ReadStringLength( *this, nCharSize );
	if ( nCharSize == 2 )
	{
		std::u16string u( n, u'\0' );
		Fill( &u[ 0 ], (UINT)( n * 2 ) );
		s = FromUtf16( u.data(), n );
	}
	else
	{
		CStringA a;
		Fill( a.GetBufferSetLength( (int)n ), (UINT)n );
		s = CStringW( a );
	}
	return *this;
}

CArchive& CArchive::operator>>(CStringA& s)
{
	int nCharSize;
	const size_t n = ReadStringLength( *this, nCharSize );
	if ( nCharSize == 2 )
	{
		std::u16string u( n, u'\0' );
		Fill( &u[ 0 ], (UINT)( n * 2 ) );
		s = CStringA( FromUtf16( u.data(), n ) );
	}
	else
		Fill( s.GetBufferSetLength( (int)n ), (UINT)n );
	return *this;
}

void CArchive::WriteString(LPCTSTR lpsz)
{
	const CStringA a( CW2A( lpsz, CP_ACP ) );
	Write( a.GetString(), (UINT)a.GetLength() );
}

BOOL CArchive::ReadString(CString& rString)
{
	CStringA a;
	char c;
	bool bAny = false;
	while ( Read( &c, 1 ) == 1 )
	{
		bAny = true;
		if ( c == '\n' ) break;
		if ( c != '\r' ) a += c;
	}
	rString = CString( a );
	return bAny;
}

// ---------------------------------------------------------------------------
// GetFileAttributes(Ex)

namespace {
	void UnixToFileTime(const struct timespec& ts, FILETIME* ft)
	{
		const ULONGLONG n = 116444736000000000ull + (ULONGLONG)ts.tv_sec * 10000000u + (ULONGLONG)ts.tv_nsec / 100u;
		ft->dwLowDateTime = (DWORD)n;
		ft->dwHighDateTime = (DWORD)( n >> 32 );
	}

	bool StatPath(LPCWSTR pszFileName, struct stat& st, std::string& sPath)
	{
		sPath = compat::ToNativePath( pszFileName );
		if ( ::stat( sPath.c_str(), &st ) == 0 )
			return true;
		SetLastError( errno == ENOENT ? ERROR_FILE_NOT_FOUND : errno == ENOTDIR ? ERROR_PATH_NOT_FOUND : ERROR_ACCESS_DENIED );
		return false;
	}

	DWORD AttributesOf(const struct stat& st, const std::string& sPath)
	{
		DWORD n = S_ISDIR( st.st_mode ) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_ARCHIVE;
		if ( ::access( sPath.c_str(), W_OK ) != 0 ) n |= FILE_ATTRIBUTE_READONLY;
		const size_t nSlash = sPath.rfind( '/' );
		if ( sPath.size() > nSlash + 1 && sPath[ nSlash + 1 ] == '.' ) n |= FILE_ATTRIBUTE_HIDDEN;
		return n;
	}
}

BOOL GetFileAttributesEx(LPCWSTR pszFileName, GET_FILEEX_INFO_LEVELS, LPVOID pInfo)
{
	struct stat st;
	std::string sPath;
	if ( ! pszFileName || ! StatPath( pszFileName, st, sPath ) )
		return FALSE;
	WIN32_FILE_ATTRIBUTE_DATA* p = static_cast< WIN32_FILE_ATTRIBUTE_DATA* >( pInfo );
	p->dwFileAttributes = AttributesOf( st, sPath );
	UnixToFileTime( st.st_ctim, &p->ftCreationTime );
	UnixToFileTime( st.st_atim, &p->ftLastAccessTime );
	UnixToFileTime( st.st_mtim, &p->ftLastWriteTime );
	const ULONGLONG nSize = S_ISDIR( st.st_mode ) ? 0 : (ULONGLONG)st.st_size;
	p->nFileSizeHigh = (DWORD)( nSize >> 32 );
	p->nFileSizeLow = (DWORD)nSize;
	return TRUE;
}

DWORD GetFileAttributes(LPCWSTR pszFileName)
{
	struct stat st;
	std::string sPath;
	if ( ! pszFileName || ! StatPath( pszFileName, st, sPath ) )
		return INVALID_FILE_ATTRIBUTES;
	return AttributesOf( st, sPath );
}

// ---------------------------------------------------------------------------
// File HANDLEs

namespace compat {
FileObject::~FileObject() { if ( m_fd >= 0 ) ::close( m_fd ); }

int FileDescriptor(HANDLE h)
{
	FileObject* p = dynamic_cast< FileObject* >( FromHandle( h ) );
	return p ? p->m_fd : -1;
}
}

namespace {
	DWORD ErrorFromErrno(int e)
	{
		switch ( e )
		{
		case ENOENT: return ERROR_FILE_NOT_FOUND;
		case ENOTDIR: return ERROR_PATH_NOT_FOUND;
		case EACCES: case EPERM: case EROFS: case EISDIR: return ERROR_ACCESS_DENIED;
		case EEXIST: return ERROR_FILE_EXISTS;
		case EBADF: return ERROR_INVALID_HANDLE;
		case ENOMEM: return ERROR_NOT_ENOUGH_MEMORY;
		case EINVAL: return ERROR_INVALID_PARAMETER;
		case ENOSPC: return 112u;	// ERROR_DISK_FULL
		default: return (DWORD)e;
		}
	}
	bool Fail(int e) { SetLastError( ErrorFromErrno( e ) ); return false; }
}

HANDLE CreateFileW(LPCWSTR pszFileName, DWORD dwAccess, DWORD /*dwShareMode*/, LPSECURITY_ATTRIBUTES,
	DWORD dwDisposition, DWORD dwFlags, HANDLE)
{
	if ( ! pszFileName ) { SetLastError( ERROR_INVALID_PARAMETER ); return INVALID_HANDLE_VALUE; }
	const std::string sPath = compat::ToNativePath( pszFileName );
	int flags = O_CLOEXEC;
	const bool bRead = ( dwAccess & ( GENERIC_READ | GENERIC_ALL ) ) != 0;
	const bool bWrite = ( dwAccess & ( GENERIC_WRITE | GENERIC_ALL ) ) != 0;
	flags |= ( bRead && bWrite ) ? O_RDWR : bWrite ? O_WRONLY : O_RDONLY;
	switch ( dwDisposition )
	{
	case CREATE_NEW: flags |= O_CREAT | O_EXCL; break;
	case CREATE_ALWAYS: flags |= O_CREAT | O_TRUNC; break;
	case OPEN_ALWAYS: flags |= O_CREAT; break;
	case TRUNCATE_EXISTING: flags |= O_TRUNC; break;
	default: break;
	}
	struct stat st;
	const bool bExisted = ::stat( sPath.c_str(), &st ) == 0;
	// FILE_FLAG_BACKUP_SEMANTICS lets Windows open folders; O_RDONLY does that here.
	if ( bExisted && S_ISDIR( st.st_mode ) && ! ( dwFlags & FILE_FLAG_BACKUP_SEMANTICS ) )
	{
		SetLastError( ERROR_ACCESS_DENIED );
		return INVALID_HANDLE_VALUE;
	}
	const int fd = ::open( sPath.c_str(), flags, 0644 );
	if ( fd < 0 ) { Fail( errno ); return INVALID_HANDLE_VALUE; }
	if ( dwFlags & FILE_FLAG_DELETE_ON_CLOSE ) ::unlink( sPath.c_str() );
	// Windows reports ERROR_ALREADY_EXISTS on success for these two.
	SetLastError( ( bExisted && ( dwDisposition == CREATE_ALWAYS || dwDisposition == OPEN_ALWAYS ) ) ? ERROR_ALREADY_EXISTS : ERROR_SUCCESS );
	return new compat::FileObject( fd );
}

BOOL ReadFile(HANDLE h, LPVOID pBuffer, DWORD nToRead, LPDWORD pnRead, LPOVERLAPPED pOverlapped)
{
	const int fd = compat::FileDescriptor( h );
	if ( pnRead ) *pnRead = 0;
	if ( fd < 0 ) return Fail( EBADF );
	BYTE* p = static_cast< BYTE* >( pBuffer );
	off_t nOffset = pOverlapped ? (off_t)( ( (ULONGLONG)pOverlapped->OffsetHigh << 32 ) | pOverlapped->Offset ) : -1;
	DWORD nDone = 0;
	while ( nDone < nToRead )
	{
		const ssize_t n = nOffset >= 0 ? ::pread( fd, p + nDone, nToRead - nDone, nOffset + nDone ) : ::read( fd, p + nDone, nToRead - nDone );
		if ( n < 0 ) { if ( errno == EINTR ) continue; return Fail( errno ); }
		if ( n == 0 ) break;	// end of file: success with fewer bytes, as on Windows
		nDone += (DWORD)n;
	}
	if ( pnRead ) *pnRead = nDone;
	return TRUE;
}

BOOL WriteFile(HANDLE h, LPCVOID pBuffer, DWORD nToWrite, LPDWORD pnWritten, LPOVERLAPPED pOverlapped)
{
	const int fd = compat::FileDescriptor( h );
	if ( pnWritten ) *pnWritten = 0;
	if ( fd < 0 ) return Fail( EBADF );
	const BYTE* p = static_cast< const BYTE* >( pBuffer );
	off_t nOffset = pOverlapped ? (off_t)( ( (ULONGLONG)pOverlapped->OffsetHigh << 32 ) | pOverlapped->Offset ) : -1;
	DWORD nDone = 0;
	while ( nDone < nToWrite )
	{
		const ssize_t n = nOffset >= 0 ? ::pwrite( fd, p + nDone, nToWrite - nDone, nOffset + nDone ) : ::write( fd, p + nDone, nToWrite - nDone );
		if ( n < 0 ) { if ( errno == EINTR ) continue; if ( pnWritten ) *pnWritten = nDone; return Fail( errno ); }
		nDone += (DWORD)n;
	}
	if ( pnWritten ) *pnWritten = nDone;
	return TRUE;
}

BOOL SetFilePointerEx(HANDLE h, LARGE_INTEGER liDistance, PLARGE_INTEGER pliNew, DWORD dwMoveMethod)
{
	const int fd = compat::FileDescriptor( h );
	if ( fd < 0 ) return Fail( EBADF );
	const off_t n = ::lseek( fd, (off_t)liDistance.QuadPart, dwMoveMethod == FILE_BEGIN ? SEEK_SET : dwMoveMethod == FILE_CURRENT ? SEEK_CUR : SEEK_END );
	if ( n < 0 ) return Fail( errno );
	if ( pliNew ) pliNew->QuadPart = n;
	return TRUE;
}

DWORD SetFilePointer(HANDLE h, LONG lDistance, PLONG plDistanceHigh, DWORD dwMoveMethod)
{
	LARGE_INTEGER li, liNew;
	// Without a high part the distance is a signed 32-bit value.
	li.QuadPart = plDistanceHigh ? (LONGLONG)( ( (ULONGLONG)(DWORD)*plDistanceHigh << 32 ) | (DWORD)lDistance ) : (LONGLONG)lDistance;
	if ( ! SetFilePointerEx( h, li, &liNew, dwMoveMethod ) ) return INVALID_SET_FILE_POINTER;
	if ( plDistanceHigh ) *plDistanceHigh = liNew.HighPart;
	SetLastError( ERROR_SUCCESS );	// callers tell a real 0xFFFFFFFF offset from failure this way
	return liNew.LowPart;
}

BOOL GetFileSizeEx(HANDLE h, PLARGE_INTEGER pSize)
{
	struct stat st;
	const int fd = compat::FileDescriptor( h );
	if ( fd < 0 ) return Fail( EBADF );
	if ( ::fstat( fd, &st ) != 0 ) return Fail( errno );
	pSize->QuadPart = st.st_size;
	return TRUE;
}

DWORD GetFileSize(HANDLE h, LPDWORD pnSizeHigh)
{
	LARGE_INTEGER li;
	if ( ! GetFileSizeEx( h, &li ) ) return INVALID_FILE_SIZE;
	if ( pnSizeHigh ) *pnSizeHigh = (DWORD)li.HighPart;
	SetLastError( ERROR_SUCCESS );
	return li.LowPart;
}

BOOL SetEndOfFile(HANDLE h)
{
	const int fd = compat::FileDescriptor( h );
	if ( fd < 0 ) return Fail( EBADF );
	const off_t n = ::lseek( fd, 0, SEEK_CUR );
	if ( n < 0 || ::ftruncate( fd, n ) != 0 ) return Fail( errno );
	return TRUE;
}

BOOL FlushFileBuffers(HANDLE h)
{
	const int fd = compat::FileDescriptor( h );
	if ( fd < 0 ) return Fail( EBADF );
	return ::fsync( fd ) == 0 ? TRUE : Fail( errno );
}

namespace {
	struct timespec ToTimespec(const FILETIME* ft)
	{
		const ULONGLONG n = ( ( (ULONGLONG)ft->dwHighDateTime << 32 ) | ft->dwLowDateTime ) - 116444736000000000ull;
		struct timespec ts = { (time_t)( n / 10000000u ), (long)( n % 10000000u ) * 100 };
		return ts;
	}
}

BOOL GetFileTime(HANDLE h, LPFILETIME pCreation, LPFILETIME pLastAccess, LPFILETIME pLastWrite)
{
	struct stat st;
	const int fd = compat::FileDescriptor( h );
	if ( fd < 0 ) return Fail( EBADF );
	if ( ::fstat( fd, &st ) != 0 ) return Fail( errno );
	if ( pCreation ) UnixToFileTime( st.st_ctim, pCreation );
	if ( pLastAccess ) UnixToFileTime( st.st_atim, pLastAccess );
	if ( pLastWrite ) UnixToFileTime( st.st_mtim, pLastWrite );
	return TRUE;
}

BOOL SetFileTime(HANDLE h, const FILETIME*, const FILETIME* pLastAccess, const FILETIME* pLastWrite)
{
	const int fd = compat::FileDescriptor( h );
	if ( fd < 0 ) return Fail( EBADF );
	struct timespec ts[ 2 ];
	ts[ 0 ].tv_nsec = UTIME_OMIT;
	ts[ 1 ].tv_nsec = UTIME_OMIT;
	if ( pLastAccess ) ts[ 0 ] = ToTimespec( pLastAccess );
	if ( pLastWrite ) ts[ 1 ] = ToTimespec( pLastWrite );
	return ::futimens( fd, ts ) == 0 ? TRUE : Fail( errno );	// creation time cannot be set on Linux
}

BOOL DeviceIoControl(HANDLE h, DWORD dwCode, LPVOID, DWORD, LPVOID, DWORD, LPDWORD pnReturned, LPOVERLAPPED)
{
	if ( pnReturned ) *pnReturned = 0;
	if ( compat::FileDescriptor( h ) < 0 ) return Fail( EBADF );
	if ( dwCode == FSCTL_SET_SPARSE ) return TRUE;
	SetLastError( ERROR_INVALID_PARAMETER );
	return FALSE;
}

LONG CompareFileTime(const FILETIME* a, const FILETIME* b)
{
	const ULONGLONG x = ( (ULONGLONG)a->dwHighDateTime << 32 ) | a->dwLowDateTime;
	const ULONGLONG y = ( (ULONGLONG)b->dwHighDateTime << 32 ) | b->dwLowDateTime;
	return x < y ? -1 : x > y ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Moves and copies with progress

namespace {
	// Copy a to b, reporting progress after every chunk. Returns false with
	// the Windows error set; a cancelled copy removes the partial target.
	bool CopyWithProgress(const std::string& a, const std::string& b, bool bFailIfExists,
		LPPROGRESS_ROUTINE pfn, LPVOID pData, LPBOOL pbCancel)
	{
		const int in = ::open( a.c_str(), O_RDONLY | O_CLOEXEC );
		if ( in < 0 ) return Fail( errno );
		struct stat st;
		::fstat( in, &st );
		const int out = ::open( b.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | ( bFailIfExists ? O_EXCL : O_TRUNC ), st.st_mode & 0777 );
		if ( out < 0 ) { const int e = errno; ::close( in ); return Fail( e ); }
		LARGE_INTEGER nTotal, nDone;
		nTotal.QuadPart = st.st_size;
		nDone.QuadPart = 0;
		bool bQuiet = ! pfn, bOk = true;
		DWORD dwError = 0;
		if ( ! bQuiet )
		{
			const DWORD r = pfn( nTotal, nDone, nTotal, nDone, 1, CALLBACK_STREAM_SWITCH, NULL, NULL, pData );
			if ( r == PROGRESS_CANCEL || r == PROGRESS_STOP ) { bOk = false; dwError = ERROR_REQUEST_ABORTED; }
			else if ( r == PROGRESS_QUIET ) bQuiet = true;
		}
		std::vector< char > buf( 1 << 20 );
		while ( bOk )
		{
			if ( pbCancel && *pbCancel ) { bOk = false; dwError = ERROR_REQUEST_ABORTED; break; }
			const ssize_t n = ::read( in, buf.data(), buf.size() );
			if ( n < 0 ) { if ( errno == EINTR ) continue; bOk = false; dwError = ErrorFromErrno( errno ); break; }
			if ( n == 0 ) break;
			for ( ssize_t w = 0; w < n; )
			{
				const ssize_t r = ::write( out, buf.data() + w, (size_t)( n - w ) );
				if ( r < 0 ) { if ( errno == EINTR ) continue; bOk = false; dwError = ErrorFromErrno( errno ); break; }
				w += r;
			}
			if ( ! bOk ) break;
			nDone.QuadPart += n;
			if ( ! bQuiet )
			{
				const DWORD r = pfn( nTotal, nDone, nTotal, nDone, 1, CALLBACK_CHUNK_FINISHED, NULL, NULL, pData );
				if ( r == PROGRESS_CANCEL || r == PROGRESS_STOP ) { bOk = false; dwError = ERROR_REQUEST_ABORTED; }
				else if ( r == PROGRESS_QUIET ) bQuiet = true;
			}
		}
		::close( in );
		if ( ::close( out ) != 0 && bOk ) { bOk = false; dwError = ErrorFromErrno( errno ); }
		if ( ! bOk )
		{
			::unlink( b.c_str() );
			SetLastError( dwError );
			return false;
		}
		struct timespec ts[ 2 ] = { st.st_atim, st.st_mtim };	// keep the times, as Windows does
		::utimensat( AT_FDCWD, b.c_str(), ts, 0 );
		return true;
	}

	bool Exists(const std::string& s) { struct stat st; return ::lstat( s.c_str(), &st ) == 0; }
}

BOOL MoveFileWithProgressW(LPCWSTR pszFrom, LPCWSTR pszTo, LPPROGRESS_ROUTINE pfn, LPVOID pData, DWORD dwFlags)
{
	if ( ! pszFrom || ! pszTo ) { SetLastError( ERROR_INVALID_PARAMETER ); return FALSE; }
	const std::string a = compat::ToNativePath( pszFrom ), b = compat::ToNativePath( pszTo );
	if ( ! ( dwFlags & MOVEFILE_REPLACE_EXISTING ) && Exists( b ) )
	{
		SetLastError( ERROR_ALREADY_EXISTS );
		return FALSE;
	}
	if ( ::rename( a.c_str(), b.c_str() ) == 0 ) return TRUE;
	if ( errno != EXDEV || ! ( dwFlags & MOVEFILE_COPY_ALLOWED ) ) return Fail( errno );
	// Another filesystem: copy, then remove the original.
	if ( ! CopyWithProgress( a, b, false, pfn, pData, NULL ) ) return FALSE;
	if ( dwFlags & MOVEFILE_WRITE_THROUGH )
	{
		const int fd = ::open( b.c_str(), O_RDONLY | O_CLOEXEC );
		if ( fd >= 0 ) { ::fsync( fd ); ::close( fd ); }
	}
	if ( ::unlink( a.c_str() ) != 0 ) return Fail( errno );
	return TRUE;
}

BOOL MoveFileExW(LPCWSTR pszFrom, LPCWSTR pszTo, DWORD dwFlags)
{
	return MoveFileWithProgressW( pszFrom, pszTo, NULL, NULL, dwFlags );
}

BOOL CopyFileExW(LPCWSTR pszFrom, LPCWSTR pszTo, LPPROGRESS_ROUTINE pfn, LPVOID pData, LPBOOL pbCancel, DWORD dwCopyFlags)
{
	if ( ! pszFrom || ! pszTo ) { SetLastError( ERROR_INVALID_PARAMETER ); return FALSE; }
	return CopyWithProgress( compat::ToNativePath( pszFrom ), compat::ToNativePath( pszTo ),
		( dwCopyFlags & COPY_FILE_FAIL_IF_EXISTS ) != 0, pfn, pData, pbCancel ) ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// FindFirstFile / FindNextFile

#include <dirent.h>
#include <fnmatch.h>
#include <sys/statvfs.h>

namespace {
	class FindObject : public compat::KernelObject
	{
	public:
		FindObject() : m_pDir( NULL ) {}
		~FindObject() override { if ( m_pDir ) closedir( m_pDir ); }
		bool IsSignalled() const override { return true; }
		DIR* m_pDir;
		std::string m_sDir;		// with trailing '/'
		std::string m_sPattern;	// UTF-8
	};

	bool Matches(const std::string& sName, const std::string& sPattern)
	{
		if ( sPattern == "*" || sPattern == "*.*" ) return true;	// "*.*" matches names without a dot on Windows
		return fnmatch( sPattern.c_str(), sName.c_str(), FNM_CASEFOLD | FNM_NOESCAPE ) == 0;
	}

	bool Fill(FindObject* p, LPWIN32_FIND_DATAW pData)
	{
		for ( ;; )
		{
			struct dirent* e = readdir( p->m_pDir );
			if ( ! e ) { SetLastError( ERROR_NO_MORE_FILES ); return false; }
			const std::string sName( e->d_name );
			if ( ! Matches( sName, p->m_sPattern ) ) continue;
			const std::string sPath = p->m_sDir + sName;
			struct stat st;
			if ( ::stat( sPath.c_str(), &st ) != 0 && ::lstat( sPath.c_str(), &st ) != 0 ) continue;	// vanished
			memset( pData, 0, sizeof( *pData ) );
			pData->dwFileAttributes = AttributesOf( st, sPath );
			UnixToFileTime( st.st_ctim, &pData->ftCreationTime );
			UnixToFileTime( st.st_atim, &pData->ftLastAccessTime );
			UnixToFileTime( st.st_mtim, &pData->ftLastWriteTime );
			const ULONGLONG nSize = S_ISDIR( st.st_mode ) ? 0 : (ULONGLONG)st.st_size;
			pData->nFileSizeHigh = (DWORD)( nSize >> 32 );
			pData->nFileSizeLow = (DWORD)nSize;
			const CStringW w( CA2W( sName.c_str(), CP_UTF8 ) );
			wcsncpy( pData->cFileName, w, MAX_PATH - 1 );
			return true;
		}
	}
}

HANDLE FindFirstFileW(LPCWSTR pszPattern, LPWIN32_FIND_DATAW pData)
{
	if ( ! pszPattern || ! pData ) { SetLastError( ERROR_INVALID_PARAMETER ); return INVALID_HANDLE_VALUE; }
	const std::string sFull = compat::ToNativePath( pszPattern );
	const size_t nSlash = sFull.rfind( '/' );
	FindObject* p = new FindObject();
	p->m_sDir = nSlash == std::string::npos ? std::string( "./" ) : sFull.substr( 0, nSlash + 1 );
	p->m_sPattern = nSlash == std::string::npos ? sFull : sFull.substr( nSlash + 1 );
	p->m_pDir = opendir( p->m_sDir.c_str() );
	if ( ! p->m_pDir )
	{
		Fail( errno == ENOENT ? ENOTDIR : errno );	// a missing folder is ERROR_PATH_NOT_FOUND
		p->Release();
		return INVALID_HANDLE_VALUE;
	}
	if ( ! Fill( p, pData ) )
	{
		p->Release();
		SetLastError( ERROR_FILE_NOT_FOUND );
		return INVALID_HANDLE_VALUE;
	}
	return p;
}

BOOL FindNextFileW(HANDLE hFind, LPWIN32_FIND_DATAW pData)
{
	FindObject* p = dynamic_cast< FindObject* >( compat::FromHandle( hFind ) );
	if ( ! p || ! pData ) { SetLastError( ERROR_INVALID_HANDLE ); return FALSE; }
	return Fill( p, pData ) ? TRUE : FALSE;
}

BOOL FindClose(HANDLE hFind)
{
	FindObject* p = dynamic_cast< FindObject* >( compat::FromHandle( hFind ) );
	if ( ! p ) { SetLastError( ERROR_INVALID_HANDLE ); return FALSE; }
	p->Release();
	return TRUE;
}

BOOL SetFileAttributesW(LPCWSTR pszPath, DWORD dwAttributes)
{
	if ( ! pszPath ) return FALSE;
	const std::string s = compat::ToNativePath( pszPath );
	struct stat st;
	if ( ::stat( s.c_str(), &st ) != 0 ) return Fail( errno );
	mode_t m = st.st_mode & 07777;
	m = ( dwAttributes & FILE_ATTRIBUTE_READONLY ) ? ( m & ~0222 ) : ( m | S_IWUSR );
	return ::chmod( s.c_str(), m ) == 0 ? TRUE : Fail( errno );
}

BOOL GetDiskFreeSpaceExW(LPCWSTR pszPath, PULARGE_INTEGER pFreeToCaller, PULARGE_INTEGER pTotal, PULARGE_INTEGER pFree)
{
	struct statvfs v;
	if ( ::statvfs( pszPath ? compat::ToNativePath( pszPath ).c_str() : ".", &v ) != 0 ) return Fail( errno );
	if ( pFreeToCaller ) pFreeToCaller->QuadPart = (ULONGLONG)v.f_bavail * v.f_frsize;
	if ( pTotal ) pTotal->QuadPart = (ULONGLONG)v.f_blocks * v.f_frsize;
	if ( pFree ) pFree->QuadPart = (ULONGLONG)v.f_bfree * v.f_frsize;
	return TRUE;
}

LPWSTR PathFindExtensionW(LPCWSTR pszPath)
{
	if ( ! pszPath ) return NULL;
	LPCWSTR pDot = NULL;
	LPCWSTR p = pszPath;
	for ( ; *p; ++p )
	{
		if ( *p == L'.' ) pDot = p;
		else if ( *p == L'\\' || *p == L'/' || *p == L' ' ) pDot = NULL;
	}
	return const_cast< LPWSTR >( pDot ? pDot : p );	// the end of the string when there is none
}

void PathRemoveExtensionW(LPWSTR pszPath)
{
	if ( LPWSTR p = PathFindExtensionW( pszPath ) ) *p = 0;
}

// ---------------------------------------------------------------------------
// CFile::GetStatus

namespace {
	BOOL StatusFromStat(const struct stat& st, const std::string& sPath, LPCTSTR pszName, CFileStatus& r)
	{
		r.m_ctime = CTime( (__time64_t)st.st_ctime );
		r.m_mtime = CTime( (__time64_t)st.st_mtime );
		r.m_atime = CTime( (__time64_t)st.st_atime );
		r.m_size = S_ISDIR( st.st_mode ) ? 0 : (ULONGLONG)st.st_size;
		r.m_attribute = (BYTE)AttributesOf( st, sPath );
		r.m_padding = 0;
		r.m_szFullName[ 0 ] = 0;
		if ( pszName ) wcsncpy( r.m_szFullName, pszName, MAX_PATH - 1 );
		r.m_szFullName[ MAX_PATH - 1 ] = 0;
		return TRUE;
	}
}

BOOL CFile::GetStatus(CFileStatus& rStatus) const
{
	struct stat st;
	if ( m_fd < 0 || ::fstat( m_fd, &st ) != 0 ) return FALSE;
	return StatusFromStat( st, compat::ToNativePath( m_strFileName ), m_strFileName, rStatus );
}

BOOL PASCAL CFile::GetStatus(LPCTSTR pszFileName, CFileStatus& rStatus)
{
	struct stat st;
	if ( ! pszFileName ) return FALSE;
	const std::string s = compat::ToNativePath( pszFileName );
	if ( ::stat( s.c_str(), &st ) != 0 ) return FALSE;
	return StatusFromStat( st, s, pszFileName, rStatus );
}
