//
// SearchSessions.cpp (Linux engine host)
//
// Searches without search windows; see SearchSessions.h. The logic follows
// shareaza/WndSearch.cpp (CSearchWnd) and WndBaseMatch.cpp (CBaseMatchWnd)
// step for step, minus the drawing; the save file is WindowManager.cpp's
// Searches.dat, in the same format.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include "StdAfx.h"
#include "Shareaza.h"
#include "Settings.h"
#include "SearchSessions.h"
#include "MatchObjects.h"
#include "ManagedSearch.h"
#include "QuerySearch.h"
#include "QueryHit.h"
#include "Network.h"
#include "Schema.h"
#include "SchemaCache.h"
#include "SchemaMember.h"
#include "Security.h"
#include "XML.h"

#include <algorithm>

CSearchSessions SearchSessions;

namespace {
	volatile LONG g_nNextID = 0;
}

//////////////////////////////////////////////////////////////////////
// CSearchSession

CSearchSession::CSearchSession(CQuerySearch* pSearch)
	: m_nID				( (DWORD)InterlockedIncrement( &g_nNextID ) )
	, m_pMatches		( NULL )
	, m_bPaused			( TRUE )
	, m_bWaitMore		( FALSE )
	, m_tSearch			( 0 )
	, m_tModify			( 0 )
	, m_nMaxResults		( 0 )
	, m_nMaxED2KResults	( 0 )
	, m_nMaxQueryCount	( 0 )
{
	m_pMatches = new CMatchList( this );

	if ( pSearch )
	{
		CQuickLock pLock( m_pMatches->m_pSection );
		m_oSearches.push_back( new CManagedSearch( pSearch ) );
		SelectColumns( pSearch->m_pSchema );
	}
}

CSearchSession::~CSearchSession()
{
	{
		CQuickLock pLock( m_pMatches->m_pSection );
		for ( List::iterator i = m_oSearches.begin(); i != m_oSearches.end(); ++i )
			(*i)->Stop();
		m_oSearches.clear();
	}
	delete m_pMatches;
}

CString CSearchSession::GetCaption() const
{
	CQuickLock pLock( m_pMatches->m_pSection );

	CString strCaption;
	if ( m_oSearches.empty() )
		return strCaption;

	CQuerySearchPtr pSearch = m_oSearches.back()->GetSearch();
	if ( ! pSearch )
		return strCaption;

	if ( pSearch->HasHash() )
		strCaption = _T("Hash ") + pSearch->GetURN();
	else if ( pSearch->m_sSearch.GetLength() )
		strCaption = pSearch->m_sSearch;
	else if ( pSearch->m_pSchema && pSearch->m_pXML )
		strCaption = pSearch->m_pSchema->GetIndexedWords( pSearch->m_pXML->GetFirstElement() );

	if ( pSearch->m_pSchema )
		strCaption += _T(" (") + pSearch->m_pSchema->m_sTitle + _T(")");

	return strCaption;
}

CQuerySearchPtr CSearchSession::GetLastSearch() const
{
	CQuickLock pLock( m_pMatches->m_pSection );

	return m_oSearches.empty() ? CQuerySearchPtr() : m_oSearches.back()->GetSearch();
}

void CSearchSession::GetProgress(DWORD& nHubs, DWORD& nLeaves) const
{
	CQuickLock pLock( m_pMatches->m_pSection );

	nHubs = nLeaves = 0;
	if ( ! m_oSearches.empty() )
	{
		nHubs = m_oSearches.back()->m_nHubs;
		nLeaves = m_oSearches.back()->m_nLeaves;
	}
}

