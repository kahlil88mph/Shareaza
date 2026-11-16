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
BOOL MoveFileW(LPCWSTR a, LPCWSTR b)
{
	return a && b && ::rename( compat::ToNativePath( a ).c_str(), compat::ToNativePath( b ).c_str() ) == 0;
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
