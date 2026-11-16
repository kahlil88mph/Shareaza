//
// windows.h (Linux compatibility layer)
//
// The subset of the Win32 API the Shareaza engine uses, implemented on POSIX.
// Types keep their Windows widths: DWORD and LONG are 32-bit even though
// long is 64-bit on Linux (LP64), so on-disk and on-wire layouts match.
//
// TCHAR is wchar_t. On Linux wchar_t is 32-bit (UTF-32) rather than UTF-16;
// see linux/README.md for what that means for serialised strings.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#ifndef __linux__
#error "This compatibility header is only for the Linux build"
#endif

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <cstdio>
#include <cstdarg>
#include <pthread.h>

// The SAL annotations below (__in, __out, __inout) are also identifiers inside
// libstdc++, so every standard header is parsed first, before they become macros.
#include <bits/stdc++.h>

// ---------------------------------------------------------------------------
// Compiler keywords and annotations

#define __forceinline inline __attribute__((always_inline))
#define __stdcall
#define __cdecl
#define __fastcall
#define WINAPI
#define CALLBACK
#define APIENTRY
#define STDMETHODCALLTYPE
#define __declspec(x)
#define __int64 long long
#define __int32 int
#define __int16 short
#define __int8 char
#define __assume(x) ((void)0)
#define __noop(...) ((void)0)
#define UNREFERENCED_PARAMETER(x) ((void)(x))

// <crtdbg.h> debug assertions
#ifdef _DEBUG
#include <cassert>
#define _ASSERT(x) assert(x)
#define _ASSERTE(x) assert(x)
#else
#define _ASSERT(x) ((void)0)
#define _ASSERTE(x) ((void)0)
#endif

// SAL annotations
#define __in
#define __out
#define __inout
#define __in_opt
#define __out_opt
#define __in_bcount(x)
#define __out_bcount(x)
#define __in_ecount(x)
#define __out_ecount(x)
#define __in_bcount_opt(x)
#define __out_bcount_opt(x)
#define __out_ecount_opt(x)
#define __in_z
#define __in_z_opt
#define _In_
#define _In_opt_
#define _Out_
#define _Out_opt_
#define _Inout_
#define _In_z_
#define _In_reads_(x)
#define _In_reads_bytes_(x)
#define _Out_writes_(x)
#define _Out_writes_bytes_(x)
#define _Out_writes_z_(x)
#define _Printf_format_string_
#define _Success_(x)
#define _Check_return_

#define FAR
#define NEAR
#define CONST const
#define VOID void
#ifndef NULL
#define NULL 0
#endif

// ---------------------------------------------------------------------------
// Basic types (LLP64 widths)

typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned char       UCHAR;
typedef char                CHAR;
typedef short               SHORT;
typedef unsigned short      USHORT;
typedef unsigned short      WORD;
typedef int                 INT;
typedef unsigned int        UINT;
typedef int32_t             LONG;
typedef uint32_t            ULONG;
typedef uint32_t            DWORD;
typedef long long           LONGLONG;	// not int64_t: that is long on LP64
typedef unsigned long long  ULONGLONG;
typedef unsigned long long  DWORD64;
typedef long long           INT64;
typedef unsigned long long  UINT64;
typedef int32_t             INT32;
typedef uint32_t            UINT32;
typedef int16_t             INT16;
typedef uint16_t            UINT16;
typedef int8_t              INT8;
typedef uint8_t             UINT8;
typedef float               FLOAT;
typedef double              DOUBLE;
typedef intptr_t            INT_PTR;
typedef uintptr_t           UINT_PTR;
typedef intptr_t            LONG_PTR;
typedef uintptr_t           ULONG_PTR;
typedef uintptr_t           DWORD_PTR;
typedef size_t              SIZE_T;
typedef ptrdiff_t           SSIZE_T;
typedef BYTE                BOOLEAN;
typedef BOOL*               LPBOOL;
typedef UINT*               PUINT;
typedef BYTE*               LPBYTE;
typedef BYTE*               PBYTE;
typedef WORD*               LPWORD;
typedef DWORD*              LPDWORD;
typedef DWORD*              PDWORD;
typedef LONG*               LPLONG;
typedef void*               LPVOID;
typedef void*               PVOID;
typedef const void*         LPCVOID;
typedef UINT_PTR            WPARAM;
typedef LONG_PTR            LPARAM;
typedef LONG_PTR            LRESULT;
typedef LONG                HRESULT;
typedef DWORD               COLORREF;
typedef DWORD               LCID;
typedef WORD                LANGID;

