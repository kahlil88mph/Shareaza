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
