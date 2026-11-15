//
// compat/afxcore.h (Linux compatibility layer)
//
// MFC core: diagnostics macros, CObject, exceptions (thrown as pointers, as
// MFC does), synchronisation objects, and the MFC collection templates
// (CList, CArray, CMap and their string/pointer variants).
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include <windows.h>
#include "cstring.h"
#include "handles.h"

#include <list>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>

#define AFXAPI
#define AFX_CDECL
#define AFX_INLINE inline
#define AFX_STATIC static
#define AFX_EXT_CLASS
#define AFX_NOVTABLE

#ifdef _DEBUG
#define ASSERT(f) assert(f)
#define VERIFY(f) assert(f)
#define ASSERT_VALID(p) assert((p) != NULL)
#else
#define ASSERT(f) ((void)0)
#define VERIFY(f) ((void)(f))
#define ASSERT_VALID(p) ((void)0)
#endif
#define ASSERT_POINTER(p, type) ASSERT((p) != NULL)
#define ASSERT_KINDOF(c, p) ((void)0)
#define TRACE(...) ((void)0)
#define TRACE0(s) ((void)0)
#define DEBUG_NEW new
#define DECLARE_DYNAMIC(c)
#define IMPLEMENT_DYNAMIC(c, b)
#define DECLARE_DYNCREATE(c)
#define IMPLEMENT_DYNCREATE(c, b)
#define DECLARE_SERIAL(c)
#define IMPLEMENT_SERIAL(c, b, v)
inline void AfxDebugBreak() { __builtin_trap(); }
inline BOOL AfxIsValidAddress(const void* p, UINT_PTR, BOOL = TRUE) { return p != NULL; }
inline BOOL AfxIsValidString(const wchar_t* p, int = -1) { return p != NULL; }
inline BOOL AfxIsValidString(const char* p, int = -1) { return p != NULL; }

// ---------------------------------------------------------------------------
// CObject

class CObject
{
public:
	virtual ~CObject() {}
	virtual void AssertValid() const {}
protected:
	CObject() {}
};

// ---------------------------------------------------------------------------
// Exceptions. MFC throws heap pointers: throw new CXxxException; catch
// ( CException* e ) { e->Delete(); }

class CException : public CObject
{
public:
	explicit CException(BOOL bAutoDelete = TRUE) : m_bAutoDelete( bAutoDelete ) {}
	void Delete() { if ( m_bAutoDelete ) delete this; }
	virtual BOOL GetErrorMessage(LPTSTR pszError, UINT nMaxError, PUINT = NULL) const
	{
		if ( pszError && nMaxError ) *pszError = 0;
		return FALSE;
	}
	virtual int ReportError(UINT = 0, UINT = 0) { return 0; }
protected:
	BOOL m_bAutoDelete;
};


class CMemoryException : public CException {};
class CNotSupportedException : public CException {};
class CInvalidArgException : public CException {};
class CUserException : public CException {};
class CResourceException : public CException {};

class CFileException : public CException
{
public:
	enum {
		none, genericException, fileNotFound, badPath, tooManyOpenFiles, accessDenied,
		invalidFile, removeCurrentDir, directoryFull, badSeek, hardIO, sharingViolation,
		lockViolation, diskFull, endOfFile
	};
	explicit CFileException(int cause = none, LONG lOsError = -1, LPCTSTR pszFileName = NULL)
		: m_cause( cause ), m_lOsError( lOsError ), m_strFileName( pszFileName ) {}
	int m_cause;
	LONG m_lOsError;
	CString m_strFileName;
};

class CArchiveException : public CException
{
public:
	enum {
		none, genericException, readOnly, endOfFile, writeOnly, badIndex, badClass, badSchema,
		bufferFull
	};
	explicit CArchiveException(int cause = none, LPCTSTR pszFileName = NULL)
		: m_cause( cause ), m_strFileName( pszFileName ) {}
	int m_cause;
	CString m_strFileName;
};

