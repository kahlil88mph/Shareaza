// tchar.h (Linux compatibility layer): the _UNICODE mapping of the TCHAR routines.
#pragma once
#include <windows.h>
#include <cwchar>
#include <cwctype>
#include <ctime>

#define _tcslen     wcslen
#define _tcscpy     wcscpy
#define _tcsncpy    wcsncpy
#define _tcscat     wcscat
#define _tcscmp     wcscmp
#define _tcsncmp    wcsncmp
#define _tcsicmp    _wcsicmp
#define _tcsnicmp   _wcsnicmp
#define _tcschr     wcschr
#define _tcsrchr    wcsrchr
#define _tcsstr     wcsstr
#define _tcspbrk    wcspbrk
#define _tcsspn     wcsspn
#define _tcscspn    wcscspn
#define _tcstok     wcstok
#define _tcstol     wcstol
#define _tcstoul    wcstoul
#define _tcstoi64   wcstoll
#define _tcstoui64  wcstoull
#define _tcstod     wcstod
#define _tcslwr     _wcslwr
#define _tcsupr     _wcsupr
#define _totlower   towlower
#define _totupper   towupper
#define _istspace   iswspace
#define _istdigit   iswdigit
#define _istalpha   iswalpha
#define _istalnum   iswalnum
#define _istxdigit  iswxdigit
#define _istupper   iswupper
#define _istlower   iswlower
#define _istprint   iswprint
#define _istpunct   iswpunct
#define _vsntprintf _vsnwprintf
#define _sntprintf  _snwprintf
#define _tcsinc(p)  ((p) + 1)

inline int _ttoi(const wchar_t* s) { return (int)wcstol( s, NULL, 10 ); }
inline long long _ttoi64(const wchar_t* s) { return wcstoll( s, NULL, 10 ); }
inline long long _wtoi64(const wchar_t* s) { return wcstoll( s, NULL, 10 ); }
inline int _wtoi(const wchar_t* s) { return (int)wcstol( s, NULL, 10 ); }
inline double _tstof(const wchar_t* s) { return wcstod( s, NULL ); }
inline double _wtof(const wchar_t* s) { return wcstod( s, NULL ); }
inline long long _atoi64(const char* s) { return strtoll( s, NULL, 10 ); }

// Windows scanf: %s and %c in a wide format are wide.
int compat_swscanf(const wchar_t* buf, const wchar_t* fmt, ...);
#define _stscanf   compat_swscanf
#define _stscanf_s compat_swscanf
#define swscanf_s  compat_swscanf

// Secure CRT (_s) variants: same results, bounds-checked by size.
template< typename Ch > inline int compat_strcpy_s(Ch* d, size_t n, const Ch* s)
{
	if ( ! d || ! n ) return 22;
	size_t i = 0;
	for ( ; s && s[ i ] && i + 1 < n; ++i ) d[ i ] = s[ i ];
	d[ i ] = 0;
	return ( s && s[ i ] ) ? 34 : 0;	// ERANGE when truncated, as the CRT reports
}
template< typename Ch > inline int compat_strncpy_s(Ch* d, size_t n, const Ch* s, size_t c)
{
	if ( ! d || ! n ) return 22;
	size_t i = 0;
	for ( ; s && s[ i ] && i < c && i + 1 < n; ++i ) d[ i ] = s[ i ];
	d[ i ] = 0;
	return 0;
}
template< typename Ch > inline int compat_strcat_s(Ch* d, size_t n, const Ch* s)
{
	size_t l = 0;
	while ( l < n && d[ l ] ) ++l;
	return l < n ? compat_strcpy_s( d + l, n - l, s ) : 22;
}
#define _tcscpy_s  compat_strcpy_s
#define wcscpy_s   compat_strcpy_s
#define strcpy_s   compat_strcpy_s
#define _tcsncpy_s compat_strncpy_s
#define wcsncpy_s  compat_strncpy_s
#define strncpy_s  compat_strncpy_s
#define _tcscat_s  compat_strcat_s
#define wcscat_s   compat_strcat_s
#define strcat_s   compat_strcat_s
#define _stprintf_s _snwprintf
#define swprintf_s  _snwprintf
#define _sntprintf_s(b, n, c, ...) _snwprintf( b, n, __VA_ARGS__ )
int compat_sprintf_s(char* buf, size_t n, const char* fmt, ...);
int compat_sscanf(const char* buf, const char* fmt, ...);
#define sprintf_s compat_sprintf_s
#define sscanf_s  compat_sscanf
inline int gmtime_s(struct tm* r, const time_t* t) { return gmtime_r( t, r ) ? 0 : 22; }
inline int localtime_s(struct tm* r, const time_t* t) { return localtime_r( t, r ) ? 0 : 22; }
inline int memcpy_s(void* d, size_t n, const void* s, size_t c) { if ( c > n ) return 34; memcpy( d, s, c ); return 0; }
