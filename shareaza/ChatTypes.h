//
// ChatTypes.h
//
// Copyright (c) Shareaza Development Team, 2010-2014.
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

// Chat data shared by the chat protocols and the chat windows.

enum MessageType
{
	cmtNull,
	cmtMessage,	// Chat message
	cmtAction,	// Chat action
	cmtStatus,	// Regular status message (gray)
	cmtError,	// Error message (red)
	cmtInfo,	// Informational message (black)
	cmtProfile,	// Profile received
	cmtCaption	// Set chat window caption
};

enum UserType
{
	cutMe,
	cutUser,
	cutOperator
};


class CChatMessage
{
public:
	inline CChatMessage(MessageType bType = cmtNull, const CString& sFrom = CString(), const CString& sMessage = CString(), HBITMAP hBitmap = NULL)
		: m_bType( bType ), m_sFrom( sFrom ), m_sMessage( sMessage ), m_hBitmap( hBitmap ) {}

	inline CChatMessage(const CChatMessage& pMsg)
		: m_bType( pMsg.m_bType ), m_sFrom( pMsg.m_sFrom ), m_sMessage( pMsg.m_sMessage ), m_hBitmap( pMsg.m_hBitmap ) {}

	inline CChatMessage& operator=(const CChatMessage& pMsg)
		{ m_bType = pMsg.m_bType; m_sFrom = pMsg.m_sFrom; m_sMessage = pMsg.m_sMessage; m_hBitmap = pMsg.m_hBitmap; return *this; }

	MessageType	m_bType;
	CString		m_sFrom;
	CString		m_sMessage;
	HBITMAP		m_hBitmap;
};


class CChatUser
{
public:
	inline CChatUser(UserType bType = cutUser, const CString& sNick = CString(), const CString& sDescription = CString())
		: m_bType( bType ), m_sNick( sNick ), m_sDescription( sDescription ) {}

	inline CChatUser(const CChatUser& pUser)
		: m_bType( pUser.m_bType ), m_sNick( pUser.m_sNick ), m_sDescription( pUser.m_sDescription ), m_sUserAgent( pUser.m_sUserAgent ) {}

	inline CChatUser& operator=(const CChatUser& pUser)
		{ m_bType = pUser.m_bType; m_sNick = pUser.m_sNick; m_sDescription = pUser.m_sDescription; m_sUserAgent = pUser.m_sUserAgent; return *this;}

	typedef CMap< CString, const CString&, CChatUser*, CChatUser* > Map;
	typedef CList< CChatUser > List;

	UserType	m_bType;
	CString		m_sNick;
	CString		m_sDescription;
	CString		m_sUserAgent;
};