[[noreturn]] inline void AfxThrowMemoryException() { throw new CMemoryException(); }
[[noreturn]] inline void AfxThrowNotSupportedException() { throw new CNotSupportedException(); }
[[noreturn]] inline void AfxThrowInvalidArgException() { throw new CInvalidArgException(); }
[[noreturn]] inline void AfxThrowUserException() { throw new CUserException(); }
[[noreturn]] inline void AfxThrowResourceException() { throw new CResourceException(); }
[[noreturn]] inline void AfxThrowFileException(int cause, LONG lOsError = -1, LPCTSTR pszFileName = NULL)
	{ throw new CFileException( cause, lOsError, pszFileName ); }
[[noreturn]] inline void AfxThrowArchiveException(int cause, LPCTSTR pszArchiveName = NULL)
	{ throw new CArchiveException( cause, pszArchiveName ); }

// ---------------------------------------------------------------------------
// Synchronisation (afxmt.h). Windows mutexes and critical sections are
// recursive; these are too.

class CSyncObject : public CObject
{
public:
	CSyncObject() : m_hObject( NULL ) {}
	operator HANDLE() const { return m_hObject; }
	HANDLE m_hObject;	// waitable handle (events); NULL for locks without one
	virtual BOOL Lock(DWORD dwTimeout = INFINITE) = 0;
	virtual BOOL Unlock() = 0;
	virtual BOOL Unlock(LONG, LPLONG = NULL) { return Unlock(); }
};

class CCriticalSection : public CSyncObject
{
public:
	CCriticalSection() { InitializeCriticalSection( &m_sect ); }
	~CCriticalSection() { DeleteCriticalSection( &m_sect ); }
	BOOL Lock() { EnterCriticalSection( &m_sect ); return TRUE; }
	BOOL Lock(DWORD) override { return Lock(); }
	BOOL Unlock() override { LeaveCriticalSection( &m_sect ); return TRUE; }
	operator CRITICAL_SECTION*() { return &m_sect; }
	CRITICAL_SECTION m_sect;
private:
	CCriticalSection(const CCriticalSection&);
	CCriticalSection& operator=(const CCriticalSection&);
};

class CMutex : public CSyncObject
{
public:
	explicit CMutex(BOOL bInitiallyOwn = FALSE, LPCTSTR = NULL, void* = NULL)
	{
		if ( bInitiallyOwn ) m_m.lock();
	}
	BOOL Lock(DWORD dwTimeout = INFINITE) override
	{
		if ( dwTimeout == INFINITE ) { m_m.lock(); return TRUE; }
		return m_m.try_lock_for( std::chrono::milliseconds( dwTimeout ) ) ? TRUE : FALSE;
	}
	BOOL Unlock() override { m_m.unlock(); return TRUE; }
private:
	std::recursive_timed_mutex m_m;
	CMutex(const CMutex&);
	CMutex& operator=(const CMutex&);
};

class CEvent : public CSyncObject
{
public:
	explicit CEvent(BOOL bInitiallyOwn = FALSE, BOOL bManualReset = FALSE, LPCTSTR pszName = NULL, LPSECURITY_ATTRIBUTES pAttr = NULL)
	{
		m_hObject = CreateEventW( pAttr, bManualReset, bInitiallyOwn, pszName );
	}
	~CEvent() { CloseHandle( m_hObject ); }
	BOOL SetEvent() { return ::SetEvent( m_hObject ); }
	BOOL ResetEvent() { return ::ResetEvent( m_hObject ); }
	BOOL PulseEvent() { return ::PulseEvent( m_hObject ); }
	BOOL Lock(DWORD dwTimeout = INFINITE) override { return WaitForSingleObject( m_hObject, dwTimeout ) == WAIT_OBJECT_0; }
	BOOL Unlock() override { return TRUE; }
private:
	CEvent(const CEvent&);
	CEvent& operator=(const CEvent&);
};

class CSingleLock
{
public:
	explicit CSingleLock(CSyncObject* pObject, BOOL bInitialLock = FALSE)
		: m_pObject( pObject ), m_bAcquired( FALSE )
	{
		if ( bInitialLock ) Lock();
	}
	~CSingleLock() { Unlock(); }
	BOOL Lock(DWORD dwTimeout = INFINITE)
	{
		if ( ! m_bAcquired ) m_bAcquired = m_pObject->Lock( dwTimeout );
		return m_bAcquired;
	}
	BOOL Unlock()
	{
		if ( m_bAcquired ) { m_pObject->Unlock(); m_bAcquired = FALSE; }
		return TRUE;
	}
	BOOL IsLocked() const { return m_bAcquired; }
private:
	CSyncObject* m_pObject;
	BOOL m_bAcquired;
	CSingleLock(const CSingleLock&);
	CSingleLock& operator=(const CSingleLock&);
};

