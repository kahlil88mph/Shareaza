// The engine's COM object model on Linux: an XML tree from the engine's own
// parser (XML.cpp), wrapped by its automation layer (XMLCOM.cpp) and used
// only through the IDL interfaces. Exercises the MFC interface maps,
// METHOD_PROLOGUE, QueryInterface across interface parts, reference
// counting, BSTRs and VARIANTs.

#include "StdAfx.h"
#include "Shareaza.h"
#include "XML.h"
#include "XMLCOM.h"

#include <cstdio>

static int g_nFailed = 0, g_nPassed = 0;
#define CHECK(x) do { if ( x ) ++g_nPassed; else { ++g_nFailed; printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); } } while ( 0 )

static CString Take(BSTR s)
{
	CString r( s );
	SysFreeString( s );
	return r;
}

static void TestXmlThroughCom()
{
	CXMLElement* pRoot = CXMLElement::FromString(
		L"<audios><audio title=\"Song &amp; Dance\" artist=\"caf\x00e9\"><genre>Jazz</genre></audio><audio title=\"Two\"/></audios>" );
	CHECK( pRoot != NULL );
	if ( ! pRoot ) return;

	ISXMLElement* pElement = (ISXMLElement*)CXMLCOM::Wrap( pRoot, IID_ISXMLElement );
	CHECK( pElement != NULL );
	if ( ! pElement ) { delete pRoot; return; }

	BSTR s = NULL;
	CHECK( SUCCEEDED( pElement->get_Name( &s ) ) && Take( s ) == L"audios" );

	// Collection interface part of another object, reached through the IDL
	ISXMLElements* pChildren = NULL;
	CHECK( SUCCEEDED( pElement->get_Elements( &pChildren ) ) && pChildren );
	LONG nCount = 0;
	if ( pChildren )
	{
		CHECK( SUCCEEDED( pChildren->get_Count( &nCount ) ) && nCount == 2 );

		ISXMLElement* pFirst = NULL;
		CHECK( SUCCEEDED( pChildren->get_First( &pFirst ) ) && pFirst );
		if ( pFirst )
		{
			ISXMLAttributes* pAttrs = NULL;
			CHECK( SUCCEEDED( pFirst->get_Attributes( &pAttrs ) ) && pAttrs );
			if ( pAttrs )
			{
				BSTR v = NULL;
				CComBSTR sName( L"title" );
				CHECK( SUCCEEDED( pAttrs->get_Get( sName, &v ) ) && Take( v ) == L"Song & Dance" );
				CComBSTR sArtist( L"ARTIST" );	// attribute lookup is case-insensitive
				CHECK( SUCCEEDED( pAttrs->get_Get( sArtist, &v ) ) && Take( v ) == L"caf\x00e9" );
				pAttrs->Release();
			}

			// QueryInterface from one interface part to another of the same object
			ISXMLNode* pNode = NULL;
			CHECK( SUCCEEDED( pFirst->QueryInterface( IID_ISXMLNode, (void**)&pNode ) ) && pNode );
			if ( pNode )
			{
				SXMLNodeType nType = xmlNode;
				CHECK( SUCCEEDED( pNode->get_Type( &nType ) ) && nType == xmlElement );
				CHECK( (void*)pNode != (void*)pFirst );	// a different vtable of the same object
				pNode->Release();
			}
			IUnknown* pUnknown1 = NULL;
			IUnknown* pUnknown2 = NULL;
			pFirst->QueryInterface( IID_IUnknown, (void**)&pUnknown1 );
			pFirst->QueryInterface( IID_IUnknown, (void**)&pUnknown2 );
			CHECK( pUnknown1 && pUnknown1 == pUnknown2 );	// COM identity
			if ( pUnknown1 ) pUnknown1->Release();
			if ( pUnknown2 ) pUnknown2->Release();
			void* pNone = (void*)1;
			CHECK( pFirst->QueryInterface( IID_ISXMLAttributes, &pNone ) == E_NOINTERFACE && pNone == NULL );

			// Unwrap gives back the engine's own object
			CHECK( CXMLCOM::Unwrap( pFirst ) == pRoot->GetFirstElement() );
			pFirst->Release();
		}

		// Item() by VARIANT index
		CComVariant vIndex( 1 );
		ISXMLElement* pSecond = NULL;
		CHECK( SUCCEEDED( pChildren->get_Item( vIndex, &pSecond ) ) && pSecond );
		if ( pSecond )
		{
			VARIANT_BOOL bNamed = VARIANT_FALSE;
			CComBSTR sAudio( L"audio" );
			CHECK( SUCCEEDED( pSecond->IsNamed( sAudio, &bNamed ) ) && bNamed == VARIANT_TRUE );
			pSecond->Release();
		}
		pChildren->Release();
	}

	// Changing the tree through COM changes the engine object
	CComBSTR sNew( L"library" );
	CHECK( SUCCEEDED( pElement->put_Name( sNew ) ) && pRoot->GetName() == L"library" );
	BSTR sXml = NULL;
	CHECK( SUCCEEDED( pElement->ToString( &sXml ) ) && Take( sXml ).Find( L"<library>" ) == 0 );

	pElement->Release();
	delete pRoot;
}

