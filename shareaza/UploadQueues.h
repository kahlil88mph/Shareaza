//
// UploadQueues.h
//
// Copyright (c) Shareaza Development Team, 2002-2010.
// This file is part of SHAREAZA (shareaza.sourceforge.net)
//
// Shareaza is free software; you can redistribute it
// and/or modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Shareaza is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Shareaza; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#pragma once

class CUploadQueue;
class CUploadTransfer;
class CLibraryFile;
class CDownload;
class CUploadFile;

// Which uploads the list shows (Settings.Uploads.FilterMask)
#define ULF_ACTIVE		0x01
#define ULF_QUEUED		0x02
#define ULF_HISTORY		0x04
#define ULF_TORRENT		0x08

#define ULF_ALL			(ULF_ACTIVE|ULF_QUEUED|ULF_HISTORY|ULF_TORRENT)


class CUploadQueues
{
public:
	CUploadQueues();
	virtual ~CUploadQueues();

public:
	mutable CMutexEx	m_pSection;
	CUploadQueue*		m_pTorrentQueue;
	CUploadQueue*		m_pHistoryQueue;

	// The uploads list as the interface shows it (Settings.Uploads.FilterMask):
	// the torrent pseudo-queue, the real queues, then the history pseudo-queue,
	// and the files in each. Queue walks need m_pSection, file walks
	// Transfers.m_pSection. *pnPosition: -1 none, 0 active, n queued at n.
	static POSITION			GetFilterIterator();
	static CUploadQueue*	GetNextFiltered(POSITION& pos);
	static POSITION			GetFilteredFileIterator(CUploadQueue* pQueue);
	static CUploadFile*		GetNextFilteredFile(CUploadQueue* pQueue, POSITION& pos, int* pnPosition = NULL);

	BOOL	Enqueue(CUploadTransfer* pUpload, BOOL bForce = FALSE);
	BOOL	Dequeue(CUploadTransfer* pUpload);
	int		GetPosition(CUploadTransfer* pUpload, BOOL bStart);
	BOOL	StealPosition(CUploadTransfer* pTarget, CUploadTransfer* pSource);

	CUploadQueue*	Create(LPCTSTR pszName = NULL, BOOL bTop = FALSE);
	void			Delete(CUploadQueue* pQueue);
	BOOL			Reorder(CUploadQueue* pQueue, CUploadQueue* pBefore);
	CUploadQueue*	SelectQueue(PROTOCOLID nProtocol, CLibraryFile const * const pFile);
	CUploadQueue*	SelectQueue(PROTOCOLID nProtocol, CDownload const * const pFile);
	CUploadQueue*	SelectQueue(PROTOCOLID nProtocol, LPCTSTR pszName, QWORD nSize, DWORD nFileState, LPCTSTR pszShareTags = NULL);

	DWORD	GetTotalBandwidthPoints(BOOL ActiveOnly = FALSE);
//	DWORD	GetQueueCapacity();
//	DWORD	GetQueuedCount();
//	DWORD	GetQueueRemaining();
//	DWORD	GetTransferCount();
	BOOL	IsTransferAvailable();
	DWORD	GetMinimumDonkeyBandwidth();
	DWORD	GetCurrentDonkeyBandwidth();
	BOOL	CanUpload(PROTOCOLID nProtocol, CLibraryFile const * const pFile, BOOL bCanQueue = FALSE );	// Can this file be uploaded with the current queue setup?

	void	Clear();
	BOOL	Load();
	BOOL	Save();
	void	CreateDefault();
	void	Validate();

// Inline Access
	inline POSITION GetIterator() const
	{
		ASSUME_LOCK( m_pSection );
		return m_pList.GetHeadPosition();
	}

	inline CUploadQueue* GetNext(POSITION& pos) const
	{
		ASSUME_LOCK( m_pSection );
		return m_pList.GetNext( pos );
	}

	inline INT_PTR GetCount() const
	{
		CQuickLock oLock( m_pSection );
		return m_pList.GetCount();
	}

	inline bool Check(CUploadQueue* pQueue) const
	{
		CQuickLock oLock( m_pSection );
		if ( pQueue == NULL ) return FALSE;
		return m_pList.Find( pQueue ) != NULL;
	}

	inline BOOL	IsDonkeyRatioActive() const 
	{ 
		return ( m_bDonkeyLimited ); 
	}

protected:
	CList< CUploadQueue* >	m_pList;
	BOOL					m_bDonkeyLimited;

	void	Serialize(CArchive& ar);
};

extern CUploadQueues UploadQueues;
