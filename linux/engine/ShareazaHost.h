//
// ShareazaHost.h (Linux engine host)
//
// On Windows, shareaza/Shareaza.h declares CShareazaApp, the MFC application
// object, which mixes engine services (logging, version, GeoIP, folders) with
// the Windows interface (fonts, windows, hooks, shell). The Linux build has no
// interface of its own, so Shareaza.h includes this header instead: a
// headless CShareazaApp with the same member names the engine code uses.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include "resource.h"
#include "ComObject.h"

#include <functional>

class CBuffer;
class CDatabase;
class CPacket;
class CShareazaFile;

// Packet dump hook: the Windows build shows packets in its Packet window;
// here a host program can install a sink (the engine never needs one).
class CPacketWnd
{
public:
	virtual ~CPacketWnd() {}
	virtual void SmartDump(const CPacket* pPacket, const SOCKADDR_IN* pAddress, BOOL bUDP, BOOL bOutgoing, DWORD_PTR nNeighbourUnique = 0) = 0;
	virtual void SmartDump(const CPacket* pPacket, const SOCKADDR_IN6* pAddress, BOOL bUDP, BOOL bOutgoing, DWORD_PTR nNeighbourUnique = 0) = 0;
};

class CShareazaApp
{
public:
	CShareazaApp();
	~CShareazaApp();

	CMutex				m_pSection;
	WORD				m_nVersion[4];
	BYTE				m_pBTVersion[4];			// SZxx
	CString				m_sVersion;					// x.x.x.x
	CString				m_sVersionLong;
	CString				m_sSmartAgent;				// Shareaza x.x.x.x
	CString				m_sBuildDate;
	CString				m_strBinaryPath;			// engine binary path

	volatile LONG		m_bBusy;
	volatile bool		m_bInteractive;
	volatile bool		m_bLive;
	volatile bool		m_bClosing;
	bool				m_bLimitedConnections;
	// Windows version checks in engine code: Linux counts as a current OS.
	bool				m_bIsServer;
	bool				m_bIsWin2000;
	bool				m_bIsVistaOrNewer;
	bool				m_bIs7OrNewer;
	DWORD				m_nLastInput;

	CPacketWnd*			m_pPacketWnd;				// packet dump sink (NULL - none)

	// Interface notifications (PostMainWndMessage). With no hook installed
	// they are dropped and their heap payloads freed; a hook that returns
	// TRUE takes ownership of the payload. See ShareazaHostMessages.cpp.
	std::function< BOOL(UINT nMsg, WPARAM wParam, LPARAM lParam) > m_fnPostMessage;

	// Log sink: every message the engine logs, after the LogLevel filter.
	// Defaults to stderr.
	std::function< void(WORD nType, const CString& strLog) > m_fnLog;

	bool				IsLogDisabled(WORD nType) const;
	void				Message(WORD nType, UINT nID, ...);
	void				Message(WORD nType, LPCTSTR pszFormat, ...);
	void				PrintMessage(WORD nType, const CString& strLog);

	CString				GetCountryCode(IN_ADDR pAddress) const;
	CString				GetCountryName(IN_ADDR pAddress) const;
	CString				GetCountryCode(IN6_ADDR pAddress) const;
	CString				GetCountryName(IN6_ADDR pAddress) const;

	// XDG locations on Linux, in the engine's '\\' form; see ShareazaHost.cpp.
	// "Program Files" is the folder holding the engine's read-only data
	// (Data, Schemas, Remote): $SHAREAZA_DATA_DIR, else the binary's folder.
	CString				GetProgramFilesFolder() const;
	CString				GetProgramFilesFolder64() const { return GetProgramFilesFolder(); }
	// The system folder sharing must refuse ("C:\\Windows" there): /usr here.
	CString				GetWindowsFolder() const { return L"\\usr"; }
	CString				GetDocumentsFolder() const;
	CString				GetDownloadsFolder() const;
	CString				GetAppDataFolder() const;
	CString				GetLocalAppDataFolder() const;

	CDatabase*			GetDatabase() const;