// ---------------------------------------------------------------------------
// Threads (CWinThread). Same lifecycle as MFC: a thread function, or
// InitInstance then Run (or ExitInstance if InitInstance fails); an
// auto-delete thread object deletes itself when the thread ends.

typedef UINT (AFX_CDECL *AFX_THREADPROC)(LPVOID);

class CWinThread : public CObject
{
public:
	explicit CWinThread(AFX_THREADPROC pfnThreadProc = NULL, LPVOID pParam = NULL)
		: m_bAutoDelete( TRUE ), m_hThread( NULL ), m_nThreadID( 0 ),
		  m_pThreadParams( pParam ), m_pfnThreadProc( pfnThreadProc ) {}
	virtual ~CWinThread() { if ( m_hThread ) CloseHandle( m_hThread ); }

	BOOL CreateThread(DWORD dwCreateFlags = 0, UINT nStackSize = 0, LPSECURITY_ATTRIBUTES = NULL);
	virtual BOOL InitInstance() { return FALSE; }
	virtual int Run() { return ExitInstance(); }	// no message pump on Linux
	virtual int ExitInstance() { return 0; }
	virtual void Delete() { if ( m_bAutoDelete ) delete this; }
	DWORD ResumeThread() { return ::ResumeThread( m_hThread ); }
	DWORD SuspendThread() { return (DWORD)-1; }
	BOOL SetThreadPriority(int nPriority) { return ::SetThreadPriority( m_hThread, nPriority ); }
	int GetThreadPriority() { return ::GetThreadPriority( m_hThread ); }
	operator HANDLE() const { return m_hThread; }

	BOOL m_bAutoDelete;
	HANDLE m_hThread;
	DWORD m_nThreadID;
	LPVOID m_pThreadParams;
	AFX_THREADPROC m_pfnThreadProc;
private:
	CWinThread(const CWinThread&);
	CWinThread& operator=(const CWinThread&);
};

CWinThread* AfxBeginThread(AFX_THREADPROC pfnThreadProc, LPVOID pParam, int nPriority = THREAD_PRIORITY_NORMAL,
	UINT nStackSize = 0, DWORD dwCreateFlags = 0, LPSECURITY_ATTRIBUTES lpSecurityAttrs = NULL);
#define UNUSED(x) ((void)(x))
#define UNUSED_ALWAYS(x) ((void)(x))

// ---------------------------------------------------------------------------
// Collections (afxtempl.h). POSITION is an opaque node pointer.

struct __POSITION {};
typedef __POSITION* POSITION;
#define BEFORE_START_POSITION ((POSITION)-1L)

template< class TYPE, class ARG_TYPE = const TYPE& >
class CList : public CObject
{
	struct CNode { CNode* pNext; CNode* pPrev; TYPE data; };
public:
	explicit CList(INT_PTR = 10) : m_pHead( NULL ), m_pTail( NULL ), m_nCount( 0 ) {}
	~CList() { RemoveAll(); }

	INT_PTR GetCount() const { return m_nCount; }
	INT_PTR GetSize() const { return m_nCount; }
	BOOL IsEmpty() const { return m_nCount == 0; }

	TYPE& GetHead() { ASSERT( m_pHead ); return m_pHead->data; }
	const TYPE& GetHead() const { ASSERT( m_pHead ); return m_pHead->data; }
	TYPE& GetTail() { ASSERT( m_pTail ); return m_pTail->data; }
	const TYPE& GetTail() const { ASSERT( m_pTail ); return m_pTail->data; }

