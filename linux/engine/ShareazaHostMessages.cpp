//
// ShareazaHostMessages.cpp (Linux engine host): PostMainWndMessage.
//
// On Windows the engine posts these to the main window, which owns some of
// the payloads (a new CString, a new CShareazaURL, a path from Detach()).
// There is no main window here: the host program may install a hook
// (theApp.m_fnPostMessage); otherwise the message is dropped and its payload
// freed, so nothing leaks.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include "StdAfx.h"
#include "Shareaza.h"
#include "ShareazaURL.h"

namespace {
	void FreePayload(UINT nMsg, WPARAM wParam, LPARAM lParam)
	{
		switch ( nMsg )
		{
		case WM_NOWUPLOADING:	// LPARAM: new CString
			delete reinterpret_cast< CString* >( lParam );
			break;
		case WM_URL:			// WPARAM: new CShareazaURL
			delete reinterpret_cast< CShareazaURL* >( wParam );
			break;
		case WM_COLLECTION:		// WPARAM: path from CAutoVectorPtr< TCHAR >::Detach()
		case WM_IMPORT:
		case WM_TORRENT:
			delete [] reinterpret_cast< TCHAR* >( wParam );
			break;
		default:				// no payload, or one the caller keeps (WM_OPENCHAT, WM_TLSPROMPT)
			break;
		}
	}
}

BOOL PostMainWndMessage(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	if ( theApp.m_fnPostMessage && theApp.m_fnPostMessage( nMsg, wParam, lParam ) )
		return TRUE;
	FreePayload( nMsg, wParam, lParam );
	return FALSE;
}
