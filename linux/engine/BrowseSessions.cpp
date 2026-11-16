//
// BrowseSessions.cpp (Linux engine host)
//
// Host browsing without Browse Host windows; see BrowseSessions.h. The logic
// follows shareaza/WndBrowseHost.cpp (CBrowseHostWnd), minus the drawing.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include "StdAfx.h"
#include "Shareaza.h"
#include "Settings.h"
#include "BrowseSessions.h"
#include "G2Packet.h"
#include "GProfile.h"
#include "HostBrowser.h"
#include "MatchObjects.h"
#include "Schema.h"
#include "SchemaCache.h"
#include "SchemaMember.h"
#include "Transfers.h"

CBrowseSessions BrowseSessions;

namespace {
	volatile LONG g_nNextID = 0;
}

//////////////////////////////////////////////////////////////////////
// CBrowseSession

CBrowseSession::CBrowseSession(PROTOCOLID nProtocol, const SOCKADDR_IN* pAddress, BOOL bMustPush, const Hashes::Guid& oClientID, const CString& sNick)
	: m_nID			( (DWORD)InterlockedIncrement( &g_nNextID ) )
	, m_pMatches	( NULL )
	, m_pBrowser	( NULL )
	, m_bPaused		( TRUE )
	, m_tModify		( 0 )
	, m_pHead		( NULL )
	, m_pPhysical	( NULL )
	, m_pVirtual	( NULL )
{
	m_pMatches = new CMatchList( this );

	// The columns the window would show for the blank schema
	if ( CSchemaPtr pSchema = SchemaCache.Get( Settings.Search.BlankSchemaURI ) )
	{
		CString strMembers = theApp.GetProfileString( _T("Interface"),
			_T("SchemaColumns.") + pSchema->m_sSingular, _T("(EMPTY)") );
		if ( strMembers == _T("(EMPTY)") )
			strMembers = pSchema->m_sDefaultColumns;

		CSchemaMemberList pColumns;
		for ( POSITION pos = pSchema->GetMemberIterator() ; pos ; )
		{
			CSchemaMemberPtr pMember = pSchema->GetNextMember( pos );
			if ( ! pMember->m_bHidden && strMembers.Find( _T("|") + pMember->m_sName + _T("|") ) >= 0 )
				pColumns.AddTail( pMember );
		}
		m_pMatches->SelectSchema( pSchema, &pColumns );
	}

	IN_ADDR oAddress = {};
	if ( pAddress ) oAddress = pAddress->sin_addr;
	m_pBrowser = new CHostBrowser( this, nProtocol, pAddress ? &oAddress : NULL,
		pAddress ? ntohs( pAddress->sin_port ) : 0, bMustPush, oClientID, sNick );
}

CBrowseSession::~CBrowseSession()
{
	{
		// The browser calls back with the transfers lock held: holding it
		// here means no callback is running while the browser goes away.
		CQuickLock oTransfersLock( Transfers.m_pSection );
		m_pBrowser->Stop();
		delete m_pBrowser;
		m_pBrowser = NULL;
	}

	if ( m_pHead ) m_pHead->Release();
	if ( m_pPhysical ) m_pPhysical->Release();
	if ( m_pVirtual ) m_pVirtual->Release();

	delete m_pMatches;
}

CString CBrowseSession::GetCaption() const
{
	CQuickLock oTransfersLock( Transfers.m_pSection );

	CString strCaption, strAddress;
	strAddress.Format( _T("%s:%u"), (LPCTSTR)CString( inet_ntoa( m_pBrowser->m_pAddress ) ), (UINT)m_pBrowser->m_nPort );

	if ( m_pBrowser->m_pProfile != NULL && m_pBrowser->m_pProfile->IsValid() )
		strCaption = m_pBrowser->m_pProfile->GetNick() + _T(" (") + strAddress + _T(")");
	else
		strCaption = strAddress;

	if ( ! m_pBrowser->m_sServer.IsEmpty() )
		strCaption += _T(" - ") + m_pBrowser->m_sServer;

	return strCaption;
}

BOOL CBrowseSession::Browse()
{
	m_pBrowser->Stop();
	m_bPaused = TRUE;

	if ( ! m_pBrowser->Browse() )
		return FALSE;

	{
		CQuickLock pLock( m_pMatches->m_pSection );
		m_pMatches->Clear();
	}
	m_bPaused = FALSE;
	SetModified();
	return TRUE;
}

void CBrowseSession::Stop()
{
	m_pBrowser->Stop();
	m_bPaused = TRUE;
}

