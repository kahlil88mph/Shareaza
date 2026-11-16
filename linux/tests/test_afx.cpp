// Tests for the MFC/ATL subset: CString semantics the engine depends on,
// collections, exceptions and locks.

#include <compat/afxcore.h>
#include <compat/atlcoll.h>

#include <cstdio>
#include <thread>
#include <vector>

static int g_nFailed = 0, g_nPassed = 0;
#define CHECK(x) do { if ( x ) ++g_nPassed; else { ++g_nFailed; printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); } } while ( 0 )

static void TestCString()
{
	CString s( L"  Hello, World  " );
	CHECK( s.GetLength() == 16 );
	s.Trim();
	CHECK( s == L"Hello, World" );
	CHECK( s.Find( L',' ) == 5 && s.Find( L"World" ) == 7 && s.Find( L'z' ) == -1 && s.Find( L'o', 5 ) == 8 );
	CHECK( s.ReverseFind( L'o' ) == 8 );
	CHECK( s.Left( 5 ) == L"Hello" && s.Right( 5 ) == L"World" && s.Mid( 7 ) == L"World" && s.Mid( 7, 2 ) == L"Wo" );
	CHECK( s.Mid( 50 ).IsEmpty() && s.Left( 99 ) == s && s.Right( -1 ).IsEmpty() && s.Mid( -3, 2 ) == L"He" );
	CHECK( s.CompareNoCase( L"hello, world" ) == 0 && s.Compare( L"hello, world" ) != 0 );
	CString t( s ); t.MakeLower();
	CHECK( t == L"hello, world" );
	CHECK( t.Replace( L"o", L"00" ) == 2 && t == L"hell00, w00rld" );
	CHECK( t.Replace( L'0', L'o' ) == 4 && t.Remove( L'o' ) == 4 && t == L"hell, wrld" );

	// Tokenize skips leading and repeated delimiters and returns empty at the end
	CString sList( L",,a,bc,,d" ), sTok;
	int nPos = 0;
	CString sOut;
	for ( sTok = sList.Tokenize( L",", nPos ); nPos != -1; sTok = sList.Tokenize( L",", nPos ) )
		sOut += sTok + L"|";
	CHECK( sOut == L"a|bc|d|" );

	CString f;
	f.Format( L"%s:%u %S %c %I64u", L"host", 6346u, "nar", L'x', 1ull << 33 );
	CHECK( f == L"host:6346 nar x 8589934592" );
	f.AppendFormat( L"|%.2f|%08X", 1.5, 0xABCDu );
	CHECK( f == L"host:6346 nar x 8589934592|1.50|0000ABCD" );

	// GetBuffer/ReleaseBuffer
	CString b;
	wchar_t* p = b.GetBuffer( 10 );
	wcscpy( p, L"abc" );
	b.ReleaseBuffer();
	CHECK( b.GetLength() == 3 && b == L"abc" );
	p = b.GetBuffer( 10 );
	p[ 3 ] = L'd';
	b.ReleaseBuffer( 4 );
	CHECK( b == L"abcd" );

	// Conversions use the ANSI code page (1252), as ATL does
	CStringA a( "caf\xe9" );
	CStringW w( a );
	CHECK( w.GetLength() == 4 && w[ 3 ] == 0xE9 );
	CStringA a2( w );
	CHECK( a2 == a );
	CHECK( CStringW( CW2A( L"\x20ac", CP_UTF8 ) ) == CStringW( CA2W( "\xe2\x82\xac", CP_ACP ) ) );
	CHECK( CStringW( CA2W( "\xe2\x82\xac", CP_UTF8 ) ) == L"\x20ac" );

	// FormatMessage inserts by number, in any order, with optional types
	CString fm;
	fm.FormatMessage( L"%2 of %1 (%3!d!%%)", L"ten", L"two", 20 );
	CHECK( fm == L"two of ten (20%)" );
	fm.FormatMessage( L"File %1!s! is %2!I64u! bytes%n", L"a.bin", 1ull << 33 );
	CHECK( fm == L"File a.bin is 8589934592 bytes\r\n" );

	CStringA fa;
	fa.Format( "%s=%d %S", "k", -3, L"wide" );
	CHECK( fa == "k=-3 wide" );

	CString sLeft( L"a" ); sLeft = sLeft + L'b' + L"c" + CString( L"d" );
	CHECK( sLeft == L"abcd" && L"x" + sLeft == L"xabcd" );
	CHECK( CString( L"abc" ).SpanIncluding( L"ab" ) == L"ab" && CString( L"abc" ).SpanExcluding( L"c" ) == L"ab" );
	CString sIns( L"ac" ); sIns.Insert( 1, L'b' ); sIns.Delete( 0 );
	CHECK( sIns == L"bc" );
	CHECK( CString( L"xxaxx" ).Trim( L'x' ) == L"a" && CString( L"--a+" ).TrimLeft( L"-+" ) == L"a+" );
}

