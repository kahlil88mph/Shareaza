//
// BrowseSessions.h (Linux engine host)
//
// On Windows, browsing a host's shared files happens in a Browse Host window
// (CBrowseHostWnd): the window owns the CHostBrowser transfer, keeps the
// files it receives in a match list, and shows the host's profile and folder
// tree. The Linux engine has no windows, so a browse lives in a
// CBrowseSession, kept in the BrowseSessions list. HostBrowser.h names it
// CBrowseHostWnd on Linux: CHostBrowser reports to it through the same calls.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include "MatchObjects.h"
#include "HostBrowser.h"

class CG2Packet;
class CQueryHit;
class CConnection;
class CLibraryFile;

typedef CLocked< CMatchList*, CMutex* > CLockedMatchList;

class CBrowseSession : public CMatchListOwner
{
public:
	// pAddress is in network byte order, as CBrowseHostWnd takes it.
	CBrowseSession(PROTOCOLID nProtocol, const SOCKADDR_IN* pAddress, BOOL bMustPush = FALSE,
		const Hashes::Guid& oClientID = Hashes::Guid(), const CString& sNick = CString());
	virtual ~CBrowseSession();

	DWORD GetID() const { return m_nID; }
	CHostBrowser* GetBrowser() const { return m_pBrowser; }

	// The host: its nickname (from the profile) with address and port, and
	// the ed2k server for a browse through one (the window caption).
	CString GetCaption() const;

	CLockedMatchList GetMatches() { return CLockedMatchList( m_pMatches, &m_pMatches->m_pSection ); }

	// A browse has no search behind it.
	virtual CQuerySearchPtr GetLastSearch() const { return CQuerySearchPtr(); }

	BOOL Browse();	// start or refresh
	void Stop();
	BOOL IsPaused() const { return m_bPaused; }

	// The host's G2 profile head and folder trees, as last received (NULL if
	// none). The packets stay valid while the session holds them; AddRef one
	// to keep it longer.
	CG2Packet* GetHeadPacket() const { return m_pHead; }
	CG2Packet* GetPhysicalTree() const { return m_pPhysical; }
	CG2Packet* GetVirtualTree() const { return m_pVirtual; }

	// When anything changed (seconds since 1970).
	DWORD GetModified() const { return m_tModify; }

	// CHostBrowser's notifications (CBrowseHostWnd's interface)
	void UpdateMessages(BOOL bActive = TRUE);
	void OnProfileReceived();
	BOOL OnQueryHits(const CQueryHit* pHits);
	void OnHeadPacket(CG2Packet* pPacket);
	void OnPhysicalTree(CG2Packet* pPacket);
	void OnVirtualTree(CG2Packet* pPacket);
	BOOL OnPush(const Hashes::Guid& oClientID, CConnection* pConnection);
	BOOL OnNewFile(CLibraryFile* pFile);

private:
	void Keep(CG2Packet*& pSlot, CG2Packet* pPacket);
	void SetModified() { m_tModify = static_cast< DWORD >( time( NULL ) ); }

	const DWORD		m_nID;
	CMatchList*		m_pMatches;
	CHostBrowser*	m_pBrowser;
	BOOL			m_bPaused;
	DWORD			m_tModify;
	CG2Packet*		m_pHead;
	CG2Packet*		m_pPhysical;
	CG2Packet*		m_pVirtual;

	CBrowseSession(const CBrowseSession&);
	CBrowseSession& operator=(const CBrowseSession&);
};

class CBrowseSessions
{
public:
	CBrowseSessions();
	~CBrowseSessions();

	mutable CCriticalSection m_pSection;	// guards the list (lock it to walk it)

	// Open a browse of a host and start it (CBrowseHostWnd's constructor).
	CBrowseSession* Open(PROTOCOLID nProtocol, const SOCKADDR_IN* pAddress, BOOL bMustPush = FALSE,
		const Hashes::Guid& oClientID = Hashes::Guid(), const CString& sNick = CString());

	CBrowseSession* Find(DWORD nID) const;
	BOOL Close(DWORD nID);
	void CloseAll();

	POSITION GetIterator() const { return m_pList.GetHeadPosition(); }
	CBrowseSession* GetNext(POSITION& pos) const { return m_pList.GetNext( pos ); }
	INT_PTR GetCount() const { return m_pList.GetCount(); }

	// A pushed connection for one of the browses (Network::OnPush).
	BOOL OnPush(const Hashes::Guid& oClientID, CConnection* pConnection);

	// A new library file (CLibraryFile::OnNewFile).
	BOOL OnNewFile(CLibraryFile* pFile);

	// The running browse of an ed2k client (CEDClient::GetBrowser).
	CHostBrowser* FindBrowsing(const Hashes::Guid& oClientID, const IN_ADDR& oAddress, WORD nPort) const;

private:
	CList< CBrowseSession* > m_pList;
};

extern CBrowseSessions BrowseSessions;