// The metadata columns the window would show (CSchemaColumnsDlg::LoadColumns):
// the user's choice from the settings, else the schema's defaults. The match
// list sorts by them.
void CSearchSession::SelectColumns(CSchemaPtr pSchema)
{
	if ( ! pSchema || m_pMatches->m_nFiles != 0 )
		return;

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

void CSearchSession::SetModified()
{
	if ( ! m_tModify )
		m_tModify = static_cast< DWORD >( time( NULL ) );
}

BOOL CSearchSession::Execute()
{
	if ( ! Network.IsWellConnected() )
		Network.Connect( TRUE );

	CQuickLock pLock( m_pMatches->m_pSection );

	if ( m_oSearches.empty() )
		return FALSE;

	CSearchPtr pManaged = m_oSearches.back();
	CQuerySearchPtr pSearch = pManaged->GetSearch();
	if ( ! pSearch || ! pSearch->m_bAutostart )
		return FALSE;

	if ( ! pSearch->CheckValid() )
	{
		CQuerySearch::SearchHelp();
		return FALSE;
	}
	if ( AdultFilter.IsSearchFiltered( pSearch->m_sKeywords ) )
	{
		theApp.Message( MSG_NOTICE, _T("Search blocked by the adult filter: %s"), (LPCTSTR)pSearch->m_sSearch );
		return FALSE;
	}

	m_bPaused	= FALSE;
	m_tSearch	= GetTickCount();
	m_bWaitMore	= FALSE;

	pManaged->Stop();
	pManaged->Start();

	m_nMaxResults		= m_pMatches->m_nGnutellaHits + Settings.Gnutella.MaxResults;
	m_nMaxED2KResults	= m_pMatches->m_nED2KHits + Settings.eDonkey.MaxResults;
	m_nMaxQueryCount	= pManaged->m_nQueryCount + Settings.Gnutella2.QueryLimit;

	SetModified();
	return TRUE;
}

BOOL CSearchSession::Search(CQuerySearch* pSearch, BOOL bClear)
{
	if ( ! pSearch )
		return FALSE;

	CSearchPtr pManaged = new CManagedSearch( pSearch );
	pManaged->CreateGUID();

	{
		CQuickLock oLock( m_pMatches->m_pSection );

		if ( bClear && m_pMatches->m_nFiles > 0 )
			m_pMatches->Clear();

		SelectColumns( pSearch->m_pSchema );

		std::for_each( m_oSearches.begin(), m_oSearches.end(), std::mem_fun( &CManagedSearch::Stop ) );
		m_oSearches.push_back( pManaged );
	}

	return Execute();
}

BOOL CSearchSession::More()
{
	if ( ! Network.IsWellConnected() )
		Network.Connect( TRUE );

	CQuickLock pLock( m_pMatches->m_pSection );

	if ( m_bPaused || ! m_bWaitMore || m_oSearches.empty() )
		return FALSE;

	CSearchPtr pManaged = m_oSearches.back();
	theApp.Message( MSG_DEBUG, _T("Resuming Search") );
	pManaged->SetActive( TRUE );
	m_bWaitMore = FALSE;

	// Resume G2 search
	m_nMaxResults = m_pMatches->m_nGnutellaHits + Settings.Gnutella.MaxResults;
	m_nMaxQueryCount = pManaged->m_nQueryCount + Settings.Gnutella2.QueryLimit;

	// Resume ED2K search
	m_nMaxED2KResults = m_pMatches->m_nED2KHits + Settings.eDonkey.MaxResults;
	pManaged->m_tLastED2K = GetTickCount();
	pManaged->m_tMoreResults = 0;

	SetModified();
	return TRUE;
}

void CSearchSession::Stop()
{
	CQuickLock pLock( m_pMatches->m_pSection );

	for ( List::iterator i = m_oSearches.begin(); i != m_oSearches.end(); ++i )
	{
		(*i)->Stop();
		(*i)->m_bReceive = FALSE;
	}

	m_bPaused = TRUE;
	SetModified();
}

void CSearchSession::Clear()
{
	{
		CQuickLock pLock( m_pMatches->m_pSection );

		m_pMatches->Clear();
		m_nMaxResults		= 0;
		m_nMaxED2KResults	= 0;
		m_nMaxQueryCount	= 0;
	}
	Stop();
}

BOOL CSearchSession::OnQueryHits(const CQueryHit* pHits)
{
	if ( m_bPaused )
		return FALSE;

	CSingleLock pLock( &m_pMatches->m_pSection );
	if ( ! pLock.Lock( 250 ) || m_bPaused )
		return FALSE;

	for ( List::reverse_iterator pManaged = m_oSearches.rbegin(); pManaged != m_oSearches.rend(); ++pManaged )
	{
		if ( ! (*pManaged)->m_bReceive )
			continue;

		if ( (*pManaged)->IsEqualGUID( pHits->m_oSearchID ) ||	// the hits' GUID matches the search
			 ( ! pHits->m_oSearchID && (*pManaged)->IsLastSearch() ) )	// no GUID, and this is the latest text search
		{
			m_pMatches->AddHits( pHits, (*pManaged)->GetSearch() );
			SetModified();

			if ( m_pMatches->m_nED2KHits >= m_nMaxED2KResults &&
				 (*pManaged)->m_tLastED2K != 0xFFFFFFFF )
			{
				if ( ! (*pManaged)->m_bAllowG2 )	// without G2 the search is done
				{
					m_bWaitMore = TRUE;
					(*pManaged)->SetActive( FALSE );
				}
				(*pManaged)->m_tLastED2K = 0xFFFFFFFF;
				theApp.Message( MSG_DEBUG, _T("ED2K Search Reached Maximum Number of Files") );
			}
#ifndef LAN_MODE
			if ( ! m_bWaitMore && m_pMatches->m_nGnutellaHits >= m_nMaxResults )
			{
				m_bWaitMore = TRUE;
				(*pManaged)->SetActive( FALSE );
				theApp.Message( MSG_DEBUG, _T("Gnutella Search Reached Maximum Number of Files") );
			}
#endif // LAN_MODE
			return TRUE;
		}
	}

	return FALSE;
}

void CSearchSession::OnRun()
{
	CSingleLock pLock( &m_pMatches->m_pSection );
	if ( ! pLock.Lock( 100 ) || m_oSearches.empty() )
		return;

	CSearchPtr pManaged = m_oSearches.back();
	if ( pManaged->IsActive() && pManaged->m_nQueryCount > m_nMaxQueryCount )
	{
		m_bWaitMore = TRUE;
		pManaged->SetActive( FALSE );
		theApp.Message( MSG_DEBUG, _T("Search Reached Maximum Duration") );
	}
}

BOOL CSearchSession::PostMessage(UINT nMsg, WPARAM wParam, LPARAM /*lParam*/)
{
	switch ( nMsg )
	{
	case WM_COMMAND:
		if ( LOWORD( wParam ) == ID_SEARCH_STOP )
		{
			Stop();
			return TRUE;
		}
		return FALSE;
	case WM_TIMER:
		return TRUE;	// a window would redraw; there is nothing to draw
	default:
		return FALSE;
	}
}

void CSearchSession::Serialize(CArchive& ar)
{
	CQuickLock pLock( m_pMatches->m_pSection );

	// CSearchWnd::Serialize
	int nVersion = 1;
	if ( ar.IsStoring() )
	{
		ar << nVersion;
		ar.WriteCount( m_oSearches.size() );
		for ( List::iterator i = m_oSearches.begin(); i != m_oSearches.end(); ++i )
			(*i)->Serialize( ar );
	}
	else
	{
		ar >> nVersion;
		if ( nVersion != 1 ) AfxThrowUserException();

		for ( DWORD_PTR nCount = ar.ReadCount() ; nCount > 0 ; nCount-- )
		{
			CSearchPtr pManaged( new CManagedSearch() );
			pManaged->Serialize( ar );
			m_oSearches.push_back( pManaged );
		}
	}

	// CBaseMatchWnd::Serialize
	m_tModify = 0;
	CString strSchema;
	if ( ar.IsStoring() )
	{
		if ( m_pMatches->m_pSchema )
			strSchema = m_pMatches->m_pSchema->GetURI();
		ar << strSchema;
	}
	else
	{
		m_bPaused = TRUE;	// a loaded search is not running
		m_bWaitMore = FALSE;
		ar >> strSchema;
		SelectColumns( SchemaCache.Get( strSchema ) );
	}

	try
	{
		m_pMatches->Serialize( ar );
	}
	catch ( CException* pException )
	{
		pException->Delete();
		m_pMatches->Clear();
	}
}

//////////////////////////////////////////////////////////////////////
// CSearchSessions

CSearchSessions::CSearchSessions()
{
}

CSearchSessions::~CSearchSessions()
{
	CloseAll();
}

CSearchSession* CSearchSessions::Open(CQuerySearch* pSearch)
{
	if ( ! pSearch || ! pSearch->CheckValid( false ) )
		return NULL;

	CSearchSession* pSession = new CSearchSession( pSearch );
	{
		CQuickLock oLock( m_pSection );
		m_pList.AddTail( pSession );
	}
	pSession->Execute();
	return pSession;
}

CSearchSession* CSearchSessions::Find(DWORD nID) const
{
	CQuickLock oLock( m_pSection );

	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		CSearchSession* pSession = m_pList.GetNext( pos );
		if ( pSession->GetID() == nID )
			return pSession;
	}
	return NULL;
}