static void TestCollections()
{
	CList< int > l;
	for ( int i = 0; i < 5; ++i ) l.AddTail( i );
	l.AddHead( -1 );
	CHECK( l.GetCount() == 6 && l.GetHead() == -1 && l.GetTail() == 4 );
	int nSum = 0;
	for ( POSITION pos = l.GetHeadPosition(); pos; )
	{
		POSITION posThis = pos;
		const int v = l.GetNext( pos );
		if ( v == 2 ) l.RemoveAt( posThis );
		else nSum += v;
	}
	CHECK( nSum == 7 && l.GetCount() == 5 && l.Find( 2 ) == NULL && l.Find( 3 ) != NULL );
	l.InsertBefore( l.Find( 3 ), 2 );
	CHECK( l.GetAt( l.FindIndex( 3 ) ) == 2 );
	CHECK( l.RemoveHead() == -1 && l.RemoveTail() == 4 && l.GetCount() == 4 );

	CMap< CString, LPCTSTR, int, int > m;	// StdAfx.h adds the const CString& key hashing
	m.SetAt( L"one", 1 );
	m[ L"two" ] = 2;
	for ( int i = 0; i < 100; ++i ) { CString k; k.Format( L"k%d", i ); m[ k ] = i; }
	int v = 0;
	CHECK( m.Lookup( L"two", v ) && v == 2 && m.GetCount() == 102 );
	CHECK( m.RemoveKey( L"one" ) && ! m.Lookup( L"one", v ) && m.GetCount() == 101 );
	int nCount = 0; CString k; int val;
	for ( POSITION pos = m.GetStartPosition(); pos; ) { m.GetNextAssoc( pos, k, val ); ++nCount; }
	CHECK( nCount == 101 );

	CArray< DWORD, DWORD > arr;
	arr.Add( 1 ); arr.Add( 3 ); arr.InsertAt( 1, 2 ); arr.SetAtGrow( 5, 6 );
	CHECK( arr.GetSize() == 6 && arr[ 1 ] == 2 && arr[ 4 ] == 0 && arr[ 5 ] == 6 );
	arr.RemoveAt( 3, 2 );
	CHECK( arr.GetSize() == 4 && arr[ 3 ] == 6 );
}