void CBrowseSession::Keep(CG2Packet*& pSlot, CG2Packet* pPacket)
{
	if ( pPacket ) pPacket->AddRef();
	if ( pSlot ) pSlot->Release();
	pSlot = pPacket;
	SetModified();
}

void CBrowseSession::UpdateMessages(BOOL /*bActive*/)
{
	SetModified();
}

void CBrowseSession::OnProfileReceived()
{
	if ( ! m_bPaused )
		SetModified();
}

BOOL CBrowseSession::OnQueryHits(const CQueryHit* pHits)
{
	if ( m_bPaused )
		return FALSE;

	CSingleLock pLock( &m_pMatches->m_pSection );
	if ( pLock.Lock( 100 ) && ! m_bPaused )
	{
		m_pMatches->AddHits( pHits );
		SetModified();
		return TRUE;
	}
	return FALSE;
}

void CBrowseSession::OnHeadPacket(CG2Packet* pPacket)
{
	if ( ! m_bPaused ) Keep( m_pHead, pPacket );
}

void CBrowseSession::OnPhysicalTree(CG2Packet* pPacket)
{
	if ( ! m_bPaused ) Keep( m_pPhysical, pPacket );
}

void CBrowseSession::OnVirtualTree(CG2Packet* pPacket)
{
	if ( ! m_bPaused ) Keep( m_pVirtual, pPacket );
}

BOOL CBrowseSession::OnPush(const Hashes::Guid& oClientID, CConnection* pConnection)
{
	return m_pBrowser->OnPush( oClientID, pConnection );
}

BOOL CBrowseSession::OnNewFile(CLibraryFile* pFile)
{
	return m_pBrowser->OnNewFile( pFile );
}

//////////////////////////////////////////////////////////////////////
// CBrowseSessions

CBrowseSessions::CBrowseSessions()
{
}

CBrowseSessions::~CBrowseSessions()
{
	CloseAll();
}

CBrowseSession* CBrowseSessions::Open(PROTOCOLID nProtocol, const SOCKADDR_IN* pAddress, BOOL bMustPush, const Hashes::Guid& oClientID, const CString& sNick)
{
	CBrowseSession* pSession = new CBrowseSession( nProtocol, pAddress, bMustPush, oClientID, sNick );
	{
		CQuickLock oLock( m_pSection );
		m_pList.AddTail( pSession );
	}
	if ( pAddress )
		pSession->Browse();
	return pSession;
}

CBrowseSession* CBrowseSessions::Find(DWORD nID) const
{
	CQuickLock oLock( m_pSection );

	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		CBrowseSession* pSession = m_pList.GetNext( pos );
		if ( pSession->GetID() == nID )
			return pSession;
	}
	return NULL;
}

BOOL CBrowseSessions::Close(DWORD nID)
{
	CQuickLock oLock( m_pSection );

	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		const POSITION posThis = pos;
		CBrowseSession* pSession = m_pList.GetNext( pos );
		if ( pSession->GetID() == nID )
		{
			m_pList.RemoveAt( posThis );
			delete pSession;
			return TRUE;
		}
	}
	return FALSE;
}

void CBrowseSessions::CloseAll()
{
	CQuickLock oLock( m_pSection );

	while ( ! m_pList.IsEmpty() )
		delete m_pList.RemoveHead();
}

BOOL CBrowseSessions::OnPush(const Hashes::Guid& oClientID, CConnection* pConnection)
{
	CSingleLock oLock( &m_pSection );
	if ( ! oLock.Lock( 250 ) )
		return FALSE;

	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		if ( m_pList.GetNext( pos )->OnPush( oClientID, pConnection ) )
			return TRUE;
	}
	return FALSE;
}

BOOL CBrowseSessions::OnNewFile(CLibraryFile* pFile)
{
	CQuickLock oLock( m_pSection );

	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		if ( m_pList.GetNext( pos )->OnNewFile( pFile ) )
			return TRUE;
	}
	return FALSE;
}

CHostBrowser* CBrowseSessions::FindBrowsing(const Hashes::Guid& oClientID, const IN_ADDR& oAddress, WORD nPort) const
{
	CSingleLock oLock( &m_pSection );
	if ( ! oLock.Lock( 1000 ) )
		return NULL;

	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		CHostBrowser* pBrowser = m_pList.GetNext( pos )->GetBrowser();
		if ( pBrowser->IsBrowsing() &&
			( oClientID == pBrowser->m_oClientID ||
			( oAddress.s_addr == pBrowser->m_pAddress.s_addr && nPort == htons( pBrowser->m_nPort ) ) ) )
		{
			return pBrowser;
		}
	}
	return NULL;
}
