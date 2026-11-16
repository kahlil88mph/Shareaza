//
// ImageFile.cpp (Linux engine host): CImageFile without image plugins.
//
// The archive format matches the Windows build (thumbnail caches and
// download files carry images), so stored images still load and save; only
// decoding from image file formats is missing.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include "StdAfx.h"
#include "Shareaza.h"
#include "ImageFile.h"

CImageFile::CImageFile()
	: m_bScanned( FALSE ), m_nWidth( 0 ), m_nHeight( 0 ), m_nComponents( 0 ), m_pImage( NULL ), m_nFlags( 0 )
{
}

CImageFile::~CImageFile()
{
	Clear();
}

void CImageFile::Clear()
{
	delete [] m_pImage;
	m_bScanned = FALSE;
	m_nWidth = m_nHeight = 0;
	m_nComponents = 0;
	m_pImage = NULL;
	m_nFlags = 0;
}

// No image decoders (the Windows build uses plugins for these).
BOOL CImageFile::LoadFromMemory(LPCTSTR, LPCVOID, DWORD, BOOL, BOOL) { Clear(); return FALSE; }
BOOL CImageFile::LoadFromFile(LPCTSTR, BOOL, BOOL) { Clear(); return FALSE; }
BOOL CImageFile::LoadFromURL(LPCTSTR) { Clear(); return FALSE; }
BOOL CImageFile::SaveToMemory(LPCTSTR, int, LPBYTE* ppBuffer, DWORD* pnLength)
{
	if ( ppBuffer ) *ppBuffer = NULL;
	if ( pnLength ) *pnLength = 0;
	return FALSE;
}
BOOL CImageFile::SaveToFile(LPCTSTR, int, DWORD* pnLength) { if ( pnLength ) *pnLength = 0; return FALSE; }
BOOL CImageFile::FitTo(int, int) { return FALSE; }
BOOL CImageFile::Resample(int, int) { return FALSE; }
BOOL CImageFile::EnsureRGB(COLORREF) { return IsLoaded(); }
BOOL CImageFile::SwapRGB() { return FALSE; }

DWORD CImageFile::GetSerialSize() const
{
	if ( ! IsLoaded() ) return 4;
	return 12 + ( ( m_nWidth * m_nComponents + 3 ) & ~3 ) * m_nHeight;
}

// Same layout as ImageFile.cpp on Windows.
void CImageFile::Serialize(CArchive& ar)
{
	if ( ar.IsStoring() )
	{
		if ( ! IsLoaded() )
		{
			ar << (int)0;
			return;
		}
		ar << m_nWidth;
		ar << m_nHeight;
		const DWORD nCompositeValue = ( (DWORD)m_nFlags << 16 ) | m_nComponents;
		ar << nCompositeValue;
		ar.Write( m_pImage, ( ( m_nWidth * m_nComponents + 3 ) & ~3 ) * m_nHeight );
	}
	else
	{
		Clear();
		ar >> m_nWidth;
		if ( ! m_nWidth )
			return;
		ar >> m_nHeight;
		DWORD nCompositeValue;
		ar >> nCompositeValue;
		m_nComponents = nCompositeValue & 0x0000FFFF;
		m_nFlags = (WORD)( nCompositeValue >> 16 );
		const int nPitch = ( m_nWidth * m_nComponents + 3 ) & ~3;
		m_pImage = new BYTE[ nPitch * m_nHeight ];
		ReadArchive( ar, m_pImage, nPitch * m_nHeight );
		m_bScanned = TRUE;
	}
}
