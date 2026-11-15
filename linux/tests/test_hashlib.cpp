// HashLib on Linux: published test vectors, plus random files checked
// against rhash (an independent implementation) when it is installed.

#include <windows.h>
#include "HashLib.h"

#include <cstdio>
#include <random>
#include <string>
#include <vector>
#include <unistd.h>

static int g_nFailed = 0, g_nPassed = 0;

static void Check(bool bOk, const std::string& sWhat, const std::string& sGot = "", const std::string& sWant = "")
{
	if ( bOk )
		++g_nPassed;
	else
	{
		++g_nFailed;
		printf( "FAIL %s\n  got  %s\n  want %s\n", sWhat.c_str(), sGot.c_str(), sWant.c_str() );
	}
}

static std::string Hex(const uchar* p, size_t n)
{
	static const char x[] = "0123456789abcdef";
	std::string s;
	for ( size_t i = 0; i < n; ++i ) { s += x[ p[ i ] >> 4 ]; s += x[ p[ i ] & 15 ]; }
	return s;
}

static std::string Base32(const uchar* p, size_t n)
{
	static const char a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
	std::string s;
	unsigned nBits = 0, nBuf = 0;
	for ( size_t i = 0; i < n; ++i )
	{
		nBuf = ( nBuf << 8 ) | p[ i ];
		nBits += 8;
		while ( nBits >= 5 ) { s += a[ ( nBuf >> ( nBits - 5 ) ) & 31 ]; nBits -= 5; }
	}
	if ( nBits ) s += a[ ( nBuf << ( 5 - nBits ) ) & 31 ];
	return s;
}

static std::string Sha1(const std::string& d)  { CSHA h; h.Add( d.data(), d.size() ); h.Finish(); uchar r[ 20 ]; h.GetHash( r ); return Hex( r, 20 ); }
static std::string Md5(const std::string& d)   { CMD5 h; h.Add( d.data(), d.size() ); h.Finish(); uchar r[ 16 ]; h.GetHash( r ); return Hex( r, 16 ); }
static std::string Md4(const std::string& d)   { CMD4 h; h.Add( d.data(), d.size() ); h.Finish(); uchar r[ 16 ]; h.GetHash( r ); return Hex( r, 16 ); }

// Hash in chunks the way the library hasher feeds file reads. CTigerTree
// treats each call's trailing partial 1 KB block as a whole leaf, so every
// chunk but the last must be a multiple of 1024 bytes.
static std::string Tth(const std::string& d, CTigerTree* pOut = NULL)
{
	CTigerTree t;
	CTigerTree& h = pOut ? *pOut : t;
	h.BeginFile( 10, d.size() );
	for ( size_t i = 0; i < d.size(); )
	{
		const size_t n = std::min< size_t >( 65536 * 3, d.size() - i );
		h.AddToFile( d.data() + i, (uint32)n );
		i += n;
	}
	if ( ! h.FinishFile() ) return "(FinishFile failed)";
	uchar r[ 24 ];
	h.GetRoot( r );
	return Base32( r, 24 );
}

static std::string Ed2k(const std::string& d, CED2K* pOut = NULL)
{
	CED2K t;
	CED2K& h = pOut ? *pOut : t;
	h.BeginFile( d.size() );
	for ( size_t i = 0; i < d.size(); )
	{
		const size_t n = std::min< size_t >( 1000003, d.size() - i );
		h.AddToFile( d.data() + i, (uint32)n );
		i += n;
	}
	if ( ! h.FinishFile() ) return "(FinishFile failed)";
	uchar r[ 16 ];
	h.GetRoot( r );
	return Hex( r, 16 );
}