typedef char                CHAR;
typedef wchar_t             WCHAR;
typedef char*               LPSTR;
typedef char*               PSTR;
typedef const char*         LPCSTR;
typedef const char*         PCSTR;
typedef wchar_t*            LPWSTR;
typedef wchar_t*            PWSTR;
typedef const wchar_t*      LPCWSTR;
typedef const wchar_t*      PCWSTR;
typedef wchar_t*            BSTR;

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
typedef wchar_t             TCHAR;
typedef wchar_t             _TCHAR;
typedef wchar_t*            LPTSTR;
typedef const wchar_t*      LPCTSTR;
#define __TEXT(x) L##x
#define TEXT(x) __TEXT(x)
#define _T(x) __TEXT(x)
#define _TEXT(x) __TEXT(x)

typedef void*               HANDLE;
typedef HANDLE*             LPHANDLE;
typedef HANDLE              HINSTANCE;
typedef HANDLE              HMODULE;
typedef HANDLE              HWND;
typedef HANDLE              HKEY;
typedef HANDLE              HGLOBAL;
typedef HANDLE              HICON;
typedef HANDLE              HBITMAP;
typedef HANDLE              HDC;
typedef HANDLE              HMENU;
typedef HANDLE              HFONT;
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

#define TRUE  1
#define FALSE 0
#define MAX_PATH 260
#define INFINITE 0xFFFFFFFFu

#define S_OK           ((HRESULT)0)
#define S_FALSE        ((HRESULT)1)
#define E_FAIL         ((HRESULT)0x80004005)
#define E_NOTIMPL      ((HRESULT)0x80004001)
#define E_INVALIDARG   ((HRESULT)0x80070057)
#define E_OUTOFMEMORY  ((HRESULT)0x8007000E)
#define E_POINTER      ((HRESULT)0x80004003)
#define E_UNEXPECTED   ((HRESULT)0x8000FFFF)
#define SUCCEEDED(hr)  (((HRESULT)(hr)) >= 0)
#define FAILED(hr)     (((HRESULT)(hr)) < 0)

#define LOBYTE(w)  ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w)  ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define LOWORD(l)  ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)  ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define _countof(a) (sizeof(a) / sizeof((a)[0]))