BOOL CSearchSessions::Close(DWORD nID)
{
	CQuickLock oLock( m_pSection );

	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		const POSITION posThis = pos;
		CSearchSession* pSession = m_pList.GetNext( pos );
		if ( pSession->GetID() == nID )
		{
			m_pList.RemoveAt( posThis );
			delete pSession;
			return TRUE;
		}
	}
	return FALSE;
}

void CSearchSessions::CloseAll()
{
	CQuickLock oLock( m_pSection );

	while ( ! m_pList.IsEmpty() )
		delete m_pList.RemoveHead();
}

BOOL CSearchSessions::OnQueryHits(const CQueryHit* pHits)
{
	CSingleLock oLock( &m_pSection );
	if ( ! oLock.Lock( 250 ) )
		return FALSE;

	BOOL bHandled = FALSE;
	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
	{
		if ( m_pList.GetNext( pos )->OnQueryHits( pHits ) )
			bHandled = TRUE;
	}
	return bHandled;
}

void CSearchSessions::OnRun()
{
	CSingleLock oLock( &m_pSection );
	if ( ! oLock.Lock( 100 ) )
		return;

	for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
		m_pList.GetNext( pos )->OnRun();
}

BOOL CSearchSessions::Load()
{
	const CString strFile = Settings.General.UserPath + _T("\\Data\\Searches.dat");

	CFile pFile;
	if ( ! pFile.Open( strFile, CFile::modeRead | CFile::shareDenyWrite | CFile::osSequentialScan ) )
		return FALSE;

	CList< CSearchSession* > pLoaded;
	try
	{
		CArchive ar( &pFile, CArchive::load, 262144 );
		while ( ar.ReadCount() == 1 )
		{
			CSearchSession* pSession = new CSearchSession();
			pLoaded.AddTail( pSession );
			pSession->Serialize( ar );
		}
		ar.Close();
		pFile.Close();
	}
	catch ( CException* pException )
	{
		pException->Delete();
		pFile.Abort();
		// A session that failed half way is not kept
		if ( ! pLoaded.IsEmpty() ) delete pLoaded.RemoveTail();
		theApp.Message( MSG_ERROR, _T("Failed to load search windows: %s"), (LPCTSTR)strFile );
	}

	CQuickLock oLock( m_pSection );
	m_pList.AddTail( &pLoaded );
	return TRUE;
}

