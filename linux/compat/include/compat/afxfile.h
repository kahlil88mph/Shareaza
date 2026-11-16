//
// compat/afxfile.h (Linux compatibility layer)
//
// MFC CFile and CArchive. CArchive reads and writes the same bytes as MFC,
// so .dat files written by the Windows build load here: strings are stored
// with MFC's length prefixes as UTF-16LE whatever the size of wchar_t.
//
// Paths: the engine builds Windows paths. CFile accepts them, turning '\'
// into '/' and dropping a "\\?\" prefix (see compat::ToNativePath).
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include "afxcore.h"

namespace compat {
	// Windows path (wide) -> POSIX path (UTF-8).
	std::string ToNativePath(LPCTSTR pszPath);
	// POSIX path (UTF-8) -> the engine's form, with '\\' separators, which
	// is what its path code (ReverseFind( '\\' ) and so on) expects.
	CString ToEnginePath(const char* pszPath);
}

// shlwapi / kernel32 path helpers on POSIX paths.
BOOL PathFileExistsW(LPCWSTR pszPath);
BOOL PathIsDirectoryW(LPCWSTR pszPath);
BOOL CreateDirectoryW(LPCWSTR pszPath, LPSECURITY_ATTRIBUTES);
BOOL RemoveDirectoryW(LPCWSTR pszPath);
BOOL DeleteFileW(LPCWSTR pszPath);
BOOL MoveFileW(LPCWSTR pszFrom, LPCWSTR pszTo);
#define PathFileExists PathFileExistsW
#define PathIsDirectory PathIsDirectoryW
#define CreateDirectory CreateDirectoryW	// as in the SDK; Shareaza's one-argument helper becomes an overload
#define RemoveDirectory RemoveDirectoryW
#define DeleteFile DeleteFileW
#define MoveFile MoveFileW
BOOL CopyFileW(LPCWSTR pszFrom, LPCWSTR pszTo, BOOL bFailIfExists);
#define CopyFile CopyFileW

// Moves and copies with progress callbacks (finished downloads use these to
// reach the downloads folder, possibly on another filesystem).
typedef DWORD (CALLBACK *LPPROGRESS_ROUTINE)(LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred,
	LARGE_INTEGER StreamSize, LARGE_INTEGER StreamBytesTransferred, DWORD dwStreamNumber, DWORD dwCallbackReason,
	HANDLE hSourceFile, HANDLE hDestinationFile, LPVOID lpData);
#define PROGRESS_CONTINUE 0
#define PROGRESS_CANCEL   1
#define PROGRESS_STOP     2
#define PROGRESS_QUIET    3
#define CALLBACK_CHUNK_FINISHED 0x00000000
#define CALLBACK_STREAM_SWITCH  0x00000001
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#define MOVEFILE_COPY_ALLOWED     0x00000002
#define MOVEFILE_DELAY_UNTIL_REBOOT 0x00000004
#define MOVEFILE_WRITE_THROUGH    0x00000008
#define COPY_FILE_FAIL_IF_EXISTS  0x00000001
#define ERROR_REQUEST_ABORTED     1235u
BOOL MoveFileExW(LPCWSTR pszFrom, LPCWSTR pszTo, DWORD dwFlags);
BOOL MoveFileWithProgressW(LPCWSTR pszFrom, LPCWSTR pszTo, LPPROGRESS_ROUTINE pfnProgress, LPVOID pData, DWORD dwFlags);
BOOL CopyFileExW(LPCWSTR pszFrom, LPCWSTR pszTo, LPPROGRESS_ROUTINE pfnProgress, LPVOID pData, LPBOOL pbCancel, DWORD dwCopyFlags);
#define MoveFileEx MoveFileExW
#define MoveFileWithProgress MoveFileWithProgressW
#define CopyFileEx CopyFileExW
// Absolute, normalised path in the engine's form ("a\\..\\b" -> "b").
DWORD GetFullPathNameW(LPCWSTR pszFileName, DWORD nBufferLength, LPWSTR pszBuffer, LPWSTR* ppFilePart);
#define GetFullPathName GetFullPathNameW
LPWSTR PathFindFileNameW(LPCWSTR pszPath);
#define PathFindFileName PathFindFileNameW

struct CFileStatus
{
	CTime m_ctime;		// creation (inode change on Linux)
	CTime m_mtime;		// last modification
	CTime m_atime;		// last access
	ULONGLONG m_size;
	BYTE m_attribute;
	BYTE m_padding;
	TCHAR m_szFullName[ MAX_PATH ];
};