typedef union _LARGE_INTEGER {
	struct { DWORD LowPart; LONG HighPart; };
	struct { DWORD LowPart; LONG HighPart; } u;
	LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
	struct { DWORD LowPart; DWORD HighPart; };
	struct { DWORD LowPart; DWORD HighPart; } u;
	ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct _FILETIME {
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _SYSTEMTIME {
	WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct _GUID {
	DWORD Data1;
	WORD  Data2;
	WORD  Data3;
	BYTE  Data4[ 8 ];
} GUID, IID, CLSID, UUID;
typedef const GUID& REFGUID;
typedef const IID& REFIID;
typedef const CLSID& REFCLSID;
inline bool operator==(const GUID& a, const GUID& b) { return memcmp( &a, &b, sizeof( GUID ) ) == 0; }
inline bool operator!=(const GUID& a, const GUID& b) { return ! ( a == b ); }
#define IsEqualGUID(a, b) ((a) == (b))

typedef struct tagPOINT { LONG x, y; } POINT, *LPPOINT;
typedef struct tagSIZE { LONG cx, cy; } SIZE, *LPSIZE;
typedef struct tagRECT { LONG left, top, right, bottom; } RECT, *LPRECT;
typedef const RECT* LPCRECT;
typedef struct tagMSG {
	HWND   hwnd;
	UINT   message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD  time;
	POINT  pt;
} MSG, *PMSG, *LPMSG;

// ---------------------------------------------------------------------------
// Memory

#define ZeroMemory(p, n)      memset( (p), 0, (n) )
#define SecureZeroMemory(p, n) memset( (p), 0, (n) )
#define FillMemory(p, n, v)   memset( (p), (v), (n) )
#define CopyMemory(d, s, n)   memcpy( (d), (s), (n) )
#define MoveMemory(d, s, n)   memmove( (d), (s), (n) )

#define GMEM_FIXED    0x0000
#define GMEM_MOVEABLE 0x0002
#define GMEM_ZEROINIT 0x0040
#define GPTR          (GMEM_FIXED | GMEM_ZEROINIT)
#define GHND          (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define LMEM_FIXED    GMEM_FIXED
#define LPTR          GPTR

// Global/Local memory: a size header in front of a malloc'd block,
// so GlobalSize works and handles are plain pointers.
HGLOBAL GlobalAlloc(UINT uFlags, SIZE_T dwBytes);
HGLOBAL GlobalReAlloc(HGLOBAL hMem, SIZE_T dwBytes, UINT uFlags);
HGLOBAL GlobalFree(HGLOBAL hMem);
SIZE_T  GlobalSize(HGLOBAL hMem);
inline LPVOID GlobalLock(HGLOBAL hMem) { return hMem; }
inline BOOL   GlobalUnlock(HGLOBAL) { return TRUE; }
#define LocalAlloc   GlobalAlloc
#define LocalReAlloc GlobalReAlloc
#define LocalFree    GlobalFree
#define LocalSize    GlobalSize

// ---------------------------------------------------------------------------
// Errors

DWORD GetLastError();
void  SetLastError(DWORD dwError);
#define ERROR_SUCCESS             0u
#define NO_ERROR                  0u
#define ERROR_FILE_NOT_FOUND      2u
#define ERROR_PATH_NOT_FOUND      3u
#define ERROR_ACCESS_DENIED       5u
#define ERROR_INVALID_HANDLE      6u
#define ERROR_NOT_ENOUGH_MEMORY   8u
#define ERROR_INVALID_DATA        13u
#define ERROR_NO_MORE_FILES       18u
#define ERROR_SHARING_VIOLATION   32u
#define ERROR_HANDLE_EOF          38u
#define ERROR_FILE_EXISTS         80u
#define ERROR_INVALID_PARAMETER   87u
#define ERROR_INSUFFICIENT_BUFFER 122u
#define ERROR_ALREADY_EXISTS      183u
#define ERROR_MORE_DATA           234u
#define ERROR_NO_UNICODE_TRANSLATION 1113u

// ---------------------------------------------------------------------------
// Time

// Milliseconds since an arbitrary start (CLOCK_MONOTONIC); wraps every 49.7 days like Windows.
DWORD     GetTickCount();
ULONGLONG GetTickCount64();
void      Sleep(DWORD dwMilliseconds);
void      GetSystemTimeAsFileTime(LPFILETIME lpFileTime);
void      GetSystemTime(LPSYSTEMTIME lpSystemTime);
void      GetLocalTime(LPSYSTEMTIME lpSystemTime);
BOOL      FileTimeToSystemTime(const FILETIME* lpFileTime, LPSYSTEMTIME lpSystemTime);
BOOL      SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, LPFILETIME lpFileTime);
BOOL      QueryPerformanceCounter(LARGE_INTEGER* lpCount);
BOOL      QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);

// ---------------------------------------------------------------------------
// Threads and synchronisation

typedef struct _CRITICAL_SECTION {
	pthread_mutex_t m_mutex;
	pthread_t       m_owner;
	LONG            RecursionCount;
} CRITICAL_SECTION, *LPCRITICAL_SECTION;

void InitializeCriticalSection(LPCRITICAL_SECTION cs);
BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION cs, DWORD dwSpinCount);
void DeleteCriticalSection(LPCRITICAL_SECTION cs);
void EnterCriticalSection(LPCRITICAL_SECTION cs);
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION cs);
void LeaveCriticalSection(LPCRITICAL_SECTION cs);