static void TestAtlMap()
{
	CAtlMap< CString, int, CStringElementTraitsI< CString > > m;	// case-insensitive keys, as the library uses
	m.SetAt( L"Song.MP3", 1 );
	m[ L"other" ] = 2;
	int v = 0;
	CHECK( m.Lookup( L"song.mp3", v ) && v == 1 && m.GetCount() == 2 );
	m.SetAt( L"SONG.mp3", 3 );	// same key
	CHECK( m.GetCount() == 2 && m.Lookup( L"song.MP3" )->m_value == 3 );
	for ( int i = 0; i < 500; ++i ) { CString k; k.Format( L"k%d", i ); m[ k ] = i; }
	int nSum = 0, nCount = 0;
	for ( POSITION pos = m.GetStartPosition(); pos; )
	{
		CString k; int n;
		m.GetNextAssoc( pos, k, n );
		if ( k.Left( 1 ) == L"k" ) nSum += n;
		++nCount;
	}
	CHECK( nCount == 502 && nSum == 499 * 500 / 2 );
	// Remove while iterating
	for ( POSITION pos = m.GetStartPosition(); pos; )
	{
		POSITION posThis = pos;
		if ( m.GetNext( pos )->m_key.Left( 1 ) == L"k" ) m.RemoveAtPos( posThis );
	}
	CHECK( m.GetCount() == 2 && ! m.Lookup( L"k7", v ) && m.RemoveKey( L"OTHER" ) && m.GetCount() == 1 );
	CAtlMap< DWORD, CString > d;
	d[ 7 ] = L"seven";
	CHECK( d.Lookup( 7 ) && d.Lookup( 7 )->m_value == L"seven" && ! d.Lookup( 8 ) );
}

static void TestExceptionsAndLocks()
{
	bool bCaught = false;
	try { AfxThrowArchiveException( CArchiveException::endOfFile ); }
	catch ( CArchiveException* e ) { bCaught = e->m_cause == CArchiveException::endOfFile; e->Delete(); }
	CHECK( bCaught );
	bCaught = false;
	try { AfxThrowUserException(); }
	catch ( CException* e ) { bCaught = true; e->Delete(); }
	CHECK( bCaught );

	CMutex m;
	CSingleLock l( &m, TRUE );
	CHECK( l.IsLocked() );
	BOOL bOther = TRUE;
	std::thread( [&] { bOther = m.Lock( 30 ); if ( bOther ) m.Unlock(); } ).join();
	CHECK( ! bOther );
	CHECK( m.Lock( 0 ) );	// recursive for the owner
	m.Unlock();
	l.Unlock();
	std::thread( [&] { bOther = m.Lock( 30 ); if ( bOther ) m.Unlock(); } ).join();
	CHECK( bOther );

	CEvent e( FALSE, FALSE );
	CHECK( ! e.Lock( 10 ) );
	std::thread t( [&] { Sleep( 20 ); e.SetEvent(); } );
	CHECK( e.Lock( 2000 ) );
	t.join();
	CHECK( ! e.Lock( 0 ) );	// auto-reset
	CEvent em( FALSE, TRUE );
	em.SetEvent();
	CHECK( em.Lock( 0 ) && em.Lock( 0 ) );	// manual reset stays signalled
}

static UINT CountTo(LPVOID p)
{
	LONG* pn = static_cast< LONG* >( p );
	for ( int i = 0; i < 1000; ++i ) InterlockedIncrement( pn );
	return 7;
}

class CWorker : public CWinThread
{
public:
	CWorker(CEvent* pDone, bool* pDeleted) : m_pDone( pDone ), m_pDeleted( pDeleted ) {}
	~CWorker() { *m_pDeleted = true; }
	BOOL InitInstance() override { return TRUE; }
	int Run() override { m_pDone->SetEvent(); return 3; }
	CEvent* m_pDone;
	bool* m_pDeleted;
};

