//
// compat/cstring.h (Linux compatibility layer)
//
// ATL/MFC CStringT on top of std::basic_string: CStringA, CStringW, CString.
// Behaviour follows the ATL documentation, including the edge cases the
// engine relies on (Mid/Left/Right clamp, Find returns -1, Tokenize skips
// leading delimiters, GetBuffer/ReleaseBuffer).
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include <windows.h>
#include <string>
#include <cwctype>
#include <cctype>

namespace compat {

template< typename Ch > struct CharTraits;

template<> struct CharTraits< char >
{
	typedef wchar_t Other;
	static size_t Length(const char* p) { return p ? strlen( p ) : 0; }
	static int Compare(const char* a, const char* b) { return strcmp( a, b ); }
	static int CompareNoCase(const char* a, const char* b) { return strcasecmp( a, b ); }
	static int Collate(const char* a, const char* b) { return strcoll( a, b ); }
	static char ToLower(char c) { return (char)tolower( (unsigned char)c ); }
	static char ToUpper(char c) { return (char)toupper( (unsigned char)c ); }
	static bool IsSpace(char c) { return isspace( (unsigned char)c ) != 0; }
	static int VFormatLength(const char* f, va_list a) { return _vscprintf( f, a ); }
	static int VFormat(char* b, size_t n, const char* f, va_list a) { return _vsnprintf_win( b, n, f, a ); }
	// ATL converts between CStringA and CStringW with the ANSI code page.
	static std::string Convert(const wchar_t* p, int n)
	{
		if ( ! p || ! n ) return std::string();
		const int nLen = WideCharToMultiByte( CP_ACP, 0, p, n, NULL, 0, NULL, NULL );
		std::string s( (size_t)nLen, '\0' );
		WideCharToMultiByte( CP_ACP, 0, p, n, &s[ 0 ], nLen, NULL, NULL );
		return s;
	}
};

template<> struct CharTraits< wchar_t >
{
	typedef char Other;
	static size_t Length(const wchar_t* p) { return p ? wcslen( p ) : 0; }
	static int Compare(const wchar_t* a, const wchar_t* b) { return wcscmp( a, b ); }
	static int CompareNoCase(const wchar_t* a, const wchar_t* b) { return wcscasecmp( a, b ); }
	static int Collate(const wchar_t* a, const wchar_t* b) { return wcscoll( a, b ); }
	static wchar_t ToLower(wchar_t c) { return (wchar_t)towlower( c ); }
	static wchar_t ToUpper(wchar_t c) { return (wchar_t)towupper( c ); }
	static bool IsSpace(wchar_t c) { return iswspace( c ) != 0; }
	static int VFormatLength(const wchar_t* f, va_list a) { return _vscwprintf( f, a ); }
	static int VFormat(wchar_t* b, size_t n, const wchar_t* f, va_list a) { return _vsnwprintf( b, n, f, a ); }
	static std::wstring Convert(const char* p, int n)
	{
		if ( ! p || ! n ) return std::wstring();
		const int nLen = MultiByteToWideChar( CP_ACP, 0, p, n, NULL, 0 );
		std::wstring s( (size_t)nLen, L'\0' );
		MultiByteToWideChar( CP_ACP, 0, p, n, &s[ 0 ], nLen );
		return s;
	}
};

// Resource strings: the Linux build has no .rc resources, so LoadString
// consults a table the application can fill (see SetResourceString).
const wchar_t* GetResourceString(UINT nID);
void SetResourceString(UINT nID, const wchar_t* pszText);

} // namespace compat

template< typename Ch >
class CStringT
{
	typedef compat::CharTraits< Ch > Traits;
	typedef typename Traits::Other OtherCh;
	typedef std::basic_string< Ch > Storage;

public:
	typedef Ch XCHAR;
	typedef const Ch* PCXSTR;
	typedef Ch* PXSTR;
	typedef OtherCh YCHAR;
	typedef const OtherCh* PCYSTR;

