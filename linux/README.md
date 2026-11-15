# Native Linux engine (feasibility test)

This directory builds parts of the Shareaza engine as native Linux code, with
no Wine. The aim is a headless engine that serves the same web remote and
JSON API as the Windows build, so Shareaza-Qt can drive it unchanged.

The approach is to keep the engine's own code and swap out what is under it.
A small Win32/MFC/ATL compatibility layer (`compat/`) lets most engine files
compile unchanged. Changes to upstream files stay small and are guarded with
`_WIN32`, so Prev-I's fixes keep merging cleanly.

## Build and test

```bash
cmake -S linux -B build-linux -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
cmake --build build-linux && ctest --test-dir build-linux
```

The engine needs **Clang**. MSVC parses templates late (it skips two-phase
lookup), and the engine relies on that in hundreds of places. Clang's
`-fdelayed-template-parsing -fms-extensions` behaves the same way, and GCC
has no equivalent.

Also needed: zlib, bzip2, Boost headers. `rhash`, if installed, is used to
cross-check the hashes.

## What builds and passes

| Part | Source | Test |
|---|---|---|
| HashLib: SHA1, MD4, MD5, Tiger tree, eD2k | `HashLib/*.cpp`, portable C++ paths (no asm) | `test-hashlib`: published vectors; rhash comparisons from 1 byte to 29 MB; tree save/load; corrupt-block detection |
| Hash types | `Hashes/*` | (through the packet and settings tests) |
| Strings, Buffer, zlib wrapper | `Strings.cpp`, `Buffer.cpp`, `ZLibWarp.cpp`, `StdAfx.cpp` | |
| Packets | `Packet.cpp`; G2 wire format from `G2Packet.cpp` | `test-g2packet`: byte-exact encoding, UTF-8 strings (including non-BMP), 1/2/3-byte lengths, stream parsing, malformed input |
| Settings | `Settings.cpp` over an INI store in place of the registry | `test-settings`: defaults, folder creation, save, reload, clamping |
| Compatibility layer | `compat/` | `test-compat`, `test-afx`: printf/scanf semantics, code pages, CString, collections, exceptions, locks, events, threads, multi-object waits |

All tests also pass under AddressSanitizer and UndefinedBehaviorSanitizer
(with `-fno-sanitize=alignment`; see below).

Two pieces are generated at configure time from upstream files, to avoid
splitting them before their dependencies are ported:
- **G2 wire format:** the first half of `G2Packet.cpp`. The second half, the protocol handlers, needs the network layer.
- **Schema URIs:** the URI constants from `Schema.cpp`. The rest of that file needs interface icons.

## Layout

- `compat/include/windows.h`, `winsock2.h`, `tchar.h`, `intrin.h`: Win32 types with Windows widths, plus time, memory, code pages and `printf` semantics.
- `compat/include/compat/`:
  - `cstring.h`: CString.
  - `afxcore.h`: MFC core (exceptions, locks, CList/CArray/CMap, CWinThread).
  - `handles.h`: waitable HANDLEs.
  - `afxfile.h`: CFile and a CArchive that is byte-compatible with MFC.
  - `atlcoll.h`: ATL collections.
  - `com.h`: minimal IUnknown, CComPtr, CAutoPtr, SEH shim.
- `engine/ShareazaHost.*`: the headless `CShareazaApp`. On Linux, `Shareaza.h` includes this in place of the MFC application class.
- `engine/Registry.cpp`: `CRegistry` over `$XDG_CONFIG_HOME/shareaza/Shareaza.ini`.

## Decisions

- **Integer widths:** `DWORD`/`LONG` stay 32-bit, and `LONGLONG` is `long long`. Plain `long` is 64-bit on Linux, so it is the main bug class to watch (two found so far).
- **`wchar_t`:** 32-bit on Linux, so strings are UTF-32 in memory. CArchive still writes UTF-16LE with MFC's length prefixes, so `.dat` files are interchangeable with the Windows build. Code that indexes tables by character, or writes `sizeof(TCHAR)` bytes to the wire, needs checking (47 sites).
- **`printf`:** Windows `printf` semantics are kept (`%s` wide in wide formats, `%lu` 32-bit, `%I64u`); formats are translated for glibc.
- **Paths:** the engine keeps its `\` paths internally. The compatibility layer converts at the OS boundary: `\` becomes `/`, and a `\\?\` prefix is dropped. File names containing a backslash are therefore not supported.
- **Settings:** stored in an INI file, with sections and names that match the Windows registry.
- **SEH:** `__try`/`__except` become `try`/`catch(...)`. Hardware faults are no longer caught.
- **Out of memory:** `operator new` throws `std::bad_alloc`; on Windows it returns NULL.

## Pre-existing upstream bugs found (worth fixing for Windows too)

- `HashLib/Utility.hpp`: missing `;` in `for_each_if` (MSVC never parsed it).
- `HashLib/ED2K.cpp`: `&pMD4[ 4 ]` binds a reference past the end.
- `HashLib/TigerTree.cpp`: unaligned `uint64` loads. Fine on x86, but undefined behaviour and a crash risk on ARM.
- `Hashes/CheckingPolicies.hpp`: default arguments call a member function, which is not valid C++.
- Missing `typename` keywords, include case mismatches, and backslashes in `#include` paths.

## Remaining work

A syntax check of all 176 engine files against the current layer: 21 compile
cleanly. The other 155 mostly stop on one of these:

1. **COM object model:** `ComObject.h`, `ShareazaFile.h`, `XMLCOM.h`, `Application.h`. This gates 84 files, because the library and download classes derive from COM-automation bases. Plan: a compile-only COM shim, with the automation code guarded.
2. **Interface coupling:** about 30 engine files include window, skin or control headers. Each needs the interface call moved behind the host (as `theApp.Message` and `PostMainWndMessage` already are).
3. **Network I/O:** `WSAEventSelect`, `WSAAsyncGetHostByName` and `WSAAccept` conditions need replacing with an epoll/poll event layer and a resolver thread. This is the largest and riskiest part.
4. **TLS:** SChannel (`TlsClient`, `TlsVerify`, `TlsPolicy`) needs moving to OpenSSL.
5. **HTTP client:** WinINet needs moving to libcurl.
6. **UPnP:** Windows UPnP COM can be replaced by the bundled miniupnpc.
7. **File I/O:** sparse and overlapped files in the transfer and fragment code.
8. **Metadata plugins (COM):** to be replaced by native libraries later. Until then, the library works without metadata extraction.