	TYPE RemoveHead()
	{
		CNode* p = m_pHead; TYPE v = p->data;
		m_pHead = p->pNext;
		if ( m_pHead ) m_pHead->pPrev = NULL; else m_pTail = NULL;
		delete p; --m_nCount;
		return v;
	}
	TYPE RemoveTail()
	{
		CNode* p = m_pTail; TYPE v = p->data;
		m_pTail = p->pPrev;
		if ( m_pTail ) m_pTail->pNext = NULL; else m_pHead = NULL;
		delete p; --m_nCount;
		return v;
	}
	POSITION AddHead(ARG_TYPE v) { return InsertBefore( (POSITION)m_pHead, v ); }
	POSITION AddTail(ARG_TYPE v) { return InsertAfter( (POSITION)m_pTail, v ); }
	void AddTail(const CList* pOther) { for ( CNode* p = pOther->m_pHead; p; p = p->pNext ) AddTail( p->data ); }
	void AddHead(const CList* pOther) { for ( CNode* p = pOther->m_pTail; p; p = p->pPrev ) AddHead( p->data ); }
	void RemoveAll()
	{
		for ( CNode* p = m_pHead; p; ) { CNode* n = p->pNext; delete p; p = n; }
		m_pHead = m_pTail = NULL; m_nCount = 0;
	}

	POSITION GetHeadPosition() const { return (POSITION)m_pHead; }
	POSITION GetTailPosition() const { return (POSITION)m_pTail; }
	TYPE& GetNext(POSITION& pos) { CNode* p = (CNode*)pos; pos = (POSITION)p->pNext; return p->data; }
	const TYPE& GetNext(POSITION& pos) const { CNode* p = (CNode*)pos; pos = (POSITION)p->pNext; return p->data; }
	TYPE& GetPrev(POSITION& pos) { CNode* p = (CNode*)pos; pos = (POSITION)p->pPrev; return p->data; }
	const TYPE& GetPrev(POSITION& pos) const { CNode* p = (CNode*)pos; pos = (POSITION)p->pPrev; return p->data; }
	TYPE& GetAt(POSITION pos) { return ( (CNode*)pos )->data; }
	const TYPE& GetAt(POSITION pos) const { return ( (CNode*)pos )->data; }
	void SetAt(POSITION pos, ARG_TYPE v) { ( (CNode*)pos )->data = v; }
	void RemoveAt(POSITION pos)
	{
		CNode* p = (CNode*)pos;
		if ( p->pPrev ) p->pPrev->pNext = p->pNext; else m_pHead = p->pNext;
		if ( p->pNext ) p->pNext->pPrev = p->pPrev; else m_pTail = p->pPrev;
		delete p; --m_nCount;
	}
	POSITION InsertBefore(POSITION pos, ARG_TYPE v)
	{
		CNode* pAt = (CNode*)pos;
		CNode* p = new CNode{ pAt, pAt ? pAt->pPrev : m_pTail, v };
		if ( ! pAt ) { if ( m_pTail ) m_pTail->pNext = p; else m_pHead = p; m_pTail = p; p->pNext = NULL; }
		else { if ( pAt->pPrev ) pAt->pPrev->pNext = p; else m_pHead = p; pAt->pPrev = p; }
		++m_nCount;
		return (POSITION)p;
	}
	POSITION InsertAfter(POSITION pos, ARG_TYPE v)
	{
		CNode* pAt = (CNode*)pos;
		if ( ! pAt ) return InsertBefore( (POSITION)m_pHead, v );
		CNode* p = new CNode{ pAt->pNext, pAt, v };
		if ( pAt->pNext ) pAt->pNext->pPrev = p; else m_pTail = p;
		pAt->pNext = p;
		++m_nCount;
		return (POSITION)p;
	}
	POSITION Find(ARG_TYPE v, POSITION startAfter = NULL) const
	{
		CNode* p = startAfter ? ( (CNode*)startAfter )->pNext : m_pHead;
		for ( ; p; p = p->pNext ) if ( p->data == v ) return (POSITION)p;
		return NULL;
	}
	POSITION FindIndex(INT_PTR n) const
	{
		if ( n < 0 || n >= m_nCount ) return NULL;
		CNode* p = m_pHead;
		while ( n-- ) p = p->pNext;
		return (POSITION)p;
	}

private:
	CNode* m_pHead;
	CNode* m_pTail;
	INT_PTR m_nCount;
	CList(const CList&);
	CList& operator=(const CList&);
};

