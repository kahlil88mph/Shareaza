//
// ole.cpp (Linux compatibility layer): BSTR, VARIANT, SAFEARRAY, GUIDs and
// CCmdTarget interface maps.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include <compat/ole.h>
#include <tchar.h>

#include <cerrno>
#include <cmath>
#include <sys/random.h>

const IID IID_NULL = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
const IID IID_IDispatch = { 0x00020400, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
const IID IID_IEnumVARIANT = { 0x00020404, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
const IID IID_IClassFactory = { 0x00000001, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

// ---------------------------------------------------------------------------
// BSTR: a length prefix (in bytes, as on Windows) in front of the characters,
// always NUL-terminated.

namespace {
	struct BstrHeader { uint64_t nPad; uint32_t nPad2; uint32_t nBytes; };	// keeps characters 16-byte aligned
	inline BstrHeader* HeaderOf(BSTR s) { return reinterpret_cast< BstrHeader* >( s ) - 1; }
}

BSTR SysAllocStringLen(const OLECHAR* psz, UINT nLen)
{
	BstrHeader* p = static_cast< BstrHeader* >( malloc( sizeof( BstrHeader ) + ( (size_t)nLen + 1 ) * sizeof( OLECHAR ) ) );
	if ( ! p ) return NULL;
	p->nBytes = nLen * (uint32_t)sizeof( OLECHAR );
	BSTR s = reinterpret_cast< BSTR >( p + 1 );
	if ( psz ) wmemcpy( s, psz, nLen );
	else wmemset( s, 0, nLen );
	s[ nLen ] = 0;
	return s;
}

BSTR SysAllocString(const OLECHAR* psz)
{
	return psz ? SysAllocStringLen( psz, (UINT)wcslen( psz ) ) : NULL;
}

BSTR SysAllocStringByteLen(LPCSTR psz, UINT nLen)
{
	const UINT nChars = ( nLen + sizeof( OLECHAR ) - 1 ) / sizeof( OLECHAR );
	BSTR s = SysAllocStringLen( NULL, nChars );
	if ( s && psz ) memcpy( s, psz, nLen );
	if ( s ) HeaderOf( s )->nBytes = nLen;
	return s;
}

INT SysReAllocString(BSTR* pbstr, const OLECHAR* psz)
{
	BSTR s = SysAllocString( psz );
	if ( psz && ! s ) return FALSE;
	SysFreeString( *pbstr );
	*pbstr = s;
	return TRUE;
}

void SysFreeString(BSTR bstr)
{
	if ( bstr ) free( HeaderOf( bstr ) );
}

UINT SysStringLen(BSTR bstr) { return bstr ? HeaderOf( bstr )->nBytes / (UINT)sizeof( OLECHAR ) : 0; }
UINT SysStringByteLen(BSTR bstr) { return bstr ? HeaderOf( bstr )->nBytes : 0; }

// ---------------------------------------------------------------------------
// VARIANT

void VariantInit(VARIANT* pv)
{
	memset( pv, 0, sizeof( VARIANT ) );
	pv->vt = VT_EMPTY;
}

HRESULT VariantClear(VARIANT* pv)
{
	if ( ! pv ) return E_INVALIDARG;
	if ( ! ( pv->vt & VT_BYREF ) )
	{
		switch ( pv->vt & ~VT_ARRAY )
		{
		case VT_BSTR:
			if ( ! ( pv->vt & VT_ARRAY ) ) SysFreeString( pv->bstrVal );
			break;
		case VT_DISPATCH:
		case VT_UNKNOWN:
			if ( ! ( pv->vt & VT_ARRAY ) && pv->punkVal ) pv->punkVal->Release();
			break;
		default:
			break;
		}
		if ( pv->vt & VT_ARRAY ) SafeArrayDestroy( pv->parray );
	}
	VariantInit( pv );
	return S_OK;
}

HRESULT VariantCopy(VARIANT* pDest, const VARIANT* pSrc)
{
	if ( ! pDest || ! pSrc ) return E_INVALIDARG;
	if ( pDest == pSrc ) return S_OK;
	VariantClear( pDest );
	*pDest = *pSrc;
	if ( pSrc->vt & ( VT_BYREF | VT_ARRAY ) )
	{
		if ( pSrc->vt & VT_ARRAY )
		{
			VariantInit( pDest );
			return E_NOTIMPL;	// the engine never copies array variants
		}
		return S_OK;
	}
	if ( pSrc->vt == VT_BSTR )
		pDest->bstrVal = pSrc->bstrVal ? SysAllocStringLen( pSrc->bstrVal, SysStringLen( pSrc->bstrVal ) ) : NULL;
	else if ( ( pSrc->vt == VT_DISPATCH || pSrc->vt == VT_UNKNOWN ) && pSrc->punkVal )
		pSrc->punkVal->AddRef();
	return S_OK;
}

namespace {
	bool ToInt64(const VARIANT& v, LONGLONG& n)
	{
		switch ( v.vt )
		{
		case VT_EMPTY: n = 0; return true;
		case VT_I1: n = v.cVal; return true;
		case VT_UI1: n = v.bVal; return true;
		case VT_I2: n = v.iVal; return true;
		case VT_UI2: n = v.uiVal; return true;
		case VT_I4: case VT_INT: n = v.lVal; return true;
		case VT_UI4: case VT_UINT: n = v.ulVal; return true;
		case VT_I8: n = v.llVal; return true;
		case VT_UI8: n = (LONGLONG)v.ullVal; return true;
		case VT_BOOL: n = v.boolVal ? -1 : 0; return true;
		case VT_R4: n = (LONGLONG)llround( v.fltVal ); return true;
		case VT_R8: n = (LONGLONG)llround( v.dblVal ); return true;
		case VT_BSTR:
		{
			if ( ! v.bstrVal ) return false;
			wchar_t* pEnd = NULL;
			errno = 0;
			n = wcstoll( v.bstrVal, &pEnd, 10 );
			while ( pEnd && iswspace( *pEnd ) ) ++pEnd;
			return errno == 0 && pEnd != v.bstrVal && *pEnd == 0;
		}
		default: return false;
		}
	}

	bool ToDouble(const VARIANT& v, double& d)
	{
		if ( v.vt == VT_R8 ) { d = v.dblVal; return true; }
		if ( v.vt == VT_R4 ) { d = v.fltVal; return true; }
		if ( v.vt == VT_BSTR )
		{
			if ( ! v.bstrVal ) return false;
			wchar_t* pEnd = NULL;
			d = wcstod( v.bstrVal, &pEnd );
			return pEnd != v.bstrVal && *pEnd == 0;
		}
		LONGLONG n;
		if ( ! ToInt64( v, n ) ) return false;
		d = (double)n;
		return true;
	}
}

HRESULT VariantChangeType(VARIANT* pDest, const VARIANT* pSrc, USHORT, VARTYPE vt)
{
	if ( ! pDest || ! pSrc ) return E_INVALIDARG;
	VARIANT r;
	VariantInit( &r );
	if ( pSrc->vt == vt )
	{
		HRESULT hr = VariantCopy( &r, pSrc );
		if ( FAILED( hr ) ) return hr;
	}
	else if ( vt == VT_BSTR )
	{
		CStringW s;
		double d;
		LONGLONG n;
		if ( pSrc->vt == VT_BOOL ) s = pSrc->boolVal ? L"True" : L"False";
		else if ( pSrc->vt == VT_R4 || pSrc->vt == VT_R8 ) { ToDouble( *pSrc, d ); s.Format( L"%.15g", d ); }
		else if ( pSrc->vt == VT_UI8 ) s.Format( L"%I64u", pSrc->ullVal );
		else if ( ToInt64( *pSrc, n ) ) s.Format( L"%I64d", n );
		else return DISP_E_TYPEMISMATCH;
		r.vt = VT_BSTR;
		r.bstrVal = SysAllocStringLen( s, (UINT)s.GetLength() );
	}
	else if ( vt == VT_R8 || vt == VT_R4 )
	{
		double d;
		if ( ! ToDouble( *pSrc, d ) ) return DISP_E_TYPEMISMATCH;
		r.vt = vt;
		if ( vt == VT_R8 ) r.dblVal = d; else r.fltVal = (FLOAT)d;
	}
	else if ( vt == VT_BOOL )
	{
		LONGLONG n = 0;
		if ( pSrc->vt == VT_BSTR && pSrc->bstrVal && ( _wcsicmp( pSrc->bstrVal, L"true" ) == 0 || _wcsicmp( pSrc->bstrVal, L"false" ) == 0 ) )
			n = _wcsicmp( pSrc->bstrVal, L"true" ) == 0;
		else if ( ! ToInt64( *pSrc, n ) ) return DISP_E_TYPEMISMATCH;
		r.vt = VT_BOOL;
		r.boolVal = n ? VARIANT_TRUE : VARIANT_FALSE;
	}
	else
	{
		LONGLONG n;
		if ( ! ToInt64( *pSrc, n ) ) return DISP_E_TYPEMISMATCH;
		r.vt = vt;
		switch ( vt )
		{
		case VT_I1: if ( n < -128 || n > 127 ) return DISP_E_OVERFLOW; r.cVal = (CHAR)n; break;
		case VT_UI1: if ( n < 0 || n > 255 ) return DISP_E_OVERFLOW; r.bVal = (BYTE)n; break;
		case VT_I2: if ( n < -32768 || n > 32767 ) return DISP_E_OVERFLOW; r.iVal = (SHORT)n; break;
		case VT_UI2: if ( n < 0 || n > 65535 ) return DISP_E_OVERFLOW; r.uiVal = (USHORT)n; break;
		case VT_I4: case VT_INT: if ( n < INT32_MIN || n > INT32_MAX ) return DISP_E_OVERFLOW; r.lVal = (LONG)n; break;
		case VT_UI4: case VT_UINT: if ( n < 0 || n > (LONGLONG)UINT32_MAX ) return DISP_E_OVERFLOW; r.ulVal = (ULONG)n; break;
		case VT_I8: r.llVal = n; break;
		case VT_UI8: if ( n < 0 ) return DISP_E_OVERFLOW; r.ullVal = (ULONGLONG)n; break;
		default: return DISP_E_BADVARTYPE;
		}
	}
	VariantClear( pDest );
	*pDest = r;
	return S_OK;
}

// ---------------------------------------------------------------------------
// SAFEARRAY (one or more dimensions of fixed-size elements; BSTR and VARIANT
// elements are owned by the array)

namespace {
	enum { FADF_BSTR = 0x0100, FADF_VARIANT = 0x0800, FADF_UNKNOWN = 0x0200 };

	size_t ElementCount(const SAFEARRAY* psa)
	{
		size_t n = 1;
		for ( USHORT i = 0; i < psa->cDims; ++i ) n *= psa->rgsabound[ i ].cElements;
		return n;
	}

	// rgIndices[0] is the leftmost dimension, which is stored last (rgsabound
	// is in reverse order, as on Windows).
	bool Offset(const SAFEARRAY* psa, const LONG* rgIndices, size_t& nOffset)
	{
		size_t nIndex = 0;
		for ( USHORT d = 0; d < psa->cDims; ++d )
		{
			const SAFEARRAYBOUND& b = psa->rgsabound[ psa->cDims - 1 - d ];
			const LONG i = rgIndices[ d ] - b.lLbound;
			if ( i < 0 || (ULONG)i >= b.cElements ) return false;
			nIndex = nIndex * b.cElements + (size_t)i;
		}
		nOffset = nIndex * psa->cbElements;
		return true;
	}

	ULONG ElementSize(VARTYPE vt)
	{
		switch ( vt )
		{
		case VT_I1: case VT_UI1: return 1;
		case VT_I2: case VT_UI2: case VT_BOOL: return 2;
		case VT_I4: case VT_UI4: case VT_INT: case VT_UINT: case VT_R4: case VT_ERROR: return 4;
		case VT_I8: case VT_UI8: case VT_R8: case VT_DATE: case VT_CY: return 8;
		case VT_BSTR: case VT_DISPATCH: case VT_UNKNOWN: return sizeof( void* );
		case VT_VARIANT: return sizeof( VARIANT );
		default: return 0;
		}
	}
}

HRESULT SafeArrayAllocDescriptor(UINT cDims, SAFEARRAY** ppsaOut)
{
	if ( ! ppsaOut || cDims == 0 ) return E_INVALIDARG;
	SAFEARRAY* p = static_cast< SAFEARRAY* >( calloc( 1, sizeof( SAFEARRAY ) + ( cDims - 1 ) * sizeof( SAFEARRAYBOUND ) ) );
	if ( ! p ) return E_OUTOFMEMORY;
	p->cDims = (USHORT)cDims;
	*ppsaOut = p;
	return S_OK;
}

HRESULT SafeArrayAllocData(SAFEARRAY* psa)
{
	if ( ! psa || ! psa->cbElements ) return E_INVALIDARG;
	psa->pvData = calloc( ElementCount( psa ) ? ElementCount( psa ) : 1, psa->cbElements );
	return psa->pvData ? S_OK : E_OUTOFMEMORY;
}

SAFEARRAY* SafeArrayCreate(VARTYPE vt, UINT cDims, SAFEARRAYBOUND* rgsabound)
{
	const ULONG nSize = ElementSize( vt );
	SAFEARRAY* p = NULL;
	if ( ! nSize || FAILED( SafeArrayAllocDescriptor( cDims, &p ) ) ) return NULL;
	p->cbElements = nSize;
	p->fFeatures = vt == VT_BSTR ? FADF_BSTR : vt == VT_VARIANT ? FADF_VARIANT : ( vt == VT_UNKNOWN || vt == VT_DISPATCH ) ? FADF_UNKNOWN : 0;
	for ( UINT i = 0; i < cDims; ++i ) p->rgsabound[ cDims - 1 - i ] = rgsabound[ i ];
	if ( FAILED( SafeArrayAllocData( p ) ) ) { free( p ); return NULL; }
	return p;
}

SAFEARRAY* SafeArrayCreateVector(VARTYPE vt, LONG lLbound, ULONG cElements)
{
	SAFEARRAYBOUND b = { cElements, lLbound };
	return SafeArrayCreate( vt, 1, &b );
}

HRESULT SafeArrayDestroyData(SAFEARRAY* psa)
{
	if ( ! psa ) return E_INVALIDARG;
	if ( psa->cLocks ) return (HRESULT)0x8002000D;	// DISP_E_ARRAYISLOCKED
	if ( psa->pvData )
	{
		const size_t n = ElementCount( psa );
		if ( psa->fFeatures & FADF_BSTR )
			for ( size_t i = 0; i < n; ++i ) SysFreeString( static_cast< BSTR* >( psa->pvData )[ i ] );
		else if ( psa->fFeatures & FADF_VARIANT )
			for ( size_t i = 0; i < n; ++i ) VariantClear( &static_cast< VARIANT* >( psa->pvData )[ i ] );
		else if ( psa->fFeatures & FADF_UNKNOWN )
			for ( size_t i = 0; i < n; ++i ) if ( IUnknown* u = static_cast< IUnknown** >( psa->pvData )[ i ] ) u->Release();
		free( psa->pvData );
		psa->pvData = NULL;
	}
	return S_OK;
}

HRESULT SafeArrayDestroyDescriptor(SAFEARRAY* psa)
{
	if ( ! psa ) return E_INVALIDARG;
	free( psa );
	return S_OK;
}

HRESULT SafeArrayDestroy(SAFEARRAY* psa)
{
	if ( ! psa ) return S_OK;
	const HRESULT hr = SafeArrayDestroyData( psa );
	if ( FAILED( hr ) ) return hr;
	return SafeArrayDestroyDescriptor( psa );
}

HRESULT SafeArrayAccessData(SAFEARRAY* psa, void** ppvData)
{
	if ( ! psa || ! ppvData ) return E_INVALIDARG;
	++psa->cLocks;
	*ppvData = psa->pvData;
	return S_OK;
}

HRESULT SafeArrayUnaccessData(SAFEARRAY* psa)
{
	if ( ! psa || ! psa->cLocks ) return E_UNEXPECTED;
	--psa->cLocks;
	return S_OK;
}

HRESULT SafeArrayGetLBound(SAFEARRAY* psa, UINT nDim, LONG* pl)
{
	if ( ! psa || ! pl || nDim < 1 || nDim > psa->cDims ) return DISP_E_BADINDEX;
	*pl = psa->rgsabound[ psa->cDims - nDim ].lLbound;
	return S_OK;
}

HRESULT SafeArrayGetUBound(SAFEARRAY* psa, UINT nDim, LONG* pl)
{
	if ( ! psa || ! pl || nDim < 1 || nDim > psa->cDims ) return DISP_E_BADINDEX;
	const SAFEARRAYBOUND& b = psa->rgsabound[ psa->cDims - nDim ];
	*pl = b.lLbound + (LONG)b.cElements - 1;
	return S_OK;
}

HRESULT SafeArrayGetElement(SAFEARRAY* psa, LONG* rgIndices, void* pv)
{
	size_t nOffset;
	if ( ! psa || ! pv || ! Offset( psa, rgIndices, nOffset ) ) return DISP_E_BADINDEX;
	const BYTE* p = static_cast< const BYTE* >( psa->pvData ) + nOffset;
	if ( psa->fFeatures & FADF_BSTR )
	{
		BSTR s = *reinterpret_cast< const BSTR* >( p );
		*static_cast< BSTR* >( pv ) = s ? SysAllocStringLen( s, SysStringLen( s ) ) : NULL;
	}
	else if ( psa->fFeatures & FADF_VARIANT )
	{
		VariantInit( static_cast< VARIANT* >( pv ) );
		return VariantCopy( static_cast< VARIANT* >( pv ), reinterpret_cast< const VARIANT* >( p ) );
	}
	else
	{
		memcpy( pv, p, psa->cbElements );
		if ( psa->fFeatures & FADF_UNKNOWN )
			if ( IUnknown* u = *static_cast< IUnknown** >( pv ) ) u->AddRef();
	}
	return S_OK;
}

HRESULT SafeArrayPutElement(SAFEARRAY* psa, LONG* rgIndices, void* pv)
{
	size_t nOffset;
	if ( ! psa || ! Offset( psa, rgIndices, nOffset ) ) return DISP_E_BADINDEX;
	BYTE* p = static_cast< BYTE* >( psa->pvData ) + nOffset;
	if ( psa->fFeatures & FADF_BSTR )
	{
		BSTR& s = *reinterpret_cast< BSTR* >( p );
		SysFreeString( s );
		BSTR src = static_cast< BSTR >( pv );
		s = src ? SysAllocStringLen( src, SysStringLen( src ) ) : NULL;
	}
	else if ( psa->fFeatures & FADF_VARIANT )
		return VariantCopy( reinterpret_cast< VARIANT* >( p ), static_cast< const VARIANT* >( pv ) );
	else if ( psa->fFeatures & FADF_UNKNOWN )
	{
		IUnknown*& u = *reinterpret_cast< IUnknown** >( p );
		IUnknown* n = static_cast< IUnknown* >( pv );
		if ( n ) n->AddRef();
		if ( u ) u->Release();
		u = n;
	}
	else
		memcpy( p, pv, psa->cbElements );
	return S_OK;
}

// ---------------------------------------------------------------------------
// GUIDs and task memory

HRESULT CoCreateGuid(GUID* pguid)
{
	if ( ! pguid ) return E_INVALIDARG;
	BYTE* b = reinterpret_cast< BYTE* >( pguid );
	for ( size_t n = 0; n < sizeof( GUID ); )
	{
		const ssize_t r = getrandom( b + n, sizeof( GUID ) - n, 0 );
		if ( r < 0 ) { if ( errno == EINTR ) continue; return E_FAIL; }
		n += (size_t)r;
	}
	// Version 4, RFC 4122 variant, as CoCreateGuid produces.
	pguid->Data3 = (WORD)( ( pguid->Data3 & 0x0FFF ) | 0x4000 );
	pguid->Data4[ 0 ] = (BYTE)( ( pguid->Data4[ 0 ] & 0x3F ) | 0x80 );
	return S_OK;
}

void* CoTaskMemAlloc(SIZE_T cb) { return malloc( cb ? cb : 1 ); }
void CoTaskMemFree(void* pv) { free( pv ); }

HRESULT CoCreateInstance(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID* ppv)
{
	if ( ppv ) *ppv = NULL;
	return REGDB_E_CLASSNOTREG;
}

HRESULT StringFromGUID2(REFGUID g, LPOLESTR lpsz, int cchMax)
{
	if ( ! lpsz || cchMax < 39 ) return E_INVALIDARG;
	_snwprintf( lpsz, (size_t)cchMax, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		g.Data1, g.Data2, g.Data3, g.Data4[ 0 ], g.Data4[ 1 ], g.Data4[ 2 ], g.Data4[ 3 ],
		g.Data4[ 4 ], g.Data4[ 5 ], g.Data4[ 6 ], g.Data4[ 7 ] );
	return 39;	// characters written, including the NUL
}

HRESULT CLSIDFromString(LPCOLESTR p, CLSID* pclsid)
{
	if ( ! p || ! pclsid ) return E_INVALIDARG;
	unsigned int d1, d2, d3, b[ 8 ];
	if ( swscanf( p, L"{%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x}", &d1, &d2, &d3,
			&b[ 0 ], &b[ 1 ], &b[ 2 ], &b[ 3 ], &b[ 4 ], &b[ 5 ], &b[ 6 ], &b[ 7 ] ) != 11 || wcslen( p ) != 38 )
		return (HRESULT)0x800401F3;	// CO_E_CLASSSTRING
	pclsid->Data1 = d1;
	pclsid->Data2 = (WORD)d2;
	pclsid->Data3 = (WORD)d3;
	for ( int i = 0; i < 8; ++i ) pclsid->Data4[ i ] = (BYTE)b[ i ];
	return S_OK;
}

HRESULT IIDFromString(LPCOLESTR lpsz, IID* piid) { return CLSIDFromString( lpsz, piid ); }

// ---------------------------------------------------------------------------
// CCmdTarget

const AFX_INTERFACEMAP CCmdTarget::interfaceMap = { NULL, NULL };
const AFX_INTERFACEMAP* CCmdTarget::GetThisInterfaceMap() { return &CCmdTarget::interfaceMap; }
const AFX_INTERFACEMAP* CCmdTarget::GetInterfaceMap() const { return &CCmdTarget::interfaceMap; }

DWORD CCmdTarget::InternalRelease()
{
	const LONG n = InterlockedDecrement( &m_dwRef );
	if ( n == 0 )
		OnFinalRelease();
	return (DWORD)n;
}

LPUNKNOWN CCmdTarget::GetInterface(const void* piid)
{
	const IID& iid = *static_cast< const IID* >( piid );
	const bool bUnknown = ( iid == IID_IUnknown );
	for ( const AFX_INTERFACEMAP* pMap = GetInterfaceMap(); pMap && pMap->pEntry; pMap = pMap->pfnGetBaseMap ? pMap->pfnGetBaseMap() : NULL )
	{
		for ( const AFX_INTERFACEMAP_ENTRY* e = pMap->pEntry; e->nOffset != (size_t)-1; ++e )
		{
			// The first interface part answers for IUnknown, as in MFC.
			if ( bUnknown || ( e->piid && *static_cast< const IID* >( e->piid ) == iid ) )
				return reinterpret_cast< LPUNKNOWN >( reinterpret_cast< BYTE* >( this ) + e->nOffset );
		}
	}
	return NULL;
}

HRESULT CCmdTarget::InternalQueryInterface(const void* piid, LPVOID* ppvObj)
{
	if ( ! ppvObj ) return E_POINTER;
	LPUNKNOWN p = GetInterface( piid );
	*ppvObj = p;
	if ( ! p ) return E_NOINTERFACE;
	InternalAddRef();
	return S_OK;
}

LPDISPATCH CCmdTarget::GetIDispatch(BOOL bAddRef)
{
	LPUNKNOWN p = GetInterface( &IID_IDispatch );
	if ( p && bAddRef ) InternalAddRef();
	return reinterpret_cast< LPDISPATCH >( p );
}
