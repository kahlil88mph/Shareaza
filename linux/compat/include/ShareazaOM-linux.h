//
// ShareazaOM-linux.h (Linux compatibility layer)
//
// On Windows, MIDL generates ShareazaOM.h from shareaza/Shareaza.idl. The
// engine uses its plain types everywhere; its COM plugin interfaces are
// Windows-only. This header carries those types, with the IDL's values
// (TRISTATE is serialised in .dat files, so the numbers must match).
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

typedef enum { TRI_UNKNOWN = 0, TRI_FALSE = 1, TRI_TRUE = 2 } TRISTATE;
typedef enum { xmlNode, xmlElement, xmlAttribute } SXMLNodeType;
typedef struct { int cbSize, nFlags, nWidth, nHeight, nComponents, nQuality; } IMAGESERVICEDATA;
typedef enum { IMAGESERVICE_SCANONLY = 0x01, IMAGESERVICE_PARTIAL_IN = 0x02, IMAGESERVICE_PARTIAL_OUT = 0x04 } IMAGESERVICEFLAGS;
typedef enum { smsNull, smsOpen, smsPaused, smsPlaying } MediaState;
typedef enum { smaDefault = 0, smzDistort = -1, smzFill = 0, smzOne = 1, smzDouble = 2 } MediaZoom;
typedef enum { guiWindowed, guiTabbed, guiBasic } GuiMode;

// Log message types (cpp_quote in Shareaza.idl)
#define MSG_SEVERITY_MASK		0x000f
#define MSG_ERROR				0x0000
#define MSG_WARNING				0x0001
#define MSG_NOTICE				0x0002
#define MSG_INFO					0x0003
#define MSG_DEBUG				0x0004
#define MSG_FACILITY_MASK		0xff00
#define MSG_FACILITY_DEFAULT		0x0000
#define MSG_FACILITY_SEARCH		0x0100
#define MSG_FACILITY_INCOMING	0x0200
#define MSG_FACILITY_OUTGOING	0x0300
#define MSG_TRAY					0x0010