template< class TYPE, class ARG_TYPE = const TYPE& >
class CArray : public CObject
{
public:
	INT_PTR GetCount() const { return (INT_PTR)m_v.size(); }
	INT_PTR GetSize() const { return (INT_PTR)m_v.size(); }
	INT_PTR GetUpperBound() const { return (INT_PTR)m_v.size() - 1; }
	BOOL IsEmpty() const { return m_v.empty(); }
	void SetSize(INT_PTR n, INT_PTR = -1) { m_v.resize( (size_t)n ); }
	void RemoveAll() { m_v.clear(); }
	void FreeExtra() { m_v.shrink_to_fit(); }
	TYPE& GetAt(INT_PTR i) { return m_v[ (size_t)i ]; }
	const TYPE& GetAt(INT_PTR i) const { return m_v[ (size_t)i ]; }
	void SetAt(INT_PTR i, ARG_TYPE v) { m_v[ (size_t)i ] = v; }
	TYPE& ElementAt(INT_PTR i) { return m_v[ (size_t)i ]; }
	TYPE* GetData() { return m_v.data(); }
	const TYPE* GetData() const { return m_v.data(); }
	TYPE& operator[](INT_PTR i) { return m_v[ (size_t)i ]; }
	const TYPE& operator[](INT_PTR i) const { return m_v[ (size_t)i ]; }
	void SetAtGrow(INT_PTR i, ARG_TYPE v) { if ( i >= GetSize() ) m_v.resize( (size_t)i + 1 ); m_v[ (size_t)i ] = v; }
	INT_PTR Add(ARG_TYPE v) { m_v.push_back( v ); return GetUpperBound(); }
	INT_PTR Append(const CArray& o) { const INT_PTR n = GetSize(); m_v.insert( m_v.end(), o.m_v.begin(), o.m_v.end() ); return n; }
	void Copy(const CArray& o) { m_v = o.m_v; }
	void InsertAt(INT_PTR i, ARG_TYPE v, INT_PTR nCount = 1)
	{
		if ( i > GetSize() ) m_v.resize( (size_t)i );
		m_v.insert( m_v.begin() + i, (size_t)nCount, v );
	}
	void RemoveAt(INT_PTR i, INT_PTR nCount = 1) { m_v.erase( m_v.begin() + i, m_v.begin() + i + nCount ); }
private:
	std::vector< TYPE > m_v;
};

// MFC hashing helpers, which StdAfx.h specialises for its own key types.
template< class ARG_KEY > inline UINT AFXAPI HashKey(ARG_KEY key) { return (UINT)( (DWORD_PTR)key >> 4 ); }
template<> inline UINT AFXAPI HashKey< __int64 >(__int64 key) { return (UINT)( key >> 4 ) ^ (UINT)( key >> 36 ); }
template< class TYPE, class ARG_TYPE > inline BOOL AFXAPI CompareElements(const TYPE* a, const ARG_TYPE* b) { return *a == *b; }

template< class KEY, class ARG_KEY, class VALUE, class ARG_VALUE >
class CMap : public CObject
{
public:
	class CPair
	{
	public:
		const KEY key;
		VALUE value;
	protected:
		explicit CPair(ARG_KEY k) : key( k ), value() {}
	};
private:
	class CAssoc : public CPair
	{
	public:
		explicit CAssoc(ARG_KEY k) : CPair( k ), pNext( NULL ), nHash( 0 ) {}
		CAssoc* pNext;
		UINT nHash;
	};
public:
	explicit CMap(INT_PTR = 10) : m_nHashTableSize( 17 ), m_nCount( 0 ) {}
	~CMap() { RemoveAll(); }
	INT_PTR GetCount() const { return m_nCount; }
	INT_PTR GetSize() const { return m_nCount; }
	BOOL IsEmpty() const { return m_nCount == 0; }
	void InitHashTable(UINT nSize, BOOL = TRUE) { ASSERT( m_nCount == 0 ); m_nHashTableSize = nSize ? nSize : 17; m_table.clear(); }
	UINT GetHashTableSize() const { return m_nHashTableSize; }