static void TestThreadsAndWaits()
{
	// WaitForMultipleObjects: any / all, auto-reset consumed by the waiter
	CEvent a( FALSE, FALSE ), b( FALSE, TRUE );
	HANDLE h[ 2 ] = { a, b };
	CHECK( WaitForMultipleObjects( 2, h, FALSE, 10 ) == WAIT_TIMEOUT );
	b.SetEvent();
	CHECK( WaitForMultipleObjects( 2, h, FALSE, 0 ) == WAIT_OBJECT_0 + 1 );
	CHECK( WaitForMultipleObjects( 2, h, TRUE, 10 ) == WAIT_TIMEOUT );
	a.SetEvent();
	CHECK( WaitForMultipleObjects( 2, h, TRUE, 0 ) == WAIT_OBJECT_0 );
	CHECK( WaitForSingleObject( a, 0 ) == WAIT_TIMEOUT );	// consumed by the WaitAll
	CHECK( WaitForSingleObject( b, 0 ) == WAIT_OBJECT_0 );	// manual reset stays set

	// Thread procs, exit codes, waiting on the thread handle
	LONG n = 0;
	CWinThread* pThread = AfxBeginThread( CountTo, &n, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED );
	pThread->m_bAutoDelete = FALSE;
	CHECK( WaitForSingleObject( *pThread, 20 ) == WAIT_TIMEOUT && n == 0 );	// still suspended
	pThread->ResumeThread();
	CHECK( WaitForSingleObject( *pThread, 5000 ) == WAIT_OBJECT_0 && n == 1000 );
	DWORD nExit = 0;
	CHECK( GetExitCodeThread( *pThread, &nExit ) && nExit == 7 );
	HANDLE hStale = pThread->m_hThread;
	delete pThread;
	Sleep( 10 );
	CHECK( WaitForSingleObject( hStale, 0 ) == WAIT_FAILED );	// closed handle is rejected, not dereferenced

	// InitInstance/Run path and auto-delete
	CEvent oDone;
	bool bDeleted = false;
	CWorker* pWorker = new CWorker( &oDone, &bDeleted );
	CHECK( pWorker->CreateThread() );
	CHECK( oDone.Lock( 5000 ) );
	for ( int i = 0; i < 500 && ! bDeleted; ++i ) Sleep( 1 );
	CHECK( bDeleted );

	// Many threads hammering one auto-reset event: each SetEvent releases at most one waiter
	CEvent oGate( FALSE, FALSE );
	LONG nWoken = 0;
	std::vector< std::thread > oWaiters;
	for ( int i = 0; i < 8; ++i )
		oWaiters.emplace_back( [&] { if ( oGate.Lock( 2000 ) ) InterlockedIncrement( &nWoken ); } );
	Sleep( 50 );
	for ( int i = 0; i < 3; ++i ) { oGate.SetEvent(); Sleep( 20 ); }
	for ( auto& t : oWaiters ) t.join();
	CHECK( nWoken == 3 );
}

// ::FormatMessage from a string, as the chat code uses it for "%1" messages
static DWORD FormatFromString(LPWSTR* ppOut, LPCWSTR pszFormat, ...)
{
	va_list a;
	va_start( a, pszFormat );
	const DWORD n = ::FormatMessage( FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		pszFormat, 0, 0, (LPWSTR)ppOut, 0, &a );
	va_end( a );
	return n;
}

static void TestFormatMessage()
{
	LPWSTR p = NULL;
	CHECK( FormatFromString( &p, L"%2 says %1!d!%%", 42, L"Bob" ) == 12 && p && CStringW( p ) == L"Bob says 42%" );
	LocalFree( p );
	p = NULL;
	CHECK( FormatFromString( &p, L"no inserts" ) == 10 && CStringW( p ) == L"no inserts" );
	LocalFree( p );

	WCHAR sz[ 8 ];
	CHECK( ::FormatMessage( FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_IGNORE_INSERTS, L"%1 x", 0, 0, sz, 8, NULL ) == 4 && CStringW( sz ) == L"%1 x" );
	CHECK( ::FormatMessage( FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_IGNORE_INSERTS, L"too long text", 0, 0, sz, 8, NULL ) == 0 &&
		GetLastError() == ERROR_INSUFFICIENT_BUFFER );
	CHECK( ::FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, 5, 0, sz, 8, NULL ) == 0 );	// system messages: not available
}

int main()
{
	TestFormatMessage();
	TestThreadsAndWaits();
	TestCString();
	TestCollections();
	TestAtlMap();
	TestExceptionsAndLocks();
	printf( "afx: %d passed, %d failed\n", g_nPassed, g_nFailed );
	return g_nFailed ? 1 : 0;
}
