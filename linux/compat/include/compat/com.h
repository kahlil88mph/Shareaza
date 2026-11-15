//
// compat/com.h (Linux compatibility layer)
//
// Just enough COM for the engine's internal reference-counted objects
// (IUnknown, interface macros). There is no COM runtime: plugins and
// out-of-process objects are not supported on Linux.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include <windows.h>

#define STDMETHODCALLTYPE
#define STDMETHODIMP HRESULT STDMETHODCALLTYPE
#define STDMETHODIMP_(type) type STDMETHODCALLTYPE
#define STDMETHOD(method) virtual HRESULT STDMETHODCALLTYPE method
#define STDMETHOD_(type, method) virtual type STDMETHODCALLTYPE method
#define PURE = 0
#define THIS_
#define THIS void
#define interface struct
#define DECLSPEC_UUID(x)
#define MIDL_INTERFACE(x) struct
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define CLASS_E_NOAGGREGATION ((HRESULT)0x80040110)

extern const IID IID_IUnknown;

struct IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) = 0;
	virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
	virtual ULONG STDMETHODCALLTYPE Release() = 0;
	virtual ~IUnknown() {}
};
typedef IUnknown* LPUNKNOWN;

// ATL's smart pointer, enough for engine-internal objects.
template< class T >
class CComPtr
{
public:
	CComPtr() : p( NULL ) {}
	CComPtr(T* lp) : p( lp ) { if ( p ) p->AddRef(); }
	CComPtr(const CComPtr& o) : p( o.p ) { if ( p ) p->AddRef(); }
	~CComPtr() { if ( p ) p->Release(); }
	CComPtr& operator=(T* lp) { if ( lp ) lp->AddRef(); if ( p ) p->Release(); p = lp; return *this; }
	CComPtr& operator=(const CComPtr& o) { return *this = o.p; }
	operator T*() const { return p; }
	T* operator->() const { return p; }
	T** operator&() { return &p; }
	bool operator!() const { return p == NULL; }
	void Release() { if ( T* t = p ) { p = NULL; t->Release(); } }
	T* Detach() { T* t = p; p = NULL; return t; }
	void Attach(T* lp) { if ( p ) p->Release(); p = lp; }
	T* p;
};

// ATL CAutoPtr: owning pointer with implicit conversion, as Shareaza uses it.
template< typename T >
class CAutoPtr
{
public:
	CAutoPtr() : m_p( NULL ) {}
	explicit CAutoPtr(T* p) : m_p( p ) {}
	CAutoPtr(CAutoPtr& o) : m_p( o.Detach() ) {}
	~CAutoPtr() { Free(); }
	CAutoPtr& operator=(CAutoPtr& o) { if ( this != &o ) Attach( o.Detach() ); return *this; }
	operator T*() const { return m_p; }
	T* operator->() const { return m_p; }
	void Attach(T* p) { Free(); m_p = p; }
	T* Detach() { T* p = m_p; m_p = NULL; return p; }
	void Free() { delete m_p; m_p = NULL; }
	T* m_p;
};

template< typename T >
class CAutoVectorPtr
{
public:
	CAutoVectorPtr() : m_p( NULL ) {}
	explicit CAutoVectorPtr(T* p) : m_p( p ) {}
	~CAutoVectorPtr() { Free(); }
	bool Allocate(size_t n) { Free(); m_p = new (std::nothrow) T[ n ]; return m_p != NULL; }
	operator T*() const { return m_p; }
	void Attach(T* p) { Free(); m_p = p; }
	T* Detach() { T* p = m_p; m_p = NULL; return p; }
	void Free() { delete [] m_p; m_p = NULL; }
	T* m_p;
};

// Structured exception handling. Linux has no SEH: __try/__except become a
// C++ try/catch, so C++ exceptions are still handled, but a hardware fault
// (access violation) inside the block terminates the process instead of
// being caught.
#define __try try
#define __except(filter) catch ( ... )
#define __finally catch ( ... ) { throw; }
#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0

// COM apartments: nothing to initialise on Linux.
inline HRESULT OleInitialize(LPVOID) { return S_FALSE; }
inline void OleUninitialize() {}
inline HRESULT CoInitialize(LPVOID) { return S_FALSE; }
inline HRESULT CoInitializeEx(LPVOID, DWORD) { return S_FALSE; }
inline void CoUninitialize() {}
#define COINIT_MULTITHREADED 0x0
#define COINIT_APARTMENTTHREADED 0x2
// Used only to name threads for the Visual Studio debugger.
inline void RaiseException(DWORD, DWORD, DWORD, const ULONG_PTR*) {}
#define EXCEPTION_CONTINUE_EXECUTION (-1)