class CFile : public CObject
{
public:
	enum OpenFlags {
		modeRead = 0x0000, modeWrite = 0x0001, modeReadWrite = 0x0002,
		shareCompat = 0x0000, shareExclusive = 0x0010, shareDenyWrite = 0x0020,
		shareDenyRead = 0x0030, shareDenyNone = 0x0040, modeNoInherit = 0x0080,
		modeCreate = 0x1000, modeNoTruncate = 0x2000, typeText = 0x4000,
		typeBinary = 0x8000, osNoBuffer = 0x10000, osWriteThrough = 0x20000,
		osRandomAccess = 0x40000, osSequentialScan = 0x80000
	};
	enum SeekPosition { begin = 0x0, current = 0x1, end = 0x2 };
	static const HANDLE hFileNull;

	CFile() : m_hFile( hFileNull ), m_fd( -1 ) {}
	CFile(LPCTSTR pszFileName, UINT nOpenFlags) : m_hFile( hFileNull ), m_fd( -1 )
	{
		CFileException* e = new CFileException();
		if ( ! Open( pszFileName, nOpenFlags, e ) ) throw e;
		e->Delete();
	}
	virtual ~CFile() { if ( m_fd >= 0 ) Close(); }

	virtual BOOL Open(LPCTSTR pszFileName, UINT nOpenFlags, CFileException* pError = NULL);
	virtual UINT Read(void* lpBuf, UINT nCount);
	virtual void Write(const void* lpBuf, UINT nCount);
	virtual ULONGLONG Seek(LONGLONG lOff, UINT nFrom);
	ULONGLONG SeekToEnd() { return Seek( 0, end ); }
	void SeekToBegin() { Seek( 0, begin ); }
	virtual ULONGLONG GetPosition() const;
	virtual ULONGLONG GetLength() const;
	virtual void SetLength(ULONGLONG dwNewLen);
	virtual void Flush();
	virtual void Close();
	virtual void Abort();
	CString GetFilePath() const { return m_strFileName; }
	CString GetFileName() const;
	BOOL GetStatus(CFileStatus& rStatus) const;
	static BOOL PASCAL GetStatus(LPCTSTR pszFileName, CFileStatus& rStatus);
	int GetDescriptor() const { return m_fd; }
	operator HANDLE() const { return m_fd < 0 ? INVALID_HANDLE_VALUE : (HANDLE)(INT_PTR)m_fd; }

	CString m_strFileName;
	HANDLE m_hFile;	// as in MFC: hFileNull when closed
protected:
	int m_fd;
private:
	CFile(const CFile&);
	CFile& operator=(const CFile&);
};

class CArchive
{
public:
	enum Mode { store = 0, load = 1, bNoFlushOnDelete = 2, bNoByteSwap = 4 };

	CArchive(CFile* pFile, UINT nMode, int nBufSize = 4096, void* lpBuf = NULL);
	~CArchive();

	BOOL IsLoading() const { return m_bLoading; }
	BOOL IsStoring() const { return ! m_bLoading; }
	CFile* GetFile() const { return m_pFile; }
	UINT Read(void* lpBuf, UINT nMax);
	void Write(const void* lpBuf, UINT nMax);
	void Flush();
	void Close();
	void Abort();

	// MFC string length prefix (used by CString and by Shareaza's own counts)
	void WriteCount(DWORD_PTR dwCount);
	DWORD_PTR ReadCount();
	void WriteString(LPCTSTR lpsz);
	BOOL ReadString(CString& rString);

	CArchive& operator<<(BYTE v) { Write( &v, 1 ); return *this; }
	CArchive& operator<<(char v) { Write( &v, 1 ); return *this; }
	CArchive& operator<<(signed char v) { Write( &v, 1 ); return *this; }
	CArchive& operator<<(bool v) { BYTE b = v ? 1 : 0; Write( &b, 1 ); return *this; }
	CArchive& operator<<(WORD v) { Write( &v, 2 ); return *this; }
	CArchive& operator<<(short v) { Write( &v, 2 ); return *this; }
	CArchive& operator<<(wchar_t v) { WORD w = (WORD)v; Write( &w, 2 ); return *this; }	// UTF-16 unit, as on Windows
	CArchive& operator<<(int v) { Write( &v, 4 ); return *this; }
	CArchive& operator<<(UINT v) { Write( &v, 4 ); return *this; }
	CArchive& operator<<(long v) { int32_t n = (int32_t)v; Write( &n, 4 ); return *this; }	// Windows long is 32-bit
	CArchive& operator<<(unsigned long v) { uint32_t n = (uint32_t)v; Write( &n, 4 ); return *this; }
	CArchive& operator<<(LONGLONG v) { Write( &v, 8 ); return *this; }
	CArchive& operator<<(ULONGLONG v) { Write( &v, 8 ); return *this; }
	CArchive& operator<<(float v) { Write( &v, 4 ); return *this; }
	CArchive& operator<<(double v) { Write( &v, 8 ); return *this; }
	CArchive& operator<<(const CStringW& s);
	CArchive& operator<<(const CStringA& s);
	CArchive& operator<<(const CTime& t) { LONGLONG n = t.GetTime(); Write( &n, 8 ); return *this; }	// __time64_t, as MFC stores it
	CArchive& operator<<(const CTimeSpan& t) { LONGLONG n = t.GetTimeSpan(); Write( &n, 8 ); return *this; }