static void TestVariants()
{
	CComVariant v( L"42" );
	CHECK( SUCCEEDED( v.ChangeType( VT_I4 ) ) && v.vt == VT_I4 && v.lVal == 42 );
	CHECK( SUCCEEDED( v.ChangeType( VT_BSTR ) ) && CString( v.bstrVal ) == L"42" );
	CComVariant b( L"true" );
	CHECK( SUCCEEDED( b.ChangeType( VT_BOOL ) ) && b.boolVal == VARIANT_TRUE );
	CComVariant big( 70000 );
	CHECK( big.ChangeType( VT_UI2 ) == DISP_E_OVERFLOW );
	CComVariant bad( L"x1" );
	CHECK( bad.ChangeType( VT_I4 ) == DISP_E_TYPEMISMATCH );
	BSTR s = SysAllocStringLen( L"ab\0cd", 5 );	// BSTRs carry their length, NULs included
	CHECK( SysStringLen( s ) == 5 && SysStringByteLen( s ) == 5 * sizeof( OLECHAR ) );
	SysFreeString( s );

	SAFEARRAYBOUND bound = { 3, 0 };
	SAFEARRAY* psa = SafeArrayCreate( VT_BSTR, 1, &bound );
	LONG i = 2;
	CComBSTR sItem( L"third" );
	CHECK( psa && SUCCEEDED( SafeArrayPutElement( psa, &i, sItem ) ) );
	BSTR sBack = NULL;
	CHECK( SUCCEEDED( SafeArrayGetElement( psa, &i, &sBack ) ) && Take( sBack ) == L"third" );
	LONG nUpper = 0;
	CHECK( SUCCEEDED( SafeArrayGetUBound( psa, 1, &nUpper ) ) && nUpper == 2 );
	i = 3;
	CHECK( SafeArrayPutElement( psa, &i, sItem ) == DISP_E_BADINDEX );
	SafeArrayDestroy( psa );

	GUID g1, g2;
	CHECK( SUCCEEDED( CoCreateGuid( &g1 ) ) && SUCCEEDED( CoCreateGuid( &g2 ) ) && g1 != g2 );
	wchar_t sz[ 40 ];
	StringFromGUID2( g1, sz, 40 );
	GUID g3;
	CHECK( SUCCEEDED( CLSIDFromString( sz, &g3 ) ) && g3 == g1 && ( g1.Data3 >> 12 ) == 4 );
}

int main()
{
	theApp.m_fnLog = []( WORD, const CString& ) {};
	TestXmlThroughCom();
	TestVariants();
	printf( "com: %d passed, %d failed\n", g_nPassed, g_nFailed );
	return g_nFailed ? 1 : 0;
}
