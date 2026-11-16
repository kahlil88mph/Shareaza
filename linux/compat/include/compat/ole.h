//
// compat/ole.h (Linux compatibility layer)
//
// OLE Automation for the engine's in-process object model: BSTR, VARIANT,
// SAFEARRAY, IDispatch, MFC's CCmdTarget interface maps (BEGIN_INTERFACE_MAP,
// INTERFACE_PART, METHOD_PROLOGUE) and ATL's CComBSTR, CComVariant and
// CComQIPtr.
//
// There is no COM runtime and no type library on Linux. Objects work through
// their C++ interfaces (QueryInterface, AddRef, Release and the interface
// methods); late-bound IDispatch::Invoke returns E_NOTIMPL. Nothing outside
// the process can reach them, so there are no plugins.
//
// OLECHAR is wchar_t, so BSTRs hold UTF-32 here; they never leave the process.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include "com.h"
#include "afxcore.h"
#include <cstddef>

// ---------------------------------------------------------------------------
// Types

typedef wchar_t OLECHAR;
typedef OLECHAR* LPOLESTR;
typedef const OLECHAR* LPCOLESTR;
typedef short VARIANT_BOOL;
typedef unsigned short VARTYPE;
typedef LONG DISPID;
typedef LONG SCODE;
typedef double DATE;
typedef LONG MEMBERID;
#define VARIANT_TRUE  ((VARIANT_BOOL)-1)
#define VARIANT_FALSE ((VARIANT_BOOL)0)
#define NOERROR S_OK
#define OLE_E_FIRST ((HRESULT)0x80040000)
#define DISP_E_UNKNOWNINTERFACE ((HRESULT)0x80020001)
#define DISP_E_MEMBERNOTFOUND   ((HRESULT)0x80020003)
#define DISP_E_PARAMNOTFOUND    ((HRESULT)0x80020004)
#define DISP_E_TYPEMISMATCH     ((HRESULT)0x80020005)
#define DISP_E_UNKNOWNNAME      ((HRESULT)0x80020006)
#define DISP_E_EXCEPTION        ((HRESULT)0x80020009)
#define DISP_E_BADINDEX         ((HRESULT)0x8002000B)
#define DISP_E_BADVARTYPE       ((HRESULT)0x80020008)
#define DISP_E_OVERFLOW         ((HRESULT)0x8002000A)
#define TYPE_E_ELEMENTNOTFOUND  ((HRESULT)0x8002802B)
#define CLASS_E_CLASSNOTAVAILABLE ((HRESULT)0x80040111)
#define REGDB_E_CLASSNOTREG     ((HRESULT)0x80040154)
#define CO_E_OBJNOTCONNECTED    ((HRESULT)0x800401FD)
#define RPC_E_SERVERFAULT       ((HRESULT)0x80010105)
#define RPC_E_INVALID_OBJECT    ((HRESULT)0x80010084)
#define DISPATCH_METHOD         0x1
#define DISPATCH_PROPERTYGET    0x2
#define DISPATCH_PROPERTYPUT    0x4
#define DISPATCH_PROPERTYPUTREF 0x8
#define DISPID_UNKNOWN (-1)
#define DISPID_VALUE   0
#define DISPID_NEWENUM (-4)
#define CLSCTX_INPROC_SERVER 0x1
#define CLSCTX_ALL 0x17
#define MAKE_HRESULT(sev, fac, code) ((HRESULT)(((unsigned long)(sev) << 31) | ((unsigned long)(fac) << 16) | ((unsigned long)(code))))
#define SEVERITY_ERROR 1
#define FACILITY_WIN32 7
#define FACILITY_ITF 4
#define HRESULT_FROM_WIN32(x) ((HRESULT)(x) <= 0 ? ((HRESULT)(x)) : ((HRESULT)(((x) & 0x0000FFFF) | (FACILITY_WIN32 << 16) | 0x80000000)))
#define RPC_S_SERVER_UNAVAILABLE 1722L

