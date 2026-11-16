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
| COM object model | `ComObject.cpp`, `XML.cpp`, `XMLCOM.cpp`; `ShareazaOM.h` generated from `Shareaza.idl` | `test-com`: the engine's XML tree used only through its IDL interfaces (interface maps, QueryInterface between interface parts, COM identity, reference counts, BSTR, VARIANT, SAFEARRAY) |
| Compatibility layer | `compat/` | `test-compat`, `test-afx`: printf/scanf semantics, FormatMessage, code pages, CString, MFC and ATL collections, exceptions, locks, events, threads, multi-object waits |
| Files and folders | `compat/` (Win32 file API) | `test-files`: file handles (64-bit seeks, short reads, positioned I/O, file times), moves and copies with progress and cancel, directory listing with Windows wildcards, change notifications (inotify), disk space |
| Strings and file types | `Strings.cpp`, `engine/ShellIcons.cpp` | `test-strings`: IPv4/IPv6 address strings in Windows format, URL and UTF-8 helpers, lower-casing beyond ASCII, MIME types |

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
  - `ole.h`: OLE Automation. Covers BSTR, VARIANT, SAFEARRAY and IDispatch; MFC's CCmdTarget interface maps (`BEGIN_INTERFACE_MAP`, `INTERFACE_PART`, `METHOD_PROLOGUE`); and ATL's CComBSTR, CComVariant and CComQIPtr.
  - `handles.h` also covers Win32 file handles (`CreateFile`, `ReadFile`, `SetFilePointer` and so on) over POSIX descriptors.
- `engine/ShareazaHost.*`: the headless `CShareazaApp`. On Linux, `Shareaza.h` includes this in place of the MFC application class.
- `engine/Registry.cpp`: `CRegistry` over `$XDG_CONFIG_HOME/shareaza/Shareaza.ini`.
- `tools/idl2h.py`: generates `ShareazaOM.h` and the IIDs from `shareaza/Shareaza.idl` at build time (MIDL does this on Windows).
- `tools/syntax-check.py`: compiles every engine file against the layer and reports what stops the rest.
- `tools/guard.py`: wraps Windows-interface code in an upstream file in `#ifdef _WIN32`, the same way every time.
- `engine/ShellIcons.cpp`, `engine/ImageFile.cpp`: Linux versions of two interface classes that also carry engine data. `ShellIcons` provides MIME types, from the freedesktop MIME database. `CImageFile` keeps the archive format but has no image decoders.
- `engine/ShareazaHostFiles.cpp`, `engine/ShareazaHostMessages.cpp`: the file helpers from `Shareaza.cpp` (`OnRename`, `DeleteFileEx` with the freedesktop Trash, delayed deletes) and `PostMainWndMessage`. These join the build once transfers are ported.

## Decisions

- **Integer widths:** `DWORD`/`LONG` stay 32-bit, and `LONGLONG` is `long long`. Plain `long` is 64-bit on Linux, so it is the main bug class to watch (two found so far).
- **`wchar_t`:** 32-bit on Linux, so strings are UTF-32 in memory. CArchive still writes UTF-16LE with MFC's length prefixes, so `.dat` files are interchangeable with the Windows build. Code that indexes tables by character, or writes `sizeof(TCHAR)` bytes to the wire, needs checking (47 sites).
- **`printf`:** Windows `printf` semantics are kept (`%s` wide in wide formats, `%lu` 32-bit, `%I64u`); formats are translated for glibc.
- **Paths:** the engine keeps its `\` paths internally. The compatibility layer converts at the OS boundary: `\` becomes `/`, and a `\\?\` prefix is dropped. File names containing a backslash are therefore not supported.
- **Settings:** stored in an INI file, with sections and names that match the Windows registry.
- **SEH:** `__try`/`__except` become `try`/`catch(...)`. Hardware faults are no longer caught.
- **Out of memory:** `operator new` throws `std::bad_alloc`; on Windows it returns NULL.
- **The interface:** engine code that only shows something (dialogs, progress windows, list rows, colours) is wrapped in `#ifdef _WIN32`. Linux gets a no-op, a log message, or a "nobody to ask" answer. Engine logic that lived in a dialog moves into the engine: magnet links now come from `CShareazaURL::CreateMagnet`, and the chat data types from `ChatTypes.h`.
- **Interface messages:** `PostMainWndMessage` has no window to post to. With no hook installed (`theApp.m_fnPostMessage`), it frees heap payloads (a `new CString`, a `new CShareazaURL`, a path array) rather than leaking them.
- **Folder IDs:** shared folders keep their GUID in the extended attribute `user.shareaza.guid`. On Windows it's the NTFS stream `folder:Shareaza.GUID`.
- **Change notifications:** these use inotify, with a watch on every folder in the tree and on each new folder as it appears.
- **CString through varargs:** MSVC accepts a CString passed straight to `Format`/`Message` because its CString is a single pointer. Clang rejects it, so each one found becomes `(LPCTSTR)str`, which is also correct on Windows.
- **COM:** objects work in process through their C++ interfaces, with MFC's interface maps reproduced. There is no COM runtime or type library, so late-bound `IDispatch::Invoke` returns `E_NOTIMPL`, `CoCreateInstance` fails, and there are no plugins. `OLECHAR` is `wchar_t`, so BSTRs are UTF-32; they never leave the process.

## Pre-existing upstream bugs found (worth fixing for Windows too)

- `HashLib/Utility.hpp`: missing `;` in `for_each_if` (MSVC never parsed it).
- `HashLib/ED2K.cpp`: `&pMD4[ 4 ]` binds a reference past the end.
- `HashLib/TigerTree.cpp`: unaligned `uint64` loads. Fine on x86, but undefined behaviour and a crash risk on ARM.
- `Hashes/CheckingPolicies.hpp`: default arguments call a member function, which is not valid C++.
- Missing `typename` keywords, include case mismatches, and backslashes in `#include` paths.

## Remaining work

Run `linux/tools/syntax-check.py` for current numbers. After the interface
milestone, 102 of the 147 engine files compile cleanly; the others stop on:

1. **Search, host browsing and chat without windows:** results live in the search and browse windows (`CSearchWnd`, `CBrowseHostWnd`), and the web remote reads them from there. Chat messages go to the chat windows. A headless engine needs its own lists of searches, browses and chats, which the windows on Windows and the remote on Linux both use.
   - Files affected: `MatchObjects`, `QuerySearch`, `Network`, `Remote`, `HostBrowser`, `ChatSession`, `ChatCore`.
2. **Network I/O:** `WSAEventSelect`, `WSAAsyncGetHostByName` and `WSAAccept` conditions need replacing with an epoll/poll event layer and a resolver thread. This is the largest and riskiest part.
   - Files affected: `Handshakes`, `Handshake`, `Datagrams`, `Connection`, the neighbour and transfer classes.
3. **TLS:** SChannel and wincrypt (`TlsClient`, `TlsVerify`, `TlsPolicy`) need moving to OpenSSL.
4. **HTTP client:** WinINet (`HttpRequest`, `DiscoveryServices`, `VersionChecker`) needs moving to libcurl.
5. **UPnP:** Windows UPnP COM is replaced by the bundled miniupnpc. Windows Firewall and AMSI integration are dropped.
6. **Resources:** the string table and embedded HTML from `Shareaza.rc`. `LoadString` returns nothing yet, so log messages built from string IDs are empty.
7. **Metadata plugins (COM):** to be replaced by native libraries later. Until then, the built-in extractors still run, and nothing decodes images (no thumbnails or previews).
