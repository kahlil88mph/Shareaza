//
// shareaza-linux.h (Linux compatibility layer)
//
// Stands in for the Windows SDK, MFC and ATL headers that shareaza/StdAfx.h
// includes on Windows.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include <windows.h>
#include <winsock2.h>
#include <intrin.h>
#include <compat/afxcore.h>
#include <compat/atlcoll.h>
#include <compat/com.h>
#include <compat/afxfile.h>

#include <cmath>
#include <tchar.h>