enum VARENUM {
	VT_EMPTY = 0, VT_NULL = 1, VT_I2 = 2, VT_I4 = 3, VT_R4 = 4, VT_R8 = 5, VT_CY = 6, VT_DATE = 7,
	VT_BSTR = 8, VT_DISPATCH = 9, VT_ERROR = 10, VT_BOOL = 11, VT_VARIANT = 12, VT_UNKNOWN = 13,
	VT_DECIMAL = 14, VT_I1 = 16, VT_UI1 = 17, VT_UI2 = 18, VT_UI4 = 19, VT_I8 = 20, VT_UI8 = 21,
	VT_INT = 22, VT_UINT = 23, VT_VOID = 24, VT_HRESULT = 25, VT_PTR = 26, VT_SAFEARRAY = 27,
	VT_LPSTR = 30, VT_LPWSTR = 31, VT_FILETIME = 64, VT_BLOB = 65, VT_CLSID = 72,
	VT_VECTOR = 0x1000, VT_ARRAY = 0x2000, VT_BYREF = 0x4000, VT_TYPEMASK = 0xfff
};

typedef OLECHAR* BSTR;

typedef struct tagSAFEARRAYBOUND { ULONG cElements; LONG lLbound; } SAFEARRAYBOUND, *LPSAFEARRAYBOUND;
typedef struct tagSAFEARRAY {
	USHORT cDims;
	USHORT fFeatures;
	ULONG  cbElements;
	ULONG  cLocks;
	PVOID  pvData;
	SAFEARRAYBOUND rgsabound[ 1 ];
} SAFEARRAY, *LPSAFEARRAY;

struct IDispatch;
typedef IDispatch* LPDISPATCH;
struct ITypeInfo;
typedef ITypeInfo* LPTYPEINFO;
struct ITypeLib;
typedef ITypeLib* LPTYPELIB;

typedef struct tagVARIANT VARIANT;
struct tagVARIANT {
	VARTYPE vt;
	WORD wReserved1, wReserved2, wReserved3;
	union {
		LONGLONG llVal; LONG lVal; BYTE bVal; SHORT iVal; FLOAT fltVal; DOUBLE dblVal;
		VARIANT_BOOL boolVal; SCODE scode; DATE date; BSTR bstrVal; IUnknown* punkVal;
		IDispatch* pdispVal; SAFEARRAY* parray; BYTE* pbVal; SHORT* piVal; LONG* plVal;
		BSTR* pbstrVal; VARIANT* pvarVal; void* byref; CHAR cVal; USHORT uiVal; ULONG ulVal;
		ULONGLONG ullVal; INT intVal; UINT uintVal;
	};
};
typedef VARIANT VARIANTARG;
typedef VARIANT* LPVARIANT;
#define V_VT(v) ((v)->vt)
#define V_BSTR(v) ((v)->bstrVal)
#define V_I4(v) ((v)->lVal)
#define V_BOOL(v) ((v)->boolVal)
#define V_DISPATCH(v) ((v)->pdispVal)
#define V_UNKNOWN(v) ((v)->punkVal)

typedef struct tagDISPPARAMS {
	VARIANTARG* rgvarg;
	DISPID* rgdispidNamedArgs;
	UINT cArgs;
	UINT cNamedArgs;
} DISPPARAMS;

typedef struct tagEXCEPINFO {
	WORD wCode, wReserved;
	BSTR bstrSource, bstrDescription, bstrHelpFile;
	DWORD dwHelpContext;
	PVOID pvReserved;
	HRESULT (STDMETHODCALLTYPE *pfnDeferredFillIn)(struct tagEXCEPINFO*);
	SCODE scode;
} EXCEPINFO, *LPEXCEPINFO;

struct IDispatch : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* pctinfo) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) = 0;
	virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
		DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr) = 0;
};

// Type information exists only on Windows; these are never obtained here.
struct ITypeInfo : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(LPOLESTR* rgszNames, UINT cNames, MEMBERID* pMemId) = 0;
};
struct ITypeLib : public IUnknown {};

struct IEnumVARIANT : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt, VARIANT* rgVar, ULONG* pCeltFetched) = 0;
	virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt) = 0;
	virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
	virtual HRESULT STDMETHODCALLTYPE Clone(IEnumVARIANT** ppEnum) = 0;
};

extern const IID IID_NULL;
extern const IID IID_IDispatch;
extern const IID IID_IEnumVARIANT;
extern const IID IID_IClassFactory;
#define GUID_NULL IID_NULL
#define CLSID_NULL IID_NULL

// ---------------------------------------------------------------------------
// BSTR, VARIANT, SAFEARRAY functions

