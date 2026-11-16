//
// winsock2.h (Linux compatibility layer)
//
// Winsock types with their Windows layouts. IN_ADDR keeps the S_un union
// (code uses both .s_addr and .S_un.S_addr); the sockaddr structures are
// binary-compatible with the BSD ones, so they can be passed straight to
// POSIX socket calls with a cast.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include <windows.h>

#define _WINSOCKAPI_	// what Winsock's own header defines; engine headers test it
#define _WINSOCK2API_

// System socket headers first: the s_addr macro below would break them.
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/ip.h>

typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef uint32_t       u_long_win;	// Windows u_long is 32-bit

typedef struct compat_in_addr {
	union {
		struct { u_char s_b1, s_b2, s_b3, s_b4; } S_un_b;
		struct { u_short s_w1, s_w2; } S_un_w;
		uint32_t S_addr;
	} S_un;
} IN_ADDR, *PIN_ADDR, *LPIN_ADDR;
static_assert( sizeof( IN_ADDR ) == sizeof( struct in_addr ), "IN_ADDR layout" );

typedef struct compat_in6_addr {
	union {
		u_char  Byte[ 16 ];
		u_short Word[ 8 ];
	} u;
} IN6_ADDR, *PIN6_ADDR, *LPIN6_ADDR;
static_assert( sizeof( IN6_ADDR ) == sizeof( struct in6_addr ), "IN6_ADDR layout" );

typedef struct sockaddr SOCKADDR, *PSOCKADDR, *LPSOCKADDR;
typedef struct sockaddr_storage SOCKADDR_STORAGE;

typedef struct compat_sockaddr_in {
	short    sin_family;
	u_short  sin_port;
	IN_ADDR  sin_addr;
	char     sin_zero[ 8 ];
} SOCKADDR_IN, *PSOCKADDR_IN, *LPSOCKADDR_IN;
static_assert( sizeof( SOCKADDR_IN ) == sizeof( struct sockaddr_in ), "SOCKADDR_IN layout" );

typedef struct compat_sockaddr_in6 {
	short    sin6_family;
	u_short  sin6_port;
	uint32_t sin6_flowinfo;
	IN6_ADDR sin6_addr;
	uint32_t sin6_scope_id;
} SOCKADDR_IN6, *PSOCKADDR_IN6, *LPSOCKADDR_IN6;
static_assert( sizeof( SOCKADDR_IN6 ) == sizeof( struct sockaddr_in6 ), "SOCKADDR_IN6 layout" );

// Windows spells these as macros over the unions.
#define s_addr  S_un.S_addr
#define s_host  S_un.S_un_b.s_b2
#define s_net   S_un.S_un_b.s_b1
#define s_imp   S_un.S_un_w.s_w2
#define s_impno S_un.S_un_b.s_b4
#define s_lh    S_un.S_un_b.s_b3
#define s6_bytes u.Byte
#define s6_words u.Word

typedef int SOCKET;
typedef int* LPINT;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)

inline int closesocket(SOCKET s) { return close( s ); }
inline int ioctlsocket(SOCKET s, long cmd, u_long_win* argp)
{
	if ( cmd == (long)0x8004667E )	// FIONBIO
	{
		const int f = fcntl( s, F_GETFL, 0 );
		return fcntl( s, F_SETFL, *argp ? ( f | O_NONBLOCK ) : ( f & ~O_NONBLOCK ) );
	}
	return -1;
}
#ifndef FIONBIO
#define FIONBIO 0x8004667E
#endif
int WSAGetLastError();
void WSASetLastError(int nError);

#define WSAEWOULDBLOCK  EWOULDBLOCK
#define WSAEINPROGRESS  EINPROGRESS
#define WSAECONNRESET   ECONNRESET
#define WSAECONNREFUSED ECONNREFUSED
#define WSAETIMEDOUT    ETIMEDOUT
#define WSAEADDRINUSE   EADDRINUSE
#define WSAENOTCONN     ENOTCONN
#define WSAEMSGSIZE     EMSGSIZE
#define WSAEHOSTUNREACH EHOSTUNREACH
#define WSAENETUNREACH  ENETUNREACH
#define WSAEINTR        EINTR

#define IN6ADDR_ANY_INIT { { { 0 } } }
inline bool IN6_ADDR_EQUAL(const IN6_ADDR* a, const IN6_ADDR* b) { return memcmp( a, b, sizeof( IN6_ADDR ) ) == 0; }
inline void IN6_SET_ADDR_UNSPECIFIED(IN6_ADDR* a) { memset( a, 0, sizeof( *a ) ); }
inline bool IN6_IS_ADDR_UNSPECIFIED_W(const IN6_ADDR* a) { static const IN6_ADDR z = {}; return IN6_ADDR_EQUAL( a, &z ); }

// inet_ntoa/inet_addr take Windows IN_ADDR.
inline char* compat_inet_ntoa(IN_ADDR a) { struct in_addr b; memcpy( &b, &a, sizeof( b ) ); return ::inet_ntoa( b ); }
#define inet_ntoa compat_inet_ntoa

// Types the engine's network headers declare members with. Their Winsock
// and WinINet functions (WSAAsyncGetHostByName, WSAAccept conditions,
// InternetOpenUrl) have no Linux implementation yet: the network layer is
// the next porting milestone.
typedef struct hostent HOSTENT, *PHOSTENT, *LPHOSTENT;
#define MAXGETHOSTSTRUCT 1024
typedef struct _WSABUF { u_long_win len; char* buf; } WSABUF, *LPWSABUF;
typedef struct _QualityOfService { char reserved[ 1 ]; } QOS, *LPQOS;
typedef unsigned int GROUP;
typedef int (CALLBACK *LPCONDITIONPROC)(LPWSABUF lpCallerId, LPWSABUF lpCallerData, LPQOS lpSQOS, LPQOS lpGQOS,
	LPWSABUF lpCalleeId, LPWSABUF lpCalleeData, GROUP* g, DWORD_PTR dwCallbackData);
