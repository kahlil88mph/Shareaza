// G2 packets built and parsed by the engine's own code, checked byte for
// byte against the G2 wire format:
//   control byte: len_len << 6 | ( name_len - 1 ) << 3 | compound 0x04 | big-endian 0x02
//   then len_len little-endian length bytes, the name, and the payload.

#include "StdAfx.h"
#include "Shareaza.h"
#include "Buffer.h"
#include "G2Packet.h"

#include <cstdio>
#include <string>
#include <vector>

// The protocol handlers live in the half of G2Packet.cpp that needs the
// network layer; the packet vtable still refers to them.
BOOL CG2Packet::OnPacket(const SOCKADDR_IN*) { return FALSE; }
BOOL CG2Packet::OnPacket(const SOCKADDR_IN6*) { return FALSE; }

static int g_nFailed = 0, g_nPassed = 0;

static std::string Hex(const BYTE* p, size_t n)
{
	std::string s;
	char b[ 4 ];
	for ( size_t i = 0; i < n; ++i ) { snprintf( b, sizeof( b ), "%02x ", p[ i ] ); s += b; }
	if ( ! s.empty() ) s.pop_back();
	return s;
}

static void Check(bool bOk, const char* szWhat, const std::string& sDetail = "")
{
	if ( bOk ) ++g_nPassed;
	else { ++g_nFailed; printf( "FAIL %s %s\n", szWhat, sDetail.c_str() ); }
}

static std::string Serialize(CG2Packet* pPacket)
{
	CBuffer oBuffer;
	pPacket->ToBuffer( &oBuffer );
	return Hex( oBuffer.GetData(), oBuffer.GetCount() );
}

static void TestBuild()
{
	CG2Packet* pPing = CG2Packet::New( G2_PACKET_PING );
	const std::string sPing = Serialize( pPing );
	// Shareaza always writes at least one length byte (CG2Packet::ToBuffer
	// starts nLenLen at 1), so an empty /PI is 48 00 rather than the minimal 08.
	Check( sPing == "48 00 50 49", "/PI", sPing );
	pPing->Release();

	// /Q2 { /DN "abc" }
	CG2Packet* pQuery = CG2Packet::New( G2_PACKET_QUERY, TRUE );
	pQuery->WritePacket( G2_PACKET_DESCRIPTIVE_NAME, 3 );
	pQuery->WriteString( L"abc", FALSE );
	const std::string sQuery = Serialize( pQuery );
	Check( sQuery == "4c 07 51 32 48 03 44 4e 61 62 63", "/Q2/DN compound", sQuery );
	pQuery->Release();

	// Strings go on the wire as UTF-8, including characters beyond the BMP
	// (wchar_t is UTF-32 on Linux, UTF-16 on Windows).
	CG2Packet* pName = CG2Packet::New( G2_PACKET_NAME );
	const wchar_t szName[] = { L'c', L'a', L'f', 0xE9, L' ', 0x1F600, 0 };
	Check( pName->GetStringLen( szName ) == 10, "UTF-8 length", std::to_string( pName->GetStringLen( szName ) ) );
	pName->WriteString( szName, FALSE );
	const std::string sName = Serialize( pName );
	Check( sName == "58 0a 4e 41 4d 45 63 61 66 c3 a9 20 f0 9f 98 80", "/NAME UTF-8 payload", sName );
	pName->Release();

	// Length field widths: 1, 2 and 3 bytes
	for ( DWORD nLen : { 255u, 256u, 65535u, 65536u } )
	{
		CG2Packet* p = CG2Packet::New( G2_PACKET_HUB );
		std::vector< BYTE > oData( nLen, 0xAB );
		p->Write( oData.data(), nLen );
		CBuffer b;
		p->ToBuffer( &b );
		const BYTE nLenLen = (BYTE)( b.GetData()[ 0 ] >> 6 );
		const BYTE nExpect = nLen > 0xFFFF ? 3 : nLen > 0xFF ? 2 : 1;
		DWORD nWire = 0;
		for ( BYTE i = 0; i < nLenLen; ++i ) nWire |= (DWORD)b.GetData()[ 1 + i ] << ( 8 * i );
		Check( nLenLen == nExpect && nWire == nLen && b.GetCount() == 1 + nLenLen + 3 + nLen,
			"length field", "len " + std::to_string( nLen ) + " lenlen " + std::to_string( nLenLen ) );
		p->Release();
	}
}