	CStringT() {}
	CStringT(const CStringT& s) : m_s( s.m_s ) {}
	CStringT(CStringT&& s) noexcept : m_s( std::move( s.m_s ) ) {}
	CStringT(const Ch* p) : m_s( p ? p : Empty_() ) {}
	CStringT(const Ch* p, int n) : m_s( p, p ? (size_t)( n < 0 ? 0 : n ) : 0 ) {}
	CStringT(Ch c, int nRepeat = 1) : m_s( (size_t)( nRepeat < 0 ? 0 : nRepeat ), c ) {}
	explicit CStringT(const OtherCh* p) : m_s( Traits::Convert( p, p ? (int)compat::CharTraits< OtherCh >::Length( p ) : 0 ) ) {}
	CStringT(const OtherCh* p, int n) : m_s( Traits::Convert( p, n ) ) {}
	explicit CStringT(const CStringT< OtherCh >& s) : m_s( Traits::Convert( s.GetString(), s.GetLength() ) ) {}
	explicit CStringT(const unsigned char* p) : CStringT( reinterpret_cast< const char* >( p ) ) {}
	explicit CStringT(const Storage& s) : m_s( s ) {}

	CStringT& operator=(const CStringT& s) { m_s = s.m_s; return *this; }
	CStringT& operator=(CStringT&& s) noexcept { m_s = std::move( s.m_s ); return *this; }
	CStringT& operator=(const Ch* p) { m_s = p ? p : Empty_(); return *this; }
	CStringT& operator=(Ch c) { m_s.assign( 1, c ); return *this; }
	CStringT& operator=(const OtherCh* p) { *this = CStringT( p ); return *this; }
	CStringT& operator=(const CStringT< OtherCh >& s) { *this = CStringT( s ); return *this; }

	// --- access
	int GetLength() const { return (int)m_s.size(); }
	bool IsEmpty() const { return m_s.empty(); }
	void Empty() { m_s.clear(); }
	const Ch* GetString() const { return m_s.c_str(); }
	operator const Ch*() const { return m_s.c_str(); }
	Ch GetAt(int i) const { return m_s[ (size_t)i ]; }
	Ch operator[](int i) const { return m_s[ (size_t)i ]; }
	void SetAt(int i, Ch c) { m_s[ (size_t)i ] = c; }
	const Storage& std_str() const { return m_s; }

	// --- buffer
	Ch* GetBuffer() { return &m_s[ 0 ]; }
	Ch* GetBuffer(int nMinLength)
	{
		if ( nMinLength > (int)m_s.size() ) m_s.resize( (size_t)nMinLength );
		return &m_s[ 0 ];
	}
	Ch* GetBufferSetLength(int nLength) { m_s.resize( (size_t)( nLength < 0 ? 0 : nLength ) ); return &m_s[ 0 ]; }
	void ReleaseBuffer(int nNewLength = -1)
	{
		if ( nNewLength < 0 )
			nNewLength = (int)Traits::Length( m_s.c_str() );
		m_s.resize( (size_t)nNewLength );
	}
	void ReleaseBufferSetLength(int nNewLength) { ReleaseBuffer( nNewLength ); }
	void Truncate(int nNewLength) { if ( nNewLength < (int)m_s.size() ) m_s.resize( (size_t)( nNewLength < 0 ? 0 : nNewLength ) ); }
	Ch* LockBuffer() { return &m_s[ 0 ]; }
	void UnlockBuffer() {}
	void Preallocate(int n) { m_s.reserve( (size_t)n ); }
	void FreeExtra() { m_s.shrink_to_fit(); }

	// --- concatenation
	CStringT& operator+=(const CStringT& s) { m_s += s.m_s; return *this; }
	CStringT& operator+=(const Ch* p) { if ( p ) m_s += p; return *this; }
	CStringT& operator+=(Ch c) { m_s += c; return *this; }
	CStringT& operator+=(const OtherCh* p) { return *this += CStringT( p ); }
	void Append(const CStringT& s) { m_s += s.m_s; }
	void Append(const Ch* p) { if ( p ) m_s += p; }
	void Append(const Ch* p, int n) { if ( p && n > 0 ) m_s.append( p, (size_t)n ); }
	void AppendChar(Ch c) { m_s += c; }

	// --- comparison
	int Compare(const Ch* p) const { return Traits::Compare( m_s.c_str(), p ? p : Empty_() ); }
	int CompareNoCase(const Ch* p) const { return Traits::CompareNoCase( m_s.c_str(), p ? p : Empty_() ); }
	int Collate(const Ch* p) const { return Traits::Collate( m_s.c_str(), p ? p : Empty_() ); }
	int CollateNoCase(const Ch* p) const
	{
		CStringT a( *this ), b( p ); a.MakeLower(); b.MakeLower();
		return a.Collate( b );
	}

