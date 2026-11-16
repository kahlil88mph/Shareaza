//
// PrivateChats.cpp (Linux engine host)
//
// Private chats without chat windows; see PrivateChats.h. The logic follows
// shareaza/WndPrivateChat.cpp, WndChat.cpp (message handling, spam and adult
// filters) and ChatWindows.cpp (finding a chat to reuse), minus the drawing.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include "StdAfx.h"
#include "Shareaza.h"
#include "Settings.h"
#include "PrivateChats.h"
#include "ChatCore.h"
#include "ChatSession.h"
#include "GProfile.h"
#include "Security.h"

CPrivateChats PrivateChats;
CPrivateChats& ChatWindows = PrivateChats;

namespace {
	volatile LONG g_nNextID = 0;
	const INT_PTR MAX_LINES = 1000;	// lines kept per chat
}

//////////////////////////////////////////////////////////////////////
// CPrivateChat

CPrivateChat::CPrivateChat()
	: m_nID			( (DWORD)InterlockedIncrement( &g_nNextID ) )
	, m_pSession	( NULL )
	, m_nNextIndex	( 1 )
{
	CQuickLock oLock( ChatCore.m_pSection );
	PrivateChats.m_pList.AddTail( this );
}

CPrivateChat::~CPrivateChat()
{
	CQuickLock oLock( ChatCore.m_pSection );

	if ( POSITION pos = PrivateChats.m_pList.Find( this ) )
		PrivateChats.m_pList.RemoveAt( pos );

	// CPrivateChatWnd::OnDestroy
	if ( CChatSession* pSession = m_pSession )
	{
		m_pSession = NULL;
		pSession->OnCloseWindow();
	}
}

BOOL CPrivateChat::Accept(CChatSession* pSession)
{
	if ( m_pSession )
	{
		if ( m_pSession->IsOnline() )
			return FALSE;

		CChatSession* pOld = m_pSession;
		m_pSession = NULL;
		pOld->OnCloseWindow();
	}

	m_pSession = pSession;
	if ( pSession && m_sNick.IsEmpty() )
		m_sNick = pSession->m_sNick;
	return TRUE;
}

BOOL CPrivateChat::Find(const SOCKADDR_IN* pAddress) const
{
	if ( ! m_pSession )
		return FALSE;

	// Regular chat that matches
	return ( m_pSession->m_pHost.sin_addr.s_addr == pAddress->sin_addr.s_addr &&
		m_pSession->m_pHost.sin_port == pAddress->sin_port ) ||
	// ED2K Low ID chat that matches
		( m_pSession->m_bMustPush &&
		  m_pSession->m_nProtocol == PROTOCOL_ED2K &&
		  m_pSession->m_nClientID == pAddress->sin_addr.s_addr );
}

BOOL CPrivateChat::Find(const Hashes::Guid& oGUID, bool bLive) const
{
	if ( m_pSession && validAndEqual( m_pSession->m_oGUID, oGUID ) )
		return ( bLive == m_pSession->IsOnline() );
	return FALSE;
}

BOOL CPrivateChat::Find(const CString& sNick) const
{
	return ( sNick == m_sNick && m_pSession == NULL );
}

void CPrivateChat::Setup(const Hashes::Guid& oGUID, const SOCKADDR_IN* pHost, BOOL bMustPush, PROTOCOLID nProtocol)
{
	CQuickLock oLock( ChatCore.m_pSection );

	ASSERT( m_pSession == NULL );

	m_pSession = new CChatSession( nProtocol, this );
	m_pSession->m_oGUID		= oGUID;
	m_pSession->m_pHost		= *pHost;
	m_pSession->m_bMustPush	= bMustPush;
	m_pSession->m_sNick		= HostToString( pHost );
	m_sNick = m_pSession->m_sNick;
}

BOOL CPrivateChat::Connect()
{
	CQuickLock oLock( ChatCore.m_pSection );

	if ( m_pSession && m_pSession->GetConnectedState() == TRI_FALSE )
		return m_pSession->Connect();
	return FALSE;
}

void CPrivateChat::Disconnect()
{
	CQuickLock oLock( ChatCore.m_pSection );

	if ( m_pSession )
		m_pSession->Close();
}

