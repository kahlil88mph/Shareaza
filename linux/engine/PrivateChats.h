//
// PrivateChats.h (Linux engine host)
//
// On Windows a private chat lives in its chat window (CPrivateChatWnd): the
// CChatSession posts messages, user changes and commands to the window, and
// the window finds and reuses sessions (CChatWindows). The Linux engine has
// no windows, so a private chat is a CPrivateChat, kept in the PrivateChats
// list: it takes the same posted messages and keeps the conversation and the
// user list for the host program to show. ChatSession.h names it
// CPrivateChatWnd on Linux.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include "ChatTypes.h"

class CChatSession;

class CPrivateChat
{
public:
	CPrivateChat();
	~CPrivateChat();

	// One line of the conversation. nIndex counts up from 1, so a reader can
	// ask for what came after the last line it saw.
	struct CLine
	{
		DWORD		nIndex;
		time_t		tTime;
		MessageType	nType;		// cmtMessage, cmtAction, cmtStatus, cmtError, cmtInfo, cmtProfile
		bool		bOutgoing;
		CString		sFrom;
		CString		sText;
	};

	DWORD GetID() const { return m_nID; }

	// The window's interface, used by CChatSession (call with ChatCore.m_pSection held)
	BOOL Accept(CChatSession* pSession);
	BOOL PostMessage(UINT nMsg, WPARAM wParam = 0, LPARAM lParam = 0);
	BOOL Find(const SOCKADDR_IN* pAddress) const;
	BOOL Find(const Hashes::Guid& oGUID, bool bLive) const;
	BOOL Find(const CString& sNick) const;

	// For the host program
	void Setup(const Hashes::Guid& oGUID, const SOCKADDR_IN* pHost, BOOL bMustPush, PROTOCOLID nProtocol);
	BOOL Connect();					// ID_CHAT_CONNECT
	void Disconnect();				// ID_CHAT_DISCONNECT
	BOOL Send(const CString& sText, bool bAction = false);
	BOOL IsOnline() const;
	CString GetNick() const;		// the other side's nickname
	CString GetCaption() const;		// the session's caption text (cmtCaption), if any
	void GetLines(DWORD nAfter, CList< CLine >& oLines) const;
	void GetUsers(CChatUser::List& oUsers) const;
	DWORD GetLastIndex() const { return m_nNextIndex - 1; }

private:
	void AddLine(MessageType nType, bool bOutgoing, const CString& sFrom, const CString& sText);
	void OnChatMessage(CChatMessage* pMsg);
	void OnAddUser(CChatUser* pUser);
	void OnDeleteUser(CString* psNick);

	const DWORD		m_nID;
	CChatSession*	m_pSession;
	CString			m_sNick;
	CString			m_sCaption;
	CList< CLine >	m_oLines;
	DWORD			m_nNextIndex;
	CChatUser::List	m_oUsers;

	CPrivateChat(const CPrivateChat&);
	CPrivateChat& operator=(const CPrivateChat&);
};

// The open private chats (CChatWindows for private chats). Lock
// ChatCore.m_pSection to use it, as the chat windows do.
class CPrivateChats
{
public:
	CPrivateChat* Find(DWORD nID) const;
	CPrivateChat* FindPrivate(const Hashes::Guid& oGUID, bool bLive) const;
	CPrivateChat* FindPrivate(const SOCKADDR_IN* pAddress) const;
	CPrivateChat* FindED2KFrame(const SOCKADDR_IN* pAddress) const;
	CPrivateChat* FindED2KFrame(DWORD nClientID, const SOCKADDR_IN* pServerAddress) const;

	// Start a chat with a G1/G2 host, or reopen the chat with it
	// (CChatWindows::OpenPrivate for Gnutella).
	CPrivateChat* OpenPrivate(const Hashes::Guid& oGUID, const SOCKADDR_IN* pHost, BOOL bMustPush = FALSE, PROTOCOLID nProtocol = PROTOCOL_G2);

	BOOL Close(DWORD nID);
	void CloseAll();

	POSITION GetIterator() const { return m_pList.GetHeadPosition(); }
	CPrivateChat* GetNext(POSITION& pos) const { return m_pList.GetNext( pos ); }

private:
	friend class CPrivateChat;
	CList< CPrivateChat* > m_pList;
};

extern CPrivateChats PrivateChats;
extern CPrivateChats& ChatWindows;	// the name the engine's chat code uses
