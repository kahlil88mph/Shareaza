//
// SearchSessions.h (Linux engine host)
//
// On Windows a search lives in its search window (CSearchWnd): the window
// holds the managed searches, the match list, and the rules for when to stop
// asking for more results; Network.cpp hands query hits to the open search
// windows, and the web remote reads results from them. The Linux engine has
// no windows, so a search lives in a CSearchSession instead, kept in the
// SearchSessions list. The session follows CSearchWnd's logic: the same result
// limits, "search more", stop and clear. QuerySearch.h names it CSearchWnd on
// Linux, so CQuerySearch::OpenWindow opens one.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include "MatchObjects.h"
#include "ManagedSearch.h"
#include "QuerySearch.h"

#include <list>

class CQueryHit;

typedef CLocked< CMatchList*, CMutex* > CLockedMatchList;

class CSearchSession : public CMatchListOwner
{
public:
	// Takes the search (it may be NULL for a session loaded from disk).
	explicit CSearchSession(CQuerySearch* pSearch = NULL);
	virtual ~CSearchSession();

	DWORD GetID() const { return m_nID; }

	// What was searched for: the text, the hash or the metadata words, with
	// the schema title in brackets (the window caption without "Search : ").
	CString GetCaption() const;

	// The matches, locked for as long as the returned object lives.
	CLockedMatchList GetMatches() { return CLockedMatchList( m_pMatches, &m_pMatches->m_pSection ); }

	virtual CQuerySearchPtr GetLastSearch() const;

	// Start the latest search (CSearchWnd::ExecuteSearch). FALSE if it is not
	// valid or the adult filter blocks it.
	BOOL Execute();

	// Start a new search in this session; the earlier ones stop (CSearchWnd
	// "Search" with a new query). bClear drops the earlier results.
	BOOL Search(CQuerySearch* pSearch, BOOL bClear);

	// "Search more": resume a search that reached its result limit.
	BOOL More();

	void Stop();	// stop all searches; results stay
	void Clear();	// drop the results and stop

	BOOL IsPaused() const { return m_bPaused; }
	BOOL IsWaitMore() const { return m_bWaitMore; }
	BOOL IsSearching() const { return ! m_bPaused && ! m_bWaitMore; }

	// G2 hubs and leaves searched by the latest search.
	void GetProgress(DWORD& nHubs, DWORD& nLeaves) const;

	// Hits for this session's searches; FALSE if they belong elsewhere.
	BOOL OnQueryHits(const CQueryHit* pHits);

	// Periodic check (the window's timer): pauses a search that has sent its
	// share of G2 queries.
	void OnRun();

	// Same layout as CSearchWnd::Serialize (searches, then the match list).
	void Serialize(CArchive& ar);

	// The window messages the web remote posts to a search window:
	// WM_COMMAND ID_SEARCH_STOP stops it, WM_TIMER (redraw) does nothing.
	BOOL PostMessage(UINT nMsg, WPARAM wParam = 0, LPARAM lParam = 0);

	// When the results last changed (seconds since 1970, 0 if not since saved).
	DWORD GetModified() const { return m_tModify; }

private:
	typedef std::list< CSearchPtr > List;

	void SelectColumns(CSchemaPtr pSchema);
	void SetModified();

	const DWORD	m_nID;
	CMatchList*	m_pMatches;
	List		m_oSearches;
	BOOL		m_bPaused;
	BOOL		m_bWaitMore;
	DWORD		m_tSearch;
	DWORD		m_tModify;
	DWORD		m_nMaxResults;
	DWORD		m_nMaxED2KResults;
	DWORD		m_nMaxQueryCount;

	CSearchSession(const CSearchSession&);
	CSearchSession& operator=(const CSearchSession&);
};

class CSearchSessions
{
public:
	CSearchSessions();
	~CSearchSessions();

	mutable CCriticalSection m_pSection;	// guards the list (lock it to walk it)

	// Open a session for a search and start it (the WM_OPENSEARCH path).
	// NULL if the search is not valid.
	CSearchSession* Open(CQuerySearch* pSearch);

	CSearchSession* Find(DWORD nID) const;
	BOOL Close(DWORD nID);
	void CloseAll();

	POSITION GetIterator() const { return m_pList.GetHeadPosition(); }
	CSearchSession* GetNext(POSITION& pos) const { return m_pList.GetNext( pos ); }
	INT_PTR GetCount() const { return m_pList.GetCount(); }

	// Hand hits to the sessions (Network.cpp's job queue, stage 2).
	BOOL OnQueryHits(const CQueryHit* pHits);

	void OnRun();

	// Data/Searches.dat in the user folder.
	BOOL Load();
	BOOL Save();

private:
	CList< CSearchSession* > m_pList;
};

extern CSearchSessions SearchSessions;