#define CF_ACCEPT 0x0000
#define CF_REJECT 0x0001
typedef void* HINTERNET;

// Address <-> string, with Windows formatting: IPv6 with a port is
// "[addr]:port", with a scope "addr%scope"; IPv4 is "a.b.c.d:port".
#define INET_ADDRSTRLEN_W 22
#define IP6_ADDRESS_STRING_LENGTH 65
#define IP6_ADDRESS_STRING_BUFFER_LENGTH 65
typedef struct _WSAPROTOCOL_INFOW* LPWSAPROTOCOL_INFOW;
typedef LPWSAPROTOCOL_INFOW LPWSAPROTOCOL_INFO;
INT WSAStringToAddressW(LPWSTR pszAddress, INT nFamily, LPWSAPROTOCOL_INFOW, LPSOCKADDR pAddress, LPINT pnLength);
INT WSAAddressToStringW(LPSOCKADDR pAddress, DWORD nLength, LPWSAPROTOCOL_INFOW, LPWSTR pszOut, LPDWORD pnOut);
// Engine code passes an unsigned long for the length (a DWORD on Windows).
inline INT WSAAddressToStringW(LPSOCKADDR pAddress, DWORD nLength, LPWSAPROTOCOL_INFOW p, LPWSTR pszOut, unsigned long* pnOut)
{
	DWORD n = (DWORD)*pnOut;
	const INT r = WSAAddressToStringW( pAddress, nLength, p, pszOut, &n );
	*pnOut = n;
	return r;
}
#define WSAStringToAddress WSAStringToAddressW
#define WSAAddressToString WSAAddressToStringW

// WinINet port constants
#define INTERNET_DEFAULT_HTTP_PORT  80
#define INTERNET_DEFAULT_HTTPS_PORT 443
#define INTERNET_DEFAULT_FTP_PORT   21
#define INTERNET_INVALID_PORT_NUMBER 0

// The engine also writes the BSD names (in_addr, sockaddr_in) and expects the
// Windows members (S_un). Every system networking header is parsed above, so
// from here on these names mean the Windows-layout structs. The layouts are
// byte-identical (see the static_asserts), so casts to the system types are safe.
// Windows' IPv6 tests take IN6_ADDR*; glibc's macros would reach into glibc's
// own struct, so they are replaced.
#undef IN6_IS_ADDR_UNSPECIFIED
#undef IN6_IS_ADDR_LOOPBACK
#undef IN6_IS_ADDR_MULTICAST
#undef IN6_IS_ADDR_LINKLOCAL
#undef IN6_IS_ADDR_SITELOCAL
#undef IN6_IS_ADDR_V4MAPPED
#undef IN6_IS_ADDR_V4COMPAT
#undef IN6_ARE_ADDR_EQUAL
inline bool IN6_IS_ADDR_UNSPECIFIED(const IN6_ADDR* a) { static const IN6_ADDR z = {}; return memcmp( a, &z, 16 ) == 0; }
inline bool IN6_IS_ADDR_LOOPBACK(const IN6_ADDR* a) { static const IN6_ADDR l = { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } }; return memcmp( a, &l, 16 ) == 0; }
inline bool IN6_IS_ADDR_MULTICAST(const IN6_ADDR* a) { return a->u.Byte[ 0 ] == 0xff; }
inline bool IN6_IS_ADDR_LINKLOCAL(const IN6_ADDR* a) { return a->u.Byte[ 0 ] == 0xfe && ( a->u.Byte[ 1 ] & 0xc0 ) == 0x80; }
inline bool IN6_IS_ADDR_SITELOCAL(const IN6_ADDR* a) { return a->u.Byte[ 0 ] == 0xfe && ( a->u.Byte[ 1 ] & 0xc0 ) == 0xc0; }
inline bool IN6_IS_ADDR_V4MAPPED(const IN6_ADDR* a)
{
	static const BYTE p[ 12 ] = { 0,0,0,0,0,0,0,0,0,0,0xff,0xff };
	return memcmp( a, p, 12 ) == 0;
}
inline bool IN6_IS_ADDR_V4COMPAT(const IN6_ADDR* a)
{
	static const BYTE p[ 12 ] = {};
	return memcmp( a, p, 12 ) == 0 && ! IN6_IS_ADDR_UNSPECIFIED( a ) && ! IN6_IS_ADDR_LOOPBACK( a );
}
// As ws2ipdef.h defines it
inline bool IN6_IS_ADDR_GLOBAL(const IN6_ADDR* a) { const BYTE h = a->u.Byte[ 0 ] & 0xf0; return h != 0 && h != 0xf0; }
inline bool IN6_ARE_ADDR_EQUAL(const IN6_ADDR* a, const IN6_ADDR* b) { return memcmp( a, b, 16 ) == 0; }

#define in_addr compat_in_addr
#define in6_addr compat_in6_addr
#define sockaddr_in compat_sockaddr_in
#define sockaddr_in6 compat_sockaddr_in6

// glibc declares in6addr_any / in6addr_loopback with its own struct type
inline const IN6_ADDR& compat_in6addr_any() { static const IN6_ADDR a = {}; return a; }
inline const IN6_ADDR& compat_in6addr_loopback() { static const IN6_ADDR a = { { { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 } } }; return a; }
#define in6addr_any compat_in6addr_any()
#define in6addr_loopback compat_in6addr_loopback()