BOOL CPrivateChat::Send(const CString& sText, bool bAction)
{
	CQuickLock oLock( ChatCore.m_pSection );

	// CPrivateChatWnd::OnLocalMessage
	AddLine( bAction ? cmtAction : cmtMessage, true, MyProfile.GetNick(), sText );

	if ( ! m_pSession )
		return FALSE;

	return m_pSession->SendPrivateMessage( bAction, sText );
}

BOOL CPrivateChat::IsOnline() const
{
	CQuickLock oLock( ChatCore.m_pSection );

	return m_pSession && m_pSession->IsOnline();
}

CString CPrivateChat::GetNick() const
{
	CQuickLock oLock( ChatCore.m_pSection );

	return m_sNick;
}

CString CPrivateChat::GetCaption() const
{
	CQuickLock oLock( ChatCore.m_pSection );

	return m_sCaption;
}

void CPrivateChat::GetLines(DWORD nAfter, CList< CLine >& oLines) const
{
	CQuickLock oLock( ChatCore.m_pSection );

	for ( POSITION pos = m_oLines.GetHeadPosition() ; pos ; )
	{
		const CLine& oLine = m_oLines.GetNext( pos );
		if ( oLine.nIndex > nAfter )
			oLines.AddTail( oLine );
	}
}

void CPrivateChat::GetUsers(CChatUser::List& oUsers) const
{
	CQuickLock oLock( ChatCore.m_pSection );

	oUsers.AddTail( const_cast< CChatUser::List* >( &m_oUsers ) );
}

void CPrivateChat::AddLine(MessageType nType, bool bOutgoing, const CString& sFrom, const CString& sText)
{
	CLine oLine;
	oLine.nIndex	= m_nNextIndex++;
	oLine.tTime		= time( NULL );
	oLine.nType		= nType;
	oLine.bOutgoing	= bOutgoing;
	oLine.sFrom		= sFrom;
	oLine.sText		= sText;
	m_oLines.AddTail( oLine );

	while ( m_oLines.GetCount() > MAX_LINES )
		m_oLines.RemoveHead();
}

BOOL CPrivateChat::PostMessage(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	switch ( nMsg )
	{
	case WM_CHAT_MESSAGE:
		OnChatMessage( reinterpret_cast< CChatMessage* >( lParam ) );
		return TRUE;

	case WM_CHAT_ADD_USER:
		OnAddUser( reinterpret_cast< CChatUser* >( lParam ) );
		return TRUE;

	case WM_CHAT_DELETE_USER:
		OnDeleteUser( reinterpret_cast< CString* >( lParam ) );
		return TRUE;

	case WM_COMMAND:
		switch ( LOWORD( wParam ) )
		{
		case ID_CHAT_CONNECT:
			Connect();
			return TRUE;
		case ID_CHAT_DISCONNECT:
			Disconnect();
			return TRUE;
		}
		return FALSE;

	default:
		return FALSE;
	}
}

// CChatWnd::OnChatMessage and OnMessage
void CPrivateChat::OnChatMessage(CChatMessage* pMsg)
{
	CAutoPtr< CChatMessage > pOwned( pMsg );
	if ( ! pMsg )
		return;

	switch ( pMsg->m_bType )
	{
	case cmtProfile:
		AddLine( cmtProfile, false, pMsg->m_sFrom, CString() );
		m_sNick = pMsg->m_sFrom;
		break;

	case cmtError:
	case cmtStatus:
	case cmtInfo:
		if ( ! pMsg->m_sMessage.IsEmpty() )
			AddLine( pMsg->m_bType, false, CString(), pMsg->m_sMessage );
		break;

	case cmtMessage:
	case cmtAction:
		{
			// Incoming spam filter, then the adult filter (if enabled)
			if ( MessageFilter.IsFiltered( pMsg->m_sMessage ) )
				break;
			CString sText( pMsg->m_sMessage );
			if ( Settings.Community.ChatCensor )
				AdultFilter.Censor( sText );
			AddLine( pMsg->m_bType, false, pMsg->m_sFrom, sText );
		}
		break;

	case cmtCaption:
		m_sCaption = pMsg->m_sMessage;
		break;

	default:
		break;
	}
}

// CChatWnd::OnChatAddUser
void CPrivateChat::OnAddUser(CChatUser* pUser)
{
	CAutoPtr< CChatUser > pOwned( pUser );
	if ( ! pUser )
		return;

	for ( POSITION pos = m_oUsers.GetHeadPosition() ; pos ; )
	{
		CChatUser& oCurrent = m_oUsers.GetNext( pos );
		if ( oCurrent.m_sNick == pUser->m_sNick && oCurrent.m_bType == pUser->m_bType )
		{
			oCurrent.m_sDescription = pUser->m_sDescription;
			return;
		}
	}
	m_oUsers.AddHead( *pUser );
}