	CArchive& operator>>(BYTE& v) { Fill( &v, 1 ); return *this; }
	CArchive& operator>>(char& v) { Fill( &v, 1 ); return *this; }
	CArchive& operator>>(signed char& v) { Fill( &v, 1 ); return *this; }
	CArchive& operator>>(bool& v) { BYTE b; Fill( &b, 1 ); v = b != 0; return *this; }
	CArchive& operator>>(WORD& v) { Fill( &v, 2 ); return *this; }
	CArchive& operator>>(short& v) { Fill( &v, 2 ); return *this; }
	CArchive& operator>>(wchar_t& v) { WORD w; Fill( &w, 2 ); v = w; return *this; }
	CArchive& operator>>(int& v) { Fill( &v, 4 ); return *this; }
	CArchive& operator>>(UINT& v) { Fill( &v, 4 ); return *this; }
	CArchive& operator>>(long& v) { int32_t n; Fill( &n, 4 ); v = n; return *this; }
	CArchive& operator>>(unsigned long& v) { uint32_t n; Fill( &n, 4 ); v = n; return *this; }
	CArchive& operator>>(LONGLONG& v) { Fill( &v, 8 ); return *this; }
	CArchive& operator>>(ULONGLONG& v) { Fill( &v, 8 ); return *this; }
	CArchive& operator>>(float& v) { Fill( &v, 4 ); return *this; }
	CArchive& operator>>(double& v) { Fill( &v, 8 ); return *this; }
	CArchive& operator>>(CStringW& s);
	CArchive& operator>>(CStringA& s);
	CArchive& operator>>(CTime& t) { LONGLONG n; Fill( &n, 8 ); t = CTime( n ); return *this; }
	CArchive& operator>>(CTimeSpan& t) { LONGLONG n; Fill( &n, 8 ); t = CTimeSpan( n ); return *this; }

	int m_nObjectSchema;
	CString m_strFileName;

private:
	void Fill(void* p, UINT n);	// throws CArchiveException::endOfFile when short
	CFile* m_pFile;
	BOOL m_bLoading;
	std::vector< BYTE > m_buf;
	size_t m_nPos;	// read position in m_buf (load) or bytes pending (store)
	size_t m_nLen;
	CArchive(const CArchive&);
	CArchive& operator=(const CArchive&);
};

// ---------------------------------------------------------------------------
// Directory listing (FindFirstFile and friends) over opendir/fnmatch.
// Patterns use Windows wildcards (* and ?), matched case-insensitively as on
// Windows. "." and ".." are reported, as Windows does.

typedef struct _WIN32_FIND_DATAW {
	DWORD    dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD    nFileSizeHigh;
	DWORD    nFileSizeLow;
	DWORD    dwReserved0;
	DWORD    dwReserved1;
	WCHAR    cFileName[ MAX_PATH ];
	WCHAR    cAlternateFileName[ 14 ];
} WIN32_FIND_DATAW, *LPWIN32_FIND_DATAW, WIN32_FIND_DATA, *LPWIN32_FIND_DATA;

HANDLE FindFirstFileW(LPCWSTR pszPattern, LPWIN32_FIND_DATAW pData);
BOOL FindNextFileW(HANDLE hFind, LPWIN32_FIND_DATAW pData);
BOOL FindClose(HANDLE hFind);
#define FindFirstFile FindFirstFileW
#define FindNextFile FindNextFileW

BOOL SetFileAttributesW(LPCWSTR pszPath, DWORD dwAttributes);	// FILE_ATTRIBUTE_READONLY only
#define SetFileAttributes SetFileAttributesW
BOOL GetDiskFreeSpaceExW(LPCWSTR pszPath, PULARGE_INTEGER pFreeToCaller, PULARGE_INTEGER pTotal, PULARGE_INTEGER pFree);
#define GetDiskFreeSpaceEx GetDiskFreeSpaceExW

// shlwapi path helpers on the engine's '\\' paths
LPWSTR PathFindExtensionW(LPCWSTR pszPath);
void PathRemoveExtensionW(LPWSTR pszPath);
#define PathFindExtension PathFindExtensionW
#define PathRemoveExtension PathRemoveExtensionW