BSTR SysAllocString(const OLECHAR* psz);
BSTR SysAllocStringLen(const OLECHAR* psz, UINT nLen);
BSTR SysAllocStringByteLen(LPCSTR psz, UINT nLen);
INT SysReAllocString(BSTR* pbstr, const OLECHAR* psz);
void SysFreeString(BSTR bstr);
UINT SysStringLen(BSTR bstr);
UINT SysStringByteLen(BSTR bstr);

void VariantInit(VARIANT* pv);
HRESULT VariantClear(VARIANT* pv);
HRESULT VariantCopy(VARIANT* pDest, const VARIANT* pSrc);
// Conversions between the empty, integer, boolean, floating-point and BSTR types.
HRESULT VariantChangeType(VARIANT* pDest, const VARIANT* pSrc, USHORT wFlags, VARTYPE vt);

SAFEARRAY* SafeArrayCreate(VARTYPE vt, UINT cDims, SAFEARRAYBOUND* rgsabound);
SAFEARRAY* SafeArrayCreateVector(VARTYPE vt, LONG lLbound, ULONG cElements);
HRESULT SafeArrayDestroy(SAFEARRAY* psa);
HRESULT SafeArrayAllocDescriptor(UINT cDims, SAFEARRAY** ppsaOut);
HRESULT SafeArrayAllocData(SAFEARRAY* psa);
HRESULT SafeArrayDestroyDescriptor(SAFEARRAY* psa);
HRESULT SafeArrayDestroyData(SAFEARRAY* psa);
HRESULT SafeArrayAccessData(SAFEARRAY* psa, void** ppvData);
HRESULT SafeArrayUnaccessData(SAFEARRAY* psa);
HRESULT SafeArrayGetLBound(SAFEARRAY* psa, UINT nDim, LONG* plLbound);
HRESULT SafeArrayGetUBound(SAFEARRAY* psa, UINT nDim, LONG* plUbound);
HRESULT SafeArrayGetElement(SAFEARRAY* psa, LONG* rgIndices, void* pv);
HRESULT SafeArrayPutElement(SAFEARRAY* psa, LONG* rgIndices, void* pv);

HRESULT CoCreateGuid(GUID* pguid);
void* CoTaskMemAlloc(SIZE_T cb);
void CoTaskMemFree(void* pv);
// No COM class registry: creating a COM class by CLSID always fails.
HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv);
HRESULT StringFromGUID2(REFGUID rguid, LPOLESTR lpsz, int cchMax);
HRESULT CLSIDFromString(LPCOLESTR lpsz, CLSID* pclsid);
HRESULT IIDFromString(LPCOLESTR lpsz, IID* piid);
#define CLSIDFromProgID(p, c) REGDB_E_CLASSNOTREG

// ---------------------------------------------------------------------------
// ATL smart types

class CComBSTR
{
public:
	CComBSTR() : m_str( NULL ) {}
	CComBSTR(LPCOLESTR p) : m_str( p ? SysAllocString( p ) : NULL ) {}
	CComBSTR(int nSize, LPCOLESTR p) : m_str( SysAllocStringLen( p, (UINT)nSize ) ) {}
	CComBSTR(const CStringW& s) : m_str( SysAllocStringLen( s.GetString(), (UINT)s.GetLength() ) ) {}
	explicit CComBSTR(LPCSTR p) : m_str( NULL ) { if ( p ) { CStringW w( p ); m_str = SysAllocStringLen( w, (UINT)w.GetLength() ); } }
	CComBSTR(const CComBSTR& o) : m_str( o.Copy() ) {}
	~CComBSTR() { SysFreeString( m_str ); }
	CComBSTR& operator=(const CComBSTR& o) { if ( this != &o ) { SysFreeString( m_str ); m_str = o.Copy(); } return *this; }
	CComBSTR& operator=(LPCOLESTR p) { SysFreeString( m_str ); m_str = p ? SysAllocString( p ) : NULL; return *this; }
	CComBSTR& operator=(const CStringW& s) { SysFreeString( m_str ); m_str = SysAllocStringLen( s.GetString(), (UINT)s.GetLength() ); return *this; }
	operator BSTR() const { return m_str; }
	BSTR* operator&() { return &m_str; }
	unsigned int Length() const { return SysStringLen( m_str ); }
	BSTR Copy() const { return m_str ? SysAllocStringLen( m_str, SysStringLen( m_str ) ) : NULL; }
	HRESULT CopyTo(BSTR* p) const { if ( ! p ) return E_POINTER; *p = Copy(); return S_OK; }
	BSTR Detach() { BSTR s = m_str; m_str = NULL; return s; }
	void Attach(BSTR s) { SysFreeString( m_str ); m_str = s; }
	void Empty() { SysFreeString( m_str ); m_str = NULL; }
	bool operator!() const { return m_str == NULL; }
	HRESULT Append(LPCOLESTR p) { CStringW s( m_str ); s += p; Attach( SysAllocStringLen( s, (UINT)s.GetLength() ) ); return S_OK; }
	CComBSTR& operator+=(LPCOLESTR p) { Append( p ); return *this; }
	BSTR m_str;
};