DWORD GetCurrentThreadId();
DWORD GetCurrentProcessId();

inline LONG InterlockedIncrement(LONG volatile* p) { return __atomic_add_fetch( p, 1, __ATOMIC_SEQ_CST ); }
inline LONG InterlockedDecrement(LONG volatile* p) { return __atomic_sub_fetch( p, 1, __ATOMIC_SEQ_CST ); }
inline LONG InterlockedExchange(LONG volatile* p, LONG v) { return __atomic_exchange_n( p, v, __ATOMIC_SEQ_CST ); }
inline LONG InterlockedExchangeAdd(LONG volatile* p, LONG v) { return __atomic_fetch_add( p, v, __ATOMIC_SEQ_CST ); }
inline LONG InterlockedCompareExchange(LONG volatile* p, LONG v, LONG cmp)
{
	__atomic_compare_exchange_n( p, &cmp, v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
	return cmp;
}
inline LONGLONG InterlockedIncrement64(LONGLONG volatile* p) { return __atomic_add_fetch( p, 1, __ATOMIC_SEQ_CST ); }
inline LONGLONG InterlockedDecrement64(LONGLONG volatile* p) { return __atomic_sub_fetch( p, 1, __ATOMIC_SEQ_CST ); }
inline LONGLONG InterlockedExchangeAdd64(LONGLONG volatile* p, LONGLONG v) { return __atomic_fetch_add( p, v, __ATOMIC_SEQ_CST ); }
inline LONGLONG InterlockedExchange64(LONGLONG volatile* p, LONGLONG v) { return __atomic_exchange_n( p, v, __ATOMIC_SEQ_CST ); }
template< class T > inline T* InterlockedExchangePointer(T* volatile* p, T* v) { return __atomic_exchange_n( p, v, __ATOMIC_SEQ_CST ); }

// ---------------------------------------------------------------------------
// Character conversion

#define CP_ACP    0u
#define CP_OEMCP  1u
#define CP_THREAD_ACP 3u
#define CP_UTF7   65000u
#define CP_UTF8   65001u
#define MB_PRECOMPOSED       0x00000001
#define MB_ERR_INVALID_CHARS 0x00000008
#define WC_NO_BEST_FIT_CHARS 0x00000400

// CP_ACP is Windows-1252, the code page Shareaza meets on most Windows
// systems; CP_UTF8 is strict UTF-8 <-> UTF-32. Other code pages fall back to 1252.
int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
	LPWSTR lpWideCharStr, int cchWideChar);
int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
	LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);
UINT GetACP();
UINT GetOEMCP();

// ---------------------------------------------------------------------------
// Strings (Windows CRT names)

inline int lstrlenA(LPCSTR s) { return s ? (int)strlen( s ) : 0; }
inline int lstrlenW(LPCWSTR s) { return s ? (int)wcslen( s ) : 0; }
#define lstrlen lstrlenW
inline int _stricmp(const char* a, const char* b) { return strcasecmp( a, b ); }
inline int _strnicmp(const char* a, const char* b, size_t n) { return strncasecmp( a, b, n ); }
inline int _wcsicmp(const wchar_t* a, const wchar_t* b) { return wcscasecmp( a, b ); }
inline int _wcsnicmp(const wchar_t* a, const wchar_t* b, size_t n) { return wcsncasecmp( a, b, n ); }
#define lstrcmpiW _wcsicmp
#define lstrcmpW wcscmp
#define lstrcmpi lstrcmpiW
#define lstrcmp lstrcmpW
#define lstrcmpiA _stricmp
#define stricmp _stricmp
#define strnicmp _strnicmp
wchar_t* _wcslwr(wchar_t* s);
wchar_t* _wcsupr(wchar_t* s);
char*    _strlwr(char* s);
char*    _strupr(char* s);