	// --- searching
	int Find(Ch c, int nStart = 0) const
	{
		if ( nStart < 0 || nStart > (int)m_s.size() ) return -1;
		const size_t n = m_s.find( c, (size_t)nStart );
		return n == Storage::npos ? -1 : (int)n;
	}
	int Find(const Ch* p, int nStart = 0) const
	{
		if ( ! p || nStart < 0 || nStart > (int)m_s.size() ) return -1;
		const size_t n = m_s.find( p, (size_t)nStart );
		return n == Storage::npos ? -1 : (int)n;
	}
	int ReverseFind(Ch c) const
	{
		const size_t n = m_s.rfind( c );
		return n == Storage::npos ? -1 : (int)n;
	}
	int FindOneOf(const Ch* pSet) const
	{
		if ( ! pSet ) return -1;
		const size_t n = m_s.find_first_of( pSet );
		return n == Storage::npos ? -1 : (int)n;
	}

	// --- extraction
	CStringT Mid(int nFirst) const { return Mid( nFirst, GetLength() - ( nFirst < 0 ? 0 : nFirst ) ); }
	CStringT Mid(int nFirst, int nCount) const
	{
		if ( nFirst < 0 ) nFirst = 0;
		if ( nCount < 0 ) nCount = 0;
		if ( nFirst >= GetLength() ) return CStringT();
		if ( nCount > GetLength() - nFirst ) nCount = GetLength() - nFirst;
		return CStringT( m_s.substr( (size_t)nFirst, (size_t)nCount ) );
	}
	CStringT Left(int nCount) const
	{
		if ( nCount < 0 ) nCount = 0;
		return CStringT( m_s.substr( 0, (size_t)nCount ) );
	}
	CStringT Right(int nCount) const
	{
		if ( nCount < 0 ) nCount = 0;
		if ( nCount >= GetLength() ) return *this;
		return CStringT( m_s.substr( m_s.size() - (size_t)nCount ) );
	}
	CStringT SpanIncluding(const Ch* pSet) const
	{
		const size_t n = pSet ? m_s.find_first_not_of( pSet ) : 0;
		return Left( n == Storage::npos ? GetLength() : (int)n );
	}
	CStringT SpanExcluding(const Ch* pSet) const
	{
		const size_t n = pSet ? m_s.find_first_of( pSet ) : Storage::npos;
		return Left( n == Storage::npos ? GetLength() : (int)n );
	}
	CStringT Tokenize(const Ch* pSet, int& nStart) const
	{
		if ( nStart < 0 || nStart >= GetLength() )
		{
			nStart = -1;
			return CStringT();
		}
		if ( ! pSet || ! *pSet )
		{
			const int n = nStart;
			nStart = GetLength();
			return Mid( n );
		}
		const size_t nBegin = m_s.find_first_not_of( pSet, (size_t)nStart );
		if ( nBegin == Storage::npos )
		{
			nStart = -1;
			return CStringT();
		}
		size_t nEnd = m_s.find_first_of( pSet, nBegin );
		if ( nEnd == Storage::npos ) nEnd = m_s.size();
		nStart = (int)nEnd + 1;
		return CStringT( m_s.substr( nBegin, nEnd - nBegin ) );
	}

	// --- modification
	CStringT& MakeLower() { for ( Ch& c : m_s ) c = Traits::ToLower( c ); return *this; }
	CStringT& MakeUpper() { for ( Ch& c : m_s ) c = Traits::ToUpper( c ); return *this; }
	CStringT& MakeReverse() { std::reverse( m_s.begin(), m_s.end() ); return *this; }
	CStringT& TrimLeft()
	{
		size_t n = 0;
		while ( n < m_s.size() && Traits::IsSpace( m_s[ n ] ) ) ++n;
		m_s.erase( 0, n );
		return *this;
	}
	CStringT& TrimRight()
	{
		size_t n = m_s.size();
		while ( n && Traits::IsSpace( m_s[ n - 1 ] ) ) --n;
		m_s.resize( n );
		return *this;
	}
	CStringT& Trim() { return TrimRight().TrimLeft(); }
	CStringT& TrimLeft(Ch c) { size_t n = 0; while ( n < m_s.size() && m_s[ n ] == c ) ++n; m_s.erase( 0, n ); return *this; }
	CStringT& TrimRight(Ch c) { size_t n = m_s.size(); while ( n && m_s[ n - 1 ] == c ) --n; m_s.resize( n ); return *this; }
	CStringT& Trim(Ch c) { return TrimRight( c ).TrimLeft( c ); }
	CStringT& TrimLeft(const Ch* pSet)
	{
		if ( ! pSet ) return *this;
		const size_t n = m_s.find_first_not_of( pSet );
		m_s.erase( 0, n == Storage::npos ? m_s.size() : n );
		return *this;
	}
	CStringT& TrimRight(const Ch* pSet)
	{
		if ( ! pSet ) return *this;
		const size_t n = m_s.find_last_not_of( pSet );
		m_s.resize( n == Storage::npos ? 0 : n + 1 );
		return *this;
	}
	CStringT& Trim(const Ch* pSet) { return TrimRight( pSet ).TrimLeft( pSet ); }