	BOOL Lookup(ARG_KEY key, VALUE& rValue) const
	{
		const CAssoc* p = Find( key );
		if ( ! p ) return FALSE;
		rValue = p->value;
		return TRUE;
	}
	const CPair* PLookup(ARG_KEY key) const { return Find( key ); }
	CPair* PLookup(ARG_KEY key) { return const_cast< CAssoc* >( Find( key ) ); }
	VALUE& operator[](ARG_KEY key)
	{
		if ( CAssoc* p = const_cast< CAssoc* >( Find( key ) ) ) return p->value;
		if ( m_table.empty() ) m_table.assign( m_nHashTableSize, NULL );
		CAssoc* p = new CAssoc( key );
		p->nHash = HashKey< ARG_KEY >( key );
		CAssoc*& head = m_table[ p->nHash % m_nHashTableSize ];
		p->pNext = head; head = p;
		++m_nCount;
		return p->value;
	}
	void SetAt(ARG_KEY key, ARG_VALUE v) { ( *this )[ key ] = v; }
	BOOL RemoveKey(ARG_KEY key)
	{
		if ( m_table.empty() ) return FALSE;
		CAssoc** pp = &m_table[ HashKey< ARG_KEY >( key ) % m_nHashTableSize ];
		for ( ; *pp; pp = &( *pp )->pNext )
		{
			if ( CompareElements( &( *pp )->key, &key ) )
			{
				CAssoc* p = *pp; *pp = p->pNext; delete p; --m_nCount;
				return TRUE;
			}
		}
		return FALSE;
	}
	void RemoveAll()
	{
		for ( CAssoc*& head : m_table )
			for ( CAssoc* p = head; p; ) { CAssoc* n = p->pNext; delete p; p = n; }
		m_table.clear();
		m_nCount = 0;
	}
	POSITION GetStartPosition() const { return m_nCount ? (POSITION)First( 0 ) : NULL; }
	void GetNextAssoc(POSITION& pos, KEY& rKey, VALUE& rValue) const
	{
		CAssoc* p = (CAssoc*)pos;
		rKey = p->key; rValue = p->value;
		pos = (POSITION)Next( p );
	}
	CPair* PGetFirstAssoc() { return m_nCount ? First( 0 ) : NULL; }
	CPair* PGetNextAssoc(const CPair* pPair) { return Next( (CAssoc*)pPair ); }

private:
	std::vector< CAssoc* > m_table;
	UINT m_nHashTableSize;
	INT_PTR m_nCount;

	const CAssoc* Find(ARG_KEY key) const
	{
		if ( m_table.empty() ) return NULL;
		const UINT nHash = HashKey< ARG_KEY >( key );
		for ( CAssoc* p = m_table[ nHash % m_nHashTableSize ]; p; p = p->pNext )
			if ( p->nHash == nHash && CompareElements( &p->key, &key ) ) return p;
		return NULL;
	}
	CAssoc* First(size_t nBucket) const
	{
		for ( ; nBucket < m_table.size(); ++nBucket ) if ( m_table[ nBucket ] ) return m_table[ nBucket ];
		return NULL;
	}
	CAssoc* Next(CAssoc* p) const
	{
		if ( p->pNext ) return p->pNext;
		return First( p->nHash % m_nHashTableSize + 1 );
	}
	CMap(const CMap&);
	CMap& operator=(const CMap&);
};

class CStringList : public CList< CString, LPCTSTR > {};
class CStringArray : public CArray< CString, LPCTSTR > {};
class CPtrList : public CList< void*, void* > {};
class CPtrArray : public CArray< void*, void* > {};
class CDWordArray : public CArray< DWORD, DWORD > {};
class CByteArray : public CArray< BYTE, BYTE > {};
class CUIntArray : public CArray< UINT, UINT > {};
class CWordArray : public CArray< WORD, WORD > {};
class CObList : public CList< CObject*, CObject* > {};
class CObArray : public CArray< CObject*, CObject* > {};
class CMapStringToString : public CMap< CString, LPCTSTR, CString, LPCTSTR > {};
class CMapStringToPtr : public CMap< CString, LPCTSTR, void*, void* > {};
class CMapPtrToPtr : public CMap< void*, void*, void*, void* > {};
class CMapStringToOb : public CMap< CString, LPCTSTR, CObject*, CObject* > {};
class CMapWordToPtr : public CMap< WORD, WORD, void*, void* > {};
class CMapPtrToWord : public CMap< void*, void*, WORD, WORD > {};

template<> inline UINT AFXAPI HashKey< LPCTSTR >(LPCTSTR key)
{
	UINT nHash = 0;
	for ( ; key && *key; ++key ) nHash = ( nHash << 5 ) + nHash + *key;
	return nHash;
}