// Wide printf with Windows semantics: in a wide format %s and %c take wide
// arguments and %S/%C take narrow ones; %I64d/%I64u/%Iu are accepted.
int _vsnwprintf(wchar_t* buf, size_t count, const wchar_t* fmt, va_list args);
int _snwprintf(wchar_t* buf, size_t count, const wchar_t* fmt, ...);
int _vscwprintf(const wchar_t* fmt, va_list args);
// Narrow printf with Windows semantics: %S takes a wide string.
int _vsnprintf_win(char* buf, size_t count, const char* fmt, va_list args);
int _vscprintf(const char* fmt, va_list args);

// Translate a Windows printf format into its glibc equivalent.
namespace compat {
	void TranslateFormat(const wchar_t* pszIn, wchar_t* pszOut, size_t nOut, bool bWide);
	void TranslateFormat(const char* pszIn, char* pszOut, size_t nOut, bool bWide);
}

// ---------------------------------------------------------------------------
// Byte order and bit intrinsics (also in <intrin.h> / <stdlib.h> on Windows)

inline unsigned short   _byteswap_ushort(unsigned short v) { return __builtin_bswap16( v ); }
inline uint32_t         _byteswap_ulong(uint32_t v) { return __builtin_bswap32( v ); }
inline uint64_t         _byteswap_uint64(uint64_t v) { return __builtin_bswap64( v ); }
#if !__has_builtin( _rotl )	// clang -fms-extensions provides these as builtins
inline uint32_t _rotl(uint32_t v, int s) { s &= 31; return s ? ( v << s ) | ( v >> ( 32 - s ) ) : v; }
inline uint32_t _rotr(uint32_t v, int s) { s &= 31; return s ? ( v >> s ) | ( v << ( 32 - s ) ) : v; }
inline uint64_t _rotl64(uint64_t v, int s) { s &= 63; return s ? ( v << s ) | ( v >> ( 64 - s ) ) : v; }
inline uint64_t _rotr64(uint64_t v, int s) { s &= 63; return s ? ( v >> s ) | ( v << ( 64 - s ) ) : v; }
inline unsigned char _BitScanForward(DWORD* pIndex, DWORD v) { if ( ! v ) return 0; *pIndex = __builtin_ctz( v ); return 1; }
inline unsigned char _BitScanReverse(DWORD* pIndex, DWORD v) { if ( ! v ) return 0; *pIndex = 31 - __builtin_clz( v ); return 1; }
#endif


// ---------------------------------------------------------------------------
// Message box constants (the headless engine logs instead of showing these)

#define MB_OK                0x00000000u
#define MB_OKCANCEL          0x00000001u
#define MB_ABORTRETRYIGNORE  0x00000002u
#define MB_YESNOCANCEL       0x00000003u
#define MB_YESNO             0x00000004u
#define MB_RETRYCANCEL       0x00000005u
#define MB_CANCELTRYCONTINUE 0x00000006u
#define MB_ICONHAND          0x00000010u
#define MB_ICONQUESTION      0x00000020u
#define MB_ICONEXCLAMATION   0x00000030u
#define MB_ICONASTERISK      0x00000040u
#define MB_ICONERROR         MB_ICONHAND
#define MB_ICONWARNING       MB_ICONEXCLAMATION
#define MB_ICONINFORMATION   MB_ICONASTERISK
#define MB_ICONSTOP          MB_ICONHAND
#define MB_DEFBUTTON1        0x00000000u
#define MB_DEFBUTTON2        0x00000100u
#define MB_DEFBUTTON3        0x00000200u
#define MB_SETFOREGROUND     0x00010000u
#define MB_TOPMOST           0x00040000u
#define IDOK     1
#define IDCANCEL 2
#define IDABORT  3
#define IDRETRY  4
#define IDIGNORE 5
#define IDYES    6
#define IDNO     7
#define IDTRYAGAIN 10
#define IDCONTINUE 11