	int Replace(Ch cOld, Ch cNew)
	{
		int n = 0;
		for ( Ch& c : m_s ) if ( c == cOld ) { c = cNew; ++n; }
		return n;
	}
	int Replace(const Ch* pOld, const Ch* pNew)
	{
		const size_t nOld = Traits::Length( pOld );
		if ( ! nOld ) return 0;
		const Storage sNew( pNew ? pNew : Empty_() );
		int n = 0;
		for ( size_t i = m_s.find( pOld ); i != Storage::npos; i = m_s.find( pOld, i + sNew.size() ) )
		{
			m_s.replace( i, nOld, sNew );
			++n;
		}
		return n;
	}
	int Remove(Ch c)
	{
		const size_t n = m_s.size();
		m_s.erase( std::remove( m_s.begin(), m_s.end(), c ), m_s.end() );
		return (int)( n - m_s.size() );
	}
	int Insert(int i, Ch c)
	{
		if ( i < 0 ) i = 0;
		if ( i > GetLength() ) i = GetLength();
		m_s.insert( (size_t)i, 1, c );
		return GetLength();
	}
	int Insert(int i, const Ch* p)
	{
		if ( i < 0 ) i = 0;
		if ( i > GetLength() ) i = GetLength();
		if ( p ) m_s.insert( (size_t)i, p );
		return GetLength();
	}
	int Delete(int i, int nCount = 1)
	{
		if ( i < 0 ) i = 0;
		if ( i < GetLength() && nCount > 0 )
			m_s.erase( (size_t)i, (size_t)nCount );
		return GetLength();
	}

	// --- formatting (Windows printf semantics, see windows.h)
	void FormatV(const Ch* pszFormat, va_list args)
	{
		va_list c;
		va_copy( c, args );
		const int n = Traits::VFormatLength( pszFormat, c );
		va_end( c );
		if ( n < 0 ) { m_s.clear(); return; }
		Storage s( (size_t)n + 1, Ch() );
		Traits::VFormat( &s[ 0 ], s.size(), pszFormat, args );
		s.resize( (size_t)n );
		m_s.swap( s );
	}
	void Format(const Ch* pszFormat, ...)
	{
		va_list a; va_start( a, pszFormat ); FormatV( pszFormat, a ); va_end( a );
	}
	void AppendFormatV(const Ch* pszFormat, va_list args)
	{
		CStringT s; s.FormatV( pszFormat, args ); m_s += s.m_s;
	}
	void AppendFormat(const Ch* pszFormat, ...)
	{
		va_list a; va_start( a, pszFormat ); AppendFormatV( pszFormat, a ); va_end( a );
	}
	void Format(UINT nFormatID, ...)
	{
		CStringT f; f.LoadString( nFormatID );
		va_list a; va_start( a, nFormatID ); FormatV( f, a ); va_end( a );
	}

	BOOL LoadString(UINT nID)
	{
		const wchar_t* p = compat::GetResourceString( nID );
		if ( ! p ) { m_s.clear(); return FALSE; }
		*this = CStringT( CStringT< wchar_t >( p ) );
		return TRUE;
	}
	BOOL LoadString(HINSTANCE, UINT nID) { return LoadString( nID ); }

	static int StringLength(const Ch* p) { return (int)Traits::Length( p ); }