// CChatWnd::OnChatDeleteUser: NULL deletes everyone
void CPrivateChat::OnDeleteUser(CString* psNick)
{
	CAutoPtr< CString > pOwned( psNick );

	for ( POSITION pos = m_oUsers.GetHeadPosition() ; pos ; )
	{
		const POSITION posThis = pos;
		const CChatUser& oCurrent = m_oUsers.GetNext( pos );
		if ( ! psNick || ( oCurrent.m_sNick == *psNick && oCurrent.m_bType != cutMe ) )
			m_oUsers.RemoveAt( posThis );
	}
}

//////////////////////////////////////////////////////////////////////
// CPrivateChats (CChatWindows lookups)

CPrivateChat* CPrivateChats::Find(DWORD nID) const
{
	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		CPrivateChat* pChat = m_pList.GetNext( pos );
		if ( pChat->GetID() == nID )
			return pChat;
	}
	return NULL;
}

CPrivateChat* CPrivateChats::FindPrivate(const Hashes::Guid& oGUID, bool bLive) const
{
	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		CPrivateChat* pChat = m_pList.GetNext( pos );
		if ( pChat->Find( oGUID, bLive ) )
			return pChat;
	}
	return NULL;
}

CPrivateChat* CPrivateChats::FindPrivate(const SOCKADDR_IN* pAddress) const
{
	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		CPrivateChat* pChat = m_pList.GetNext( pos );
		if ( pChat->Find( pAddress ) )
			return pChat;
	}
	return NULL;
}

CPrivateChat* CPrivateChats::FindED2KFrame(const SOCKADDR_IN* pAddress) const
{
	// For High ID clients
	CString strHighID;
	strHighID.Format( _T("%s:%hu"), (LPCTSTR)CString( inet_ntoa( pAddress->sin_addr ) ), ntohs( pAddress->sin_port ) );

	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		CPrivateChat* pChat = m_pList.GetNext( pos );
		if ( pChat->Find( strHighID ) )
			return pChat;
	}
	return NULL;
}

CPrivateChat* CPrivateChats::FindED2KFrame(DWORD nClientID, const SOCKADDR_IN* pServerAddress) const
{
	// For Low ID clients
	if ( nClientID == 0 || nClientID >= 16777216 )
		return NULL;

	// As ChatWindows.cpp builds it (the port stays in network order there)
	CString strLowID;
	strLowID.Format( _T("%u@%s:%hu"), nClientID,
		(LPCTSTR)CString( inet_ntoa( pServerAddress->sin_addr ) ), pServerAddress->sin_port );

	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		CPrivateChat* pChat = m_pList.GetNext( pos );
		if ( pChat->Find( strLowID ) )
			return pChat;
	}
	return NULL;
}

CPrivateChat* CPrivateChats::OpenPrivate(const Hashes::Guid& oGUID, const SOCKADDR_IN* pHost, BOOL bMustPush, PROTOCOLID nProtocol)
{
	CQuickLock oLock( ChatCore.m_pSection );

	// CChatWindows::OpenPrivateGnutella
	CPrivateChat* pChat = NULL;
	if ( oGUID )
	{
		pChat = FindPrivate( oGUID, false );
		if ( pChat == NULL ) pChat = FindPrivate( oGUID, true );
	}
	if ( pChat == NULL ) pChat = FindPrivate( pHost );

	if ( pChat == NULL )
	{
		pChat = new CPrivateChat();
		pChat->Setup( oGUID, pHost, bMustPush, nProtocol );
	}

	pChat->Connect();
	return pChat;
}

BOOL CPrivateChats::Close(DWORD nID)
{
	CQuickLock oLock( ChatCore.m_pSection );

	if ( CPrivateChat* pChat = Find( nID ) )
	{
		delete pChat;
		return TRUE;
	}
	return FALSE;
}

void CPrivateChats::CloseAll()
{
	CQuickLock oLock( ChatCore.m_pSection );

	while ( ! m_pList.IsEmpty() )
		delete m_pList.GetHead();	// the destructor takes it off the list
}