// ---------------------------------------------------------------------------
// File attributes (stat-based; paths as in CFile, see compat/afxfile.h)

#define FILE_ATTRIBUTE_READONLY  0x00000001u
#define FILE_ATTRIBUTE_HIDDEN    0x00000002u
#define FILE_ATTRIBUTE_SYSTEM    0x00000004u
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010u
#define FILE_ATTRIBUTE_ARCHIVE   0x00000020u
#define FILE_ATTRIBUTE_NORMAL    0x00000080u
#define FILE_ATTRIBUTE_SPARSE_FILE 0x00000200u
#define INVALID_FILE_ATTRIBUTES  0xFFFFFFFFu
#define INVALID_FILE_SIZE        0xFFFFFFFFu

typedef struct _WIN32_FILE_ATTRIBUTE_DATA {
	DWORD    dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD    nFileSizeHigh;
	DWORD    nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA, *LPWIN32_FILE_ATTRIBUTE_DATA;
typedef enum _GET_FILEEX_INFO_LEVELS { GetFileExInfoStandard, GetFileExMaxInfoLevel } GET_FILEEX_INFO_LEVELS;

BOOL  GetFileAttributesEx(LPCWSTR pszFileName, GET_FILEEX_INFO_LEVELS level, LPVOID pInfo);
DWORD GetFileAttributes(LPCWSTR pszFileName);
#define GetFileAttributesExW GetFileAttributesEx
#define GetFileAttributesW   GetFileAttributes

// ---------------------------------------------------------------------------
// Character classification (GetStringTypeW CT_CTYPE3, CharLower)

#define CT_CTYPE1 0x00000001u
#define CT_CTYPE2 0x00000002u
#define CT_CTYPE3 0x00000004u
#define C3_NONSPACING 0x0001
#define C3_DIACRITIC  0x0002
#define C3_VOWELMARK  0x0004
#define C3_SYMBOL     0x0008
#define C3_KATAKANA   0x0010
#define C3_HIRAGANA   0x0020
#define C3_HALFWIDTH  0x0040
#define C3_FULLWIDTH  0x0080
#define C3_IDEOGRAPH  0x0100
#define C3_KASHIDA    0x0200
#define C3_LEXICAL    0x0400
#define C3_ALPHA      0x8000

// CT_CTYPE3 from Unicode ranges; Windows uses its own tables, so rare
// characters may classify differently.
BOOL  GetStringTypeW(DWORD dwInfoType, LPCWSTR lpSrcStr, int cchSrc, LPWORD lpCharType);
DWORD CharLowerBuffW(LPWSTR lpsz, DWORD cchLength);
DWORD CharUpperBuffW(LPWSTR lpsz, DWORD cchLength);
LPWSTR CharLowerW(LPWSTR lpsz);
LPWSTR CharUpperW(LPWSTR lpsz);
#define CharLowerBuff CharLowerBuffW
#define CharUpperBuff CharUpperBuffW
#define CharLower CharLowerW
#define CharUpper CharUpperW
#define GetStringType GetStringTypeW

// Window messages the engine posts to its interface (dropped on Linux, see
// PostMainWndMessage).
#define WM_USER 0x0400
#define WM_APP  0x8000

// No DLLs on Linux: optional Windows libraries (Msi.dll, plugins) are absent.
inline HMODULE LoadLibraryW(LPCWSTR) { SetLastError( ERROR_FILE_NOT_FOUND ); return NULL; }
inline HMODULE LoadLibraryExW(LPCWSTR, HANDLE, DWORD) { SetLastError( ERROR_FILE_NOT_FOUND ); return NULL; }
inline BOOL FreeLibrary(HMODULE) { return TRUE; }
inline void* GetProcAddress(HMODULE, LPCSTR) { return NULL; }
#define LoadLibrary LoadLibraryW
#define LoadLibraryEx LoadLibraryExW