static void TestVectors()
{
	const std::string sMillionA( 1000000, 'a' );
	Check( Sha1( "" ) == "da39a3ee5e6b4b0d3255bfef95601890afd80709", "SHA1 empty", Sha1( "" ) );
	Check( Sha1( "abc" ) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA1 abc", Sha1( "abc" ) );
	Check( Sha1( "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1", "SHA1 448-bit" );
	Check( Sha1( sMillionA ) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f", "SHA1 million a", Sha1( sMillionA ) );
	Check( Md5( "" ) == "d41d8cd98f00b204e9800998ecf8427e", "MD5 empty", Md5( "" ) );
	Check( Md5( "abc" ) == "900150983cd24fb0d6963f7d28e17f72", "MD5 abc", Md5( "abc" ) );
	Check( Md5( "message digest" ) == "f96b697d7cb7938d525a2f31aaf161d0", "MD5 message digest" );
	Check( Md4( "" ) == "31d6cfe0d16ae931b73c59d7e0c089c0", "MD4 empty", Md4( "" ) );
	Check( Md4( "abc" ) == "a448017aaf21d8525fc10ae87aa6729d", "MD4 abc", Md4( "abc" ) );
	Check( Md4( "message digest" ) == "d9130a8164549fe818874806e1c7014b", "MD4 message digest" );
	// THEX test vectors
	Check( Tth( "" ) == "LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ", "TTH empty", Tth( "" ) );
	Check( Tth( std::string( 1, '\0' ) ) == "VK54ZIEEVTWNAUI5D5RDFIL37LX2IQNSTAXFKSA", "TTH one zero byte", Tth( std::string( 1, '\0' ) ) );
	Check( Tth( std::string( 1024, 'A' ) ) == "L66Q4YVNAFWVS23X2HJIRA5ZJ7WXR3F26RSASFA", "TTH 1024 A", Tth( std::string( 1024, 'A' ) ) );
	Check( Tth( std::string( 1025, 'A' ) ) == "PZMRYHGY6LTBEH63ZWAHDORHSYTLO4LEFUIKHWY", "TTH 1025 A", Tth( std::string( 1025, 'A' ) ) );
	Check( Ed2k( "" ) == "31d6cfe0d16ae931b73c59d7e0c089c0", "ED2K empty", Ed2k( "" ) );
}

// Hash trees survive ToBytes/FromBytes, and block verification accepts good
// data and rejects corrupted data.
static void TestTrees()
{
	std::mt19937 rng( 7 );
	std::string d( 5 * ED2K_PART_SIZE / 2, '\0' );
	for ( char& c : d ) c = (char)rng();

	CED2K e;
	const std::string sRoot = Ed2k( d, &e );
	BYTE* p = NULL; uint32 n = 0;
	Check( e.ToBytes( &p, &n ) && n == 3 * 16, "ED2K ToBytes size", std::to_string( n ), "48" );
	CED2K e2;
	Check( e2.FromBytes( p, n, d.size() ) == TRUE, "ED2K FromBytes" );
	GlobalFree( p );
	uchar r[ 16 ]; e2.GetRoot( r );
	Check( Hex( r, 16 ) == sRoot, "ED2K root after round trip", Hex( r, 16 ), sRoot );
	e2.BeginBlockTest(); e2.AddToTest( d.data() + ED2K_PART_SIZE, ED2K_PART_SIZE );
	Check( e2.FinishBlockTest( 1 ) == TRUE, "ED2K block 1 verifies" );
	std::string bad = d.substr( ED2K_PART_SIZE, ED2K_PART_SIZE ); bad[ 1234 ] ^= 1;
	e2.BeginBlockTest(); e2.AddToTest( bad.data(), ED2K_PART_SIZE );
	Check( e2.FinishBlockTest( 1 ) == FALSE, "ED2K corrupted block 1 rejected" );

	CTigerTree t;
	const std::string sTth = Tth( d, &t );
	uint8* q = NULL; uint32 m = 0;
	Check( t.ToBytes( &q, &m ) == TRUE && m > 24, "TTH ToBytes" );
	CTigerTree t2;
	const BOOL bFrom = t2.FromBytes( q, m, t.GetHeight(), d.size() );
	Check( bFrom == TRUE, "TTH FromBytes", "height " + std::to_string( t.GetHeight() ) + ", " + std::to_string( m ) + " bytes" );
	GlobalFree( q );
	if ( ! bFrom ) return;
	uchar tr[ 24 ]; t2.GetRoot( tr );
	Check( Base32( tr, 24 ) == sTth, "TTH root after round trip", Base32( tr, 24 ), sTth );
	const uint32 nBlock = t2.GetBlockLength();
	t2.BeginBlockTest(); t2.AddToTest( d.data() + 2 * nBlock, nBlock );
	Check( t2.FinishBlockTest( 2 ) == TRUE, "TTH block 2 verifies" );
	std::string tbad = d.substr( 2 * nBlock, nBlock ); tbad[ 99 ] ^= 1;
	t2.BeginBlockTest(); t2.AddToTest( tbad.data(), nBlock );
	Check( t2.FinishBlockTest( 2 ) == FALSE, "TTH corrupted block 2 rejected" );
}

// Random files of awkward sizes, compared with rhash.
static void TestAgainstRhash()
{
	if ( system( "rhash --version >/dev/null 2>&1" ) != 0 )
	{
		printf( "rhash not installed; skipping cross-check\n" );
		return;
	}
	char szDir[] = "/tmp/hashlib-XXXXXX";
	if ( ! mkdtemp( szDir ) ) return;
	const size_t nSizes[] = { 1, 63, 64, 1023, 1024, 1025, 65536, 999999,
		ED2K_PART_SIZE - 1, ED2K_PART_SIZE + 1, 3 * ED2K_PART_SIZE + 12345 };
	std::mt19937 rng( 42 );
	for ( size_t nSize : nSizes )
	{
		std::string d( nSize, '\0' );
		for ( char& c : d ) c = (char)rng();
		const std::string sPath = std::string( szDir ) + "/f";
		FILE* f = fopen( sPath.c_str(), "wb" ); fwrite( d.data(), 1, d.size(), f ); fclose( f );
		const std::string sCmd = "rhash --printf='%{sha1} %{md5} %{ed2k} %{tth}' '" + sPath + "'";
		FILE* r = popen( sCmd.c_str(), "r" );
		char szLine[ 512 ] = {};
		if ( ! fgets( szLine, sizeof( szLine ), r ) ) szLine[ 0 ] = 0;
		pclose( r );
		char s1[ 64 ] = {}, s2[ 64 ] = {}, s3[ 64 ] = {}, s4[ 64 ] = {};
		sscanf( szLine, "%63s %63s %63s %63s", s1, s2, s3, s4 );
		const std::string sN = std::to_string( nSize );
		Check( Sha1( d ) == s1, "SHA1 vs rhash, " + sN + " bytes", Sha1( d ), s1 );
		Check( Md5( d ) == s2, "MD5 vs rhash, " + sN + " bytes", Md5( d ), s2 );
		Check( Ed2k( d ) == s3, "ED2K vs rhash, " + sN + " bytes", Ed2k( d ), s3 );
		std::string sTthWant( s4 );
		for ( char& c : sTthWant ) c = (char)toupper( (unsigned char)c );
		Check( Tth( d ) == sTthWant, "TTH vs rhash, " + sN + " bytes", Tth( d ), sTthWant );
		unlink( sPath.c_str() );
	}
	rmdir( szDir );
}

int main()
{
	setvbuf( stdout, NULL, _IONBF, 0 );
	TestVectors();
	TestAgainstRhash();
	TestTrees();
	printf( "hashlib: %d passed, %d failed\n", g_nPassed, g_nFailed );
	return g_nFailed ? 1 : 0;
}