	// --- operators
	friend CStringT operator+(const CStringT& a, const CStringT& b) { CStringT r( a ); r += b; return r; }
	friend CStringT operator+(const CStringT& a, const Ch* b) { CStringT r( a ); r += b; return r; }
	friend CStringT operator+(const Ch* a, const CStringT& b) { CStringT r( a ); r += b; return r; }
	friend CStringT operator+(const CStringT& a, Ch b) { CStringT r( a ); r += b; return r; }
	friend CStringT operator+(Ch a, const CStringT& b) { CStringT r( a ); r += b; return r; }
	friend bool operator==(const CStringT& a, const CStringT& b) { return a.m_s == b.m_s; }
	friend bool operator==(const CStringT& a, const Ch* b) { return a.Compare( b ) == 0; }
	friend bool operator==(const Ch* a, const CStringT& b) { return b.Compare( a ) == 0; }
	friend bool operator==(const CStringT& a, Ch b) { return a.m_s.size() == 1 && a.m_s[ 0 ] == b; }
	friend bool operator!=(const CStringT& a, const CStringT& b) { return a.m_s != b.m_s; }
	friend bool operator!=(const CStringT& a, const Ch* b) { return a.Compare( b ) != 0; }
	friend bool operator!=(const Ch* a, const CStringT& b) { return b.Compare( a ) != 0; }
	friend bool operator!=(const CStringT& a, Ch b) { return ! ( a == b ); }
	friend bool operator<(const CStringT& a, const CStringT& b) { return a.m_s < b.m_s; }
	friend bool operator<(const CStringT& a, const Ch* b) { return a.Compare( b ) < 0; }
	friend bool operator>(const CStringT& a, const CStringT& b) { return a.m_s > b.m_s; }
	friend bool operator<=(const CStringT& a, const CStringT& b) { return a.m_s <= b.m_s; }
	friend bool operator>=(const CStringT& a, const CStringT& b) { return a.m_s >= b.m_s; }

private:
	Storage m_s;
	static const Ch* Empty_() { static const Ch z = Ch(); return &z; }
};

typedef CStringT< char > CStringA;
typedef CStringT< wchar_t > CStringW;
typedef CStringW CString;
typedef CStringA CAtlStringA;
typedef CStringW CAtlStringW;
typedef CStringW CAtlString;

namespace std {
	template< typename Ch > struct hash< CStringT< Ch > >
	{
		size_t operator()(const CStringT< Ch >& s) const { return std::hash< std::basic_string< Ch > >()( s.std_str() ); }
	};
}

// ATL conversion helpers (CT2A, CA2W, ...): the code page argument is honoured.
template< typename To, typename From >
class CConvertString
{
public:
	explicit CConvertString(const From* p, UINT nCodePage = CP_ACP) { Init( p, nCodePage ); }
	operator const To*() const { return m_s.c_str(); }
	const To* c_str() const { return m_s.c_str(); }
private:
	std::basic_string< To > m_s;
	void Init(const char* p, UINT cp)
	{
		if ( ! p ) return;
		const int n = MultiByteToWideChar( cp, 0, p, (int)strlen( p ), NULL, 0 );
		m_s.resize( (size_t)n );
		if ( n ) MultiByteToWideChar( cp, 0, p, (int)strlen( p ), &m_s[ 0 ], n );
	}
	void Init(const wchar_t* p, UINT cp)
	{
		if ( ! p ) return;
		const int n = WideCharToMultiByte( cp, 0, p, (int)wcslen( p ), NULL, 0, NULL, NULL );
		m_s.resize( (size_t)n );
		if ( n ) WideCharToMultiByte( cp, 0, p, (int)wcslen( p ), &m_s[ 0 ], n, NULL, NULL );
	}
};
template< typename T > class CPassString
{
public:
	explicit CPassString(const T* p, UINT = CP_ACP) : m_p( p ) {}
	operator const T*() const { return m_p; }
private:
	const T* m_p;
};
typedef CConvertString< char, wchar_t > CW2A;
typedef CConvertString< wchar_t, char > CA2W;
typedef CConvertString< char, wchar_t > CT2A;
typedef CConvertString< wchar_t, char > CA2T;
typedef CConvertString< char, wchar_t > CW2CA;
typedef CConvertString< wchar_t, char > CA2CW;
typedef CConvertString< char, wchar_t > CT2CA;
typedef CConvertString< wchar_t, char > CA2CT;
typedef CPassString< wchar_t > CW2T;
typedef CPassString< wchar_t > CT2W;
typedef CPassString< wchar_t > CW2CT;
typedef CPassString< wchar_t > CT2CW;
