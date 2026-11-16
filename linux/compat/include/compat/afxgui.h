//
// compat/afxgui.h (Linux compatibility layer)
//
// The Windows interface classes, declared but not defined. Engine headers
// mention them in signatures (a CListCtrl& to fill, a CDC* to draw a status
// bar on); with these declarations those headers compile. Any code that
// actually uses one belongs to the Windows interface and is not built on
// Linux. The geometry value types (CPoint, CSize, CRect) are real, since
// engine code passes them around by value.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include <windows.h>

#define afx_msg

class CWnd; class CFrameWnd; class CMDIFrameWnd; class CMDIChildWnd; class CDialog; class CPropertyPage;
class CPropertySheet; class CControlBar; class CToolBar; class CStatusBar; class CReBar;
class CDC; class CPaintDC; class CClientDC; class CWindowDC; class CGdiObject; class CPen; class CBrush;
class CFont; class CBitmap; class CRgn; class CPalette; class CImageList; class CMenu; class CCmdUI;
class CListCtrl; class CTreeCtrl; class CListBox; class CComboBox; class CEdit; class CRichEditCtrl;
class CButton; class CStatic; class CScrollBar; class CSliderCtrl; class CSpinButtonCtrl; class CProgressCtrl;
class CHeaderCtrl; class CTabCtrl; class CToolTipCtrl; class CDataExchange; class CDocument; class CView;
class CHtmlView; class COleDataObject; class COleDropTarget; class CRecentFileList; class CCommandLineInfo;

typedef HANDLE HCURSOR;
typedef HANDLE HBRUSH;
typedef HANDLE HPEN;
typedef HANDLE HRGN;
typedef HANDLE HIMAGELIST;
typedef HANDLE HTREEITEM;
typedef HANDLE HACCEL;
typedef HANDLE HTHEME;
typedef HANDLE HHOOK;
typedef HANDLE HDROP;
typedef HANDLE HPALETTE;
typedef HANDLE HMONITOR;
typedef HANDLE HKL;

struct NMHDR;
struct tagLOGFONTW;
typedef struct tagLOGFONTW LOGFONT;
struct tagCREATESTRUCTW;
typedef struct tagCREATESTRUCTW* LPCREATESTRUCT;
struct tagNCCALCSIZE_PARAMS;
typedef struct tagNCCALCSIZE_PARAMS NCCALCSIZE_PARAMS;
struct tagDRAWITEMSTRUCT;
typedef struct tagDRAWITEMSTRUCT* LPDRAWITEMSTRUCT;
struct tagMEASUREITEMSTRUCT;
typedef struct tagMEASUREITEMSTRUCT* LPMEASUREITEMSTRUCT;
struct tagTOOLINFOW;
typedef struct tagTOOLINFOW TOOLINFO;

class CPoint : public tagPOINT
{
public:
	CPoint() { x = y = 0; }
	CPoint(int nx, int ny) { x = nx; y = ny; }
	CPoint(POINT p) { x = p.x; y = p.y; }
	bool operator==(const POINT& p) const { return x == p.x && y == p.y; }
	bool operator!=(const POINT& p) const { return ! ( *this == p ); }
	void Offset(int dx, int dy) { x += dx; y += dy; }
	CPoint operator+(POINT p) const { return CPoint( x + p.x, y + p.y ); }
	CPoint operator-(POINT p) const { return CPoint( x - p.x, y - p.y ); }
};

class CSize : public tagSIZE
{
public:
	CSize() { cx = cy = 0; }
	CSize(int x, int y) { cx = x; cy = y; }
	CSize(SIZE s) { cx = s.cx; cy = s.cy; }
	bool operator==(const SIZE& s) const { return cx == s.cx && cy == s.cy; }
	bool operator!=(const SIZE& s) const { return ! ( *this == s ); }
};

class CRect : public tagRECT
{
public:
	CRect() { left = top = right = bottom = 0; }
	CRect(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; }
	CRect(const RECT& r) { left = r.left; top = r.top; right = r.right; bottom = r.bottom; }
	CRect(POINT p, SIZE s) { left = p.x; top = p.y; right = p.x + s.cx; bottom = p.y + s.cy; }
	int Width() const { return right - left; }
	int Height() const { return bottom - top; }
	CSize Size() const { return CSize( Width(), Height() ); }
	CPoint TopLeft() const { return CPoint( left, top ); }
	CPoint BottomRight() const { return CPoint( right, bottom ); }
	CPoint CenterPoint() const { return CPoint( ( left + right ) / 2, ( top + bottom ) / 2 ); }
	BOOL IsRectEmpty() const { return right <= left || bottom <= top; }
	BOOL IsRectNull() const { return left == 0 && top == 0 && right == 0 && bottom == 0; }
	BOOL PtInRect(POINT p) const { return p.x >= left && p.x < right && p.y >= top && p.y < bottom; }
	void SetRect(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; }
	void SetRectEmpty() { left = top = right = bottom = 0; }
	void OffsetRect(int dx, int dy) { left += dx; right += dx; top += dy; bottom += dy; }
	void InflateRect(int dx, int dy) { left -= dx; right += dx; top -= dy; bottom += dy; }
	void DeflateRect(int dx, int dy) { InflateRect( -dx, -dy ); }
	void NormalizeRect()
	{
		if ( left > right ) { LONG t = left; left = right; right = t; }
		if ( top > bottom ) { LONG t = top; top = bottom; bottom = t; }
	}
	operator LPRECT() { return this; }
	operator LPCRECT() const { return this; }
	bool operator==(const RECT& r) const { return left == r.left && top == r.top && right == r.right && bottom == r.bottom; }
	bool operator!=(const RECT& r) const { return ! ( *this == r ); }
};

// The hourglass cursor during long operations: nothing to show without an interface.
class CWaitCursor
{
public:
	CWaitCursor() {}
	void Restore() {}
};

// No main window on Linux.
inline CWnd* AfxGetMainWnd() { return NULL; }
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_RESTORE 9

// Sounds: none without an interface.
#define SND_SYNC        0x0000
#define SND_ASYNC       0x0001
#define SND_NODEFAULT   0x0002
#define SND_LOOP        0x0008
#define SND_NOSTOP      0x0010
#define SND_ALIAS       0x00010000
#define SND_FILENAME    0x00020000
#define SND_APPLICATION 0x0080
inline BOOL PlaySoundW(LPCWSTR, HMODULE, DWORD) { return FALSE; }
#define PlaySound PlaySoundW