	// A file is about to be renamed (pszTarget) or deleted (NULL): transfers
	// and the library builder let go of it. In ShareazaHostFiles.cpp.
	void				OnRename(LPCTSTR pszSource, LPCTSTR pszTarget = (LPCTSTR)1);

	// CWinApp's profile functions, over the same store as CRegistry.
	CString				GetProfileString(LPCTSTR pszSection, LPCTSTR pszEntry, LPCTSTR pszDefault = NULL);
	BOOL				WriteProfileString(LPCTSTR pszSection, LPCTSTR pszEntry, LPCTSTR pszValue);
	UINT				GetProfileInt(LPCTSTR pszSection, LPCTSTR pszEntry, int nDefault);
	BOOL				WriteProfileInt(LPCTSTR pszSection, LPCTSTR pszEntry, int nValue);

private:
	CShareazaApp(const CShareazaApp&);
	CShareazaApp& operator=(const CShareazaApp&);
};

extern CShareazaApp theApp;

// Engine-facing helpers that Shareaza.h declares on Windows.
BOOL	LoadString(CString& str, UINT nID);
CString	LoadString(UINT nID);
CString GetErrorString(DWORD dwError = GetLastError());
BOOL	CreateDirectory(LPCTSTR szPath);	// whole path; TRUE if it exists afterwards
// Replace characters Windows forbids in names (as Shareaza.cpp does, so
// downloads get the same names on both systems).
CString SafeFilename(CString strName, bool bPath = false);

// A shared folder's GUID survives renames and moves. Windows keeps it in an
// NTFS stream (folder:Shareaza.GUID); Linux keeps it in the extended
// attribute user.shareaza.guid. Filesystems without user xattrs lose it,
// which only costs a rescan.
bool	LoadGUID(const CString& sFilename, Hashes::Guid& oGUID);
// Delete a file, optionally to the trash (the freedesktop.org Trash in
// $XDG_DATA_HOME/Trash), retrying at the next start if it is busy.
BOOL	DeleteFileEx(LPCTSTR szFileName, BOOL bShared, BOOL bToRecycleBin, BOOL bEnableDelayed);
void	PurgeDeletes();

// Embedded resources (HTML pages and files under /res/ that uploads and the
// web remote serve). The Windows build keeps them in Shareaza.rc; the Linux
// build has none yet, so these report "not found".
bool	ResourceRequest(const CString& strPath, CBuffer& pResponse, CString& sHeader);
CString LoadHTML(HINSTANCE hInstance, UINT nResourceID);
CString LoadRichHTML(UINT nResourceID, CString& strResponse, CShareazaFile* pFile = NULL);
bool	SaveGUID(const CString& sFilename, const Hashes::Guid& oGUID);
BOOL	PostMainWndMessage(UINT Msg, WPARAM wParam = 0, LPARAM lParam = 0);
inline void SafeMessageLoop() {}	// no window message queue on Linux
DWORD	TimeFromString(LPCTSTR psz);
CString	TimeToString(time_t tVal);
BOOL	TimeFromString(LPCTSTR psz, FILETIME* pTime);
CString	TimeToString(FILETIME* pTime);

// Uniform random number in [min, max] from the system CSPRNG (the Windows
// build uses CryptGenRandom).
namespace compat { void RandomBytes(void* p, size_t n); }
template < typename T >
inline T GetRandomNum(const T& min, const T& max)
{
	T nRandom = 0;
	compat::RandomBytes( &nRandom, sizeof( T ) );
	return static_cast< T >( (double)nRandom * ( (double)max - (double)min + 1 ) / ( (double)static_cast< T >( -1 ) + 1 ) + min );
}
template <> __int8 GetRandomNum<__int8>(const __int8& min, const __int8& max);
template <> __int16 GetRandomNum<__int16>(const __int16& min, const __int16& max);
template <> __int32 GetRandomNum<__int32>(const __int32& min, const __int32& max);
template <> __int64 GetRandomNum<__int64>(const __int64& min, const __int64& max);

struct CompareNums
{
	bool operator()(WORD lhs, WORD rhs) const { return lhs > rhs; }
};