class CComVariant : public tagVARIANT
{
public:
	CComVariant() { VariantInit( this ); }
	CComVariant(const VARIANT& v) { VariantInit( this ); VariantCopy( this, &v ); }
	CComVariant(const CComVariant& v) { VariantInit( this ); VariantCopy( this, &v ); }
	CComVariant(LPCOLESTR p) { VariantInit( this ); vt = VT_BSTR; bstrVal = SysAllocString( p ); }
	CComVariant(LPCSTR p) { VariantInit( this ); vt = VT_BSTR; CStringW w( p ); bstrVal = SysAllocStringLen( w, (UINT)w.GetLength() ); }
	CComVariant(bool b) { VariantInit( this ); vt = VT_BOOL; boolVal = b ? VARIANT_TRUE : VARIANT_FALSE; }
	CComVariant(int n, VARTYPE t = VT_I4) { VariantInit( this ); vt = t; lVal = n; }
	CComVariant(DWORD n) { VariantInit( this ); vt = VT_UI4; ulVal = n; }
	CComVariant(double d) { VariantInit( this ); vt = VT_R8; dblVal = d; }
	CComVariant(LONGLONG n) { VariantInit( this ); vt = VT_I8; llVal = n; }
	CComVariant(ULONGLONG n) { VariantInit( this ); vt = VT_UI8; ullVal = n; }
	CComVariant(IDispatch* p) { VariantInit( this ); vt = VT_DISPATCH; pdispVal = p; if ( p ) p->AddRef(); }
	CComVariant(IUnknown* p) { VariantInit( this ); vt = VT_UNKNOWN; punkVal = p; if ( p ) p->AddRef(); }
	~CComVariant() { VariantClear( this ); }
	CComVariant& operator=(const CComVariant& v) { if ( this != &v ) VariantCopy( this, &v ); return *this; }
	CComVariant& operator=(const VARIANT& v) { if ( this != &v ) VariantCopy( this, &v ); return *this; }
	HRESULT Clear() { return VariantClear( this ); }
	HRESULT ChangeType(VARTYPE t, const VARIANT* pSrc = NULL) { return VariantChangeType( this, pSrc ? pSrc : this, 0, t ); }
	HRESULT Detach(VARIANT* p) { if ( ! p ) return E_POINTER; VariantClear( p ); *p = *this; vt = VT_EMPTY; return S_OK; }
	HRESULT CopyTo(VARIANT* p) const { return VariantCopy( p, this ); }
};

template< class T, const IID* piid = NULL >
class CComQIPtr : public CComPtr< T >
{
public:
	CComQIPtr() {}
	CComQIPtr(T* p) : CComPtr< T >( p ) {}
	CComQIPtr(IUnknown* p) { if ( p && piid ) p->QueryInterface( *piid, (void**)&this->p ); }
	CComQIPtr& operator=(T* p) { CComPtr< T >::operator=( p ); return *this; }
};

// ---------------------------------------------------------------------------
// CCmdTarget: MFC's base for COM objects, with interface maps.

struct AFX_INTERFACEMAP_ENTRY
{
	const void* piid;	// the IID (NULL ends the map)
	size_t nOffset;		// offset of the interface part from the object
};

struct AFX_INTERFACEMAP
{
	const AFX_INTERFACEMAP* (*pfnGetBaseMap)();
	const AFX_INTERFACEMAP_ENTRY* pEntry;
};

class CCmdTarget : public CObject
{
public:
	CCmdTarget() : m_dwRef( 1 ) {}
	virtual ~CCmdTarget() {}