static void TestParse()
{
	// Two packets back to back, then a truncated third: the reader takes
	// whole packets and leaves the partial one in the buffer.
	const BYTE pStream[] = {
		0x4c, 0x07, 'Q', '2', 0x48, 0x03, 'D', 'N', 'a', 'b', 'c',
		0x08, 'P', 'I',
		0x48, 0x05, 'D', 'N', 'x' };
	CBuffer oBuffer;
	oBuffer.Add( pStream, sizeof( pStream ) );

	CG2Packet* p1 = CG2Packet::ReadBuffer( &oBuffer );
	Check( p1 && p1->IsType( G2_PACKET_QUERY ) && p1->m_bCompound, "read /Q2" );
	if ( p1 )
	{
		G2_PACKET nType = 0;
		DWORD nLength = 0;
		BOOL bCompound = TRUE;
		Check( p1->ReadPacket( nType, nLength, &bCompound ) && nType == G2_PACKET_DESCRIPTIVE_NAME && nLength == 3 && ! bCompound, "read /Q2/DN header" );
		const CString s = p1->ReadString( nLength );
		Check( s == L"abc", "read /Q2/DN string" );
		Check( ! p1->ReadPacket( nType, nLength ), "no more children" );
		p1->Release();
	}
	CG2Packet* p2 = CG2Packet::ReadBuffer( &oBuffer );
	Check( p2 && p2->IsType( G2_PACKET_PING ) && p2->m_nLength == 0, "read /PI" );
	if ( p2 ) p2->Release();
	CG2Packet* p3 = CG2Packet::ReadBuffer( &oBuffer );
	Check( p3 == NULL && oBuffer.GetCount() == 5, "partial packet waits for more data", std::to_string( oBuffer.GetCount() ) );
	if ( p3 ) p3->Release();

	// UTF-8 round trip through ReadString
	CG2Packet* pName = CG2Packet::New( G2_PACKET_NAME );
	const wchar_t szName[] = { 0x65E5, 0x672C, L'-', 0x1F600, 0 };
	pName->WriteString( szName, FALSE );
	CBuffer b;
	pName->ToBuffer( &b );
	pName->Release();
	CG2Packet* pBack = CG2Packet::ReadBuffer( &b );
	Check( pBack && pBack->ReadString( pBack->m_nLength ) == szName, "UTF-8 round trip" );
	if ( pBack ) pBack->Release();

	// Malformed: a child that claims to be longer than its parent
	const BYTE pBad[] = { 0x4c, 0x05, 'Q', '2', 0x48, 0x09, 'D', 'N', 'a' };
	CBuffer oBad;
	oBad.Add( pBad, sizeof( pBad ) );
	CG2Packet* pB = CG2Packet::ReadBuffer( &oBad );
	if ( pB )
	{
		G2_PACKET nType;
		DWORD nLength;
		bool bThrew = false;
		BOOL bRead = FALSE;
		try { bRead = pB->ReadPacket( nType, nLength ); }
		catch ( CException* e ) { bThrew = true; e->Delete(); }
		Check( bThrew || ! bRead, "oversized child rejected" );
		pB->Release();
	}
	else
		Check( true, "oversized child rejected" );
}

int main()
{
	TestBuild();
	TestParse();
	printf( "g2packet: %d passed, %d failed\n", g_nPassed, g_nFailed );
	return g_nFailed ? 1 : 0;
}