BOOL CSearchSessions::Save()
{
	const CString strTemp = Settings.General.UserPath + _T("\\Data\\Searches.tmp");
	const CString strFile = Settings.General.UserPath + _T("\\Data\\Searches.dat");

	CQuickLock oLock( m_pSection );

	CFile pFile;
	if ( ! pFile.Open( strTemp, CFile::modeWrite | CFile::modeCreate | CFile::shareExclusive | CFile::osSequentialScan ) )
	{
		theApp.Message( MSG_ERROR, _T("Failed to save search windows: %s"), (LPCTSTR)strTemp );
		return FALSE;
	}

	try
	{
		CArchive ar( &pFile, CArchive::store, 262144 );

		// Only the newest SearchWindowsLimit searches are kept, as on Windows
		DWORD nTotal = 0;
		for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
			if ( m_pList.GetNext( pos )->GetLastSearch() ) ++nTotal;
		DWORD nSkip = ( nTotal > Settings.Interface.SearchWindowsLimit ) ? ( nTotal - Settings.Interface.SearchWindowsLimit ) : 0;

		for ( POSITION pos = m_pList.GetHeadPosition() ; pos ; )
		{
			CSearchSession* pSession = m_pList.GetNext( pos );
			if ( ! pSession->GetLastSearch() )
				continue;
			if ( nSkip )
			{
				--nSkip;
				continue;
			}
			ar.WriteCount( 1 );
			pSession->Serialize( ar );
		}
		ar.WriteCount( 0 );
		ar.Close();
		pFile.Close();
	}
	catch ( CException* pException )
	{
		pException->Delete();
		pFile.Abort();
		DeleteFile( strTemp );
		theApp.Message( MSG_ERROR, _T("Failed to save search windows: %s"), (LPCTSTR)strTemp );
		return FALSE;
	}

	if ( ! MoveFileEx( strTemp, strFile, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING ) )
	{
		DeleteFile( strTemp );
		theApp.Message( MSG_ERROR, _T("Failed to save search windows: %s"), (LPCTSTR)strFile );
		return FALSE;
	}
	return TRUE;
}