	// Reference counting and interface lookup, as MFC's oleunk.cpp
	DWORD InternalAddRef() { return (DWORD)InterlockedIncrement( &m_dwRef ); }
	DWORD InternalRelease();
	DWORD ExternalAddRef() { return InternalAddRef(); }
	DWORD ExternalRelease() { return InternalRelease(); }
	HRESULT InternalQueryInterface(const void* piid, LPVOID* ppvObj);
	HRESULT ExternalQueryInterface(const void* piid, LPVOID* ppvObj) { return InternalQueryInterface( piid, ppvObj ); }
	LPUNKNOWN GetInterface(const void* piid);
	LPUNKNOWN GetControllingUnknown() { return GetInterface( &IID_IUnknown ); }
	LPDISPATCH GetIDispatch(BOOL bAddRef);
	virtual void OnFinalRelease() { delete this; }

	void EnableAutomation() {}
	void EnableAggregation() {}
	void EnableTypeLib() {}
	UINT GetTypeInfoCount() { return 0; }
	HRESULT GetTypeInfoOfGuid(LCID, REFGUID, LPTYPEINFO* ppTypeInfo) { if ( ppTypeInfo ) *ppTypeInfo = NULL; return E_NOTIMPL; }
	BOOL IsInvokeAllowed(DISPID) { return TRUE; }

	static const AFX_INTERFACEMAP* GetThisInterfaceMap();
	virtual const AFX_INTERFACEMAP* GetInterfaceMap() const;

	volatile LONG m_dwRef;
private:
	static const AFX_INTERFACEMAP interfaceMap;
};

// Late-bound calls need a type library, which Linux does not have.
inline HRESULT DispInvoke(void*, ITypeInfo*, DISPID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) { return E_NOTIMPL; }

#define DECLARE_INTERFACE_MAP() \
private: \
	static const AFX_INTERFACEMAP_ENTRY _interfaceEntries[]; \
protected: \
	static const AFX_INTERFACEMAP interfaceMap; \
public: \
	static const AFX_INTERFACEMAP* GetThisInterfaceMap(); \
	virtual const AFX_INTERFACEMAP* GetInterfaceMap() const override;

#define BEGIN_INTERFACE_MAP(theClass, theBase) \
	const AFX_INTERFACEMAP* theClass::GetThisInterfaceMap() { return &theClass::interfaceMap; } \
	const AFX_INTERFACEMAP* theClass::GetInterfaceMap() const { return &theClass::interfaceMap; } \
	const AFX_INTERFACEMAP theClass::interfaceMap = { &theBase::GetThisInterfaceMap, &theClass::_interfaceEntries[ 0 ] }; \
	const AFX_INTERFACEMAP_ENTRY theClass::_interfaceEntries[] = {

#define INTERFACE_PART(theClass, iid, localClass) \
	{ &iid, offsetof( theClass, m_x##localClass ) },

#define INTERFACE_AGGREGATE(theClass, theAggr) \
	{ NULL, offsetof( theClass, theAggr ) },

#define END_INTERFACE_MAP() \
	{ NULL, (size_t)-1 } };

#define BEGIN_INTERFACE_PART(localClass, baseClass) \
	class X##localClass : public baseClass \
	{ \
	public: \
		STDMETHOD_(ULONG, AddRef)(); \
		STDMETHOD_(ULONG, Release)(); \
		STDMETHOD(QueryInterface)(REFIID iid, LPVOID* ppvObj);

#define INIT_INTERFACE_PART(theClass, localClass)

#define END_INTERFACE_PART(localClass) \
	} m_x##localClass; \
	friend class X##localClass;

#define END_INTERFACE_PART_STATIC END_INTERFACE_PART

#define METHOD_PROLOGUE(theClass, localClass) \
	theClass* pThis = ((theClass*)((BYTE*)this - offsetof( theClass, m_x##localClass ))); \
	(void)pThis;
#define METHOD_PROLOGUE_EX METHOD_PROLOGUE
#define METHOD_PROLOGUE_EX_ METHOD_PROLOGUE

// Type libraries and class factories: registration with Windows only.
#define DECLARE_OLETYPELIB(theClass)
#define IMPLEMENT_OLETYPELIB(theClass, tlid, wVerMajor, wVerMinor)
#define DECLARE_OLECREATE(theClass)
#define IMPLEMENT_OLECREATE(theClass, pszProgID, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)
#define IMPLEMENT_OLECREATE_FLAGS(theClass, pszProgID, flags, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)
#define DECLARE_DISPATCH_MAP()
#define afxRegApartmentThreading 0
#define afxRegFreeThreading 0

#define ResultFromScode(sc) ((HRESULT)(sc))
#define GetScode(hr) ((SCODE)(hr))
