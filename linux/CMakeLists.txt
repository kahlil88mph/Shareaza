# Native Linux build of the Shareaza engine (feasibility spike).
#
# Builds the parts of the engine ported so far against a small Win32/MFC
# compatibility layer (compat/), plus tests. The Windows build (vc143/*.sln)
# is unaffected.
#
#   cmake -S linux -B build-linux -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
#   cmake --build build-linux && ctest --test-dir build-linux

cmake_minimum_required(VERSION 3.20)
project(shareaza-linux CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

get_filename_component(SHAREAZA_SRC "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# Warnings MSVC code trips on GCC that are noise, not bugs.
set(SHAREAZA_COMPAT_WARNINGS -Wno-unknown-pragmas -Wno-multichar -Wno-ignored-pragmas -Wno-register)

# --- compat: Win32/MFC subset on POSIX --------------------------------------
add_library(shareaza-compat STATIC
	compat/src/windows.cpp
	compat/src/cstring.cpp
	compat/src/afxfile.cpp
	compat/src/handles.cpp
	compat/src/ole.cpp
)
target_include_directories(shareaza-compat PUBLIC compat/include)
target_compile_options(shareaza-compat PUBLIC ${SHAREAZA_COMPAT_WARNINGS}
	$<$<CXX_COMPILER_ID:Clang>:-fms-extensions -Wno-microsoft>)
target_link_libraries(shareaza-compat PUBLIC pthread)

# --- HashLib: SHA1, MD4, MD5, Tiger tree, eD2k (portable C++ paths, no asm) --
add_library(hashlib STATIC
	${SHAREAZA_SRC}/HashLib/ED2K.cpp
	${SHAREAZA_SRC}/HashLib/MD4.cpp
	${SHAREAZA_SRC}/HashLib/MD5.cpp
	${SHAREAZA_SRC}/HashLib/SHA.cpp
	${SHAREAZA_SRC}/HashLib/TigerTree.cpp
)
# compat/hashlib forwards "StdAfx.h" to HashLib's stdafx.h (case-sensitive filesystem)
target_include_directories(hashlib PRIVATE compat/hashlib PUBLIC ${SHAREAZA_SRC}/HashLib)
target_compile_definitions(hashlib PRIVATE HASHLIB_EXPORTS)
target_link_libraries(hashlib PUBLIC shareaza-compat)

# --- engine: files from shareaza/ ported so far ---------------------------
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)
set(ENGINE_SOURCES
	Buffer.cpp
	Packet.cpp
	Settings.cpp
	StdAfx.cpp
	Strings.cpp
	XML.cpp
	XMLCOM.cpp
	ZLibWarp.cpp
	Hashes/HashDescriptors.cpp
	Hashes/HashStringConversion.cpp
)
list(TRANSFORM ENGINE_SOURCES PREPEND ${SHAREAZA_SRC}/shareaza/)
list(APPEND ENGINE_SOURCES engine/ShareazaHost.cpp engine/Registry.cpp)

# G2Packet.cpp holds both the G2 wire format and the protocol handlers,
# which reach into the whole network layer. Until that layer is ported,
# build only the wire-format half: everything before the handler section.
set(G2_SRC ${SHAREAZA_SRC}/shareaza/G2Packet.cpp)
set(G2_FORMAT ${CMAKE_CURRENT_BINARY_DIR}/G2PacketFormat.cpp)
file(READ ${G2_SRC} G2_TEXT)
string(FIND "${G2_TEXT}" "CG2Packet::CG2PacketPool CG2Packet::POOL;" G2_BEGIN)
string(FIND "${G2_TEXT}" "// CDatagrams G2UDP packet handler" G2_END)
if(G2_BEGIN EQUAL -1 OR G2_END EQUAL -1)
	message(FATAL_ERROR "G2Packet.cpp markers not found")
endif()
math(EXPR G2_LEN "${G2_END} - ${G2_BEGIN}")
string(SUBSTRING "${G2_TEXT}" ${G2_BEGIN} ${G2_LEN} G2_BODY)
file(WRITE ${G2_FORMAT}.tmp "// Generated from shareaza/G2Packet.cpp (wire format only); see linux/CMakeLists.txt\n"
	"#include \"StdAfx.h\"\n#include \"Shareaza.h\"\n#include \"Settings.h\"\n#include \"Buffer.h\"\n#include \"G1Packet.h\"\n#include \"G2Packet.h\"\n\n//\n${G2_BODY}")
configure_file(${G2_FORMAT}.tmp ${G2_FORMAT} COPYONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${G2_SRC})
list(APPEND ENGINE_SOURCES ${G2_FORMAT})

# Settings needs CSchema's URI constants, which Schema.cpp defines next to
# code that needs the Windows interface (icons). Take just the constants.
set(SCHEMA_URIS ${CMAKE_CURRENT_BINARY_DIR}/SchemaURIs.cpp)
file(READ ${SHAREAZA_SRC}/shareaza/Schema.cpp SCHEMA_TEXT)
string(REPLACE "\r" "" SCHEMA_TEXT "${SCHEMA_TEXT}")
# Copy each "LPCTSTR CSchema::uri... = ...;" line. string(FIND) keeps the
# text a plain string (a list would split it at the semicolons).
set(SCHEMA_URI_TEXT "")
set(SCHEMA_REST "${SCHEMA_TEXT}")
while(TRUE)
	string(REGEX MATCH "\nLPCTSTR[ \t]+CSchema::uri[^\n]*" SCHEMA_LINE "${SCHEMA_REST}")
	if(NOT SCHEMA_LINE)
		break()
	endif()
	string(APPEND SCHEMA_URI_TEXT "${SCHEMA_LINE}")
	string(FIND "${SCHEMA_REST}" "${SCHEMA_LINE}" SCHEMA_AT)
	string(LENGTH "${SCHEMA_LINE}" SCHEMA_LEN)
	math(EXPR SCHEMA_AT "${SCHEMA_AT} + ${SCHEMA_LEN}")
	string(SUBSTRING "${SCHEMA_REST}" ${SCHEMA_AT} -1 SCHEMA_REST)
endwhile()
file(WRITE ${SCHEMA_URIS}.tmp "// Generated from shareaza/Schema.cpp (URI constants only); see linux/CMakeLists.txt\n"
	"#include \"StdAfx.h\"\n#include \"Schema.h\"\n${SCHEMA_URI_TEXT}\n")
configure_file(${SCHEMA_URIS}.tmp ${SCHEMA_URIS} COPYONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SHAREAZA_SRC}/shareaza/Schema.cpp)
list(APPEND ENGINE_SOURCES ${SCHEMA_URIS})

# ShareazaOM.h and its IIDs from Shareaza.idl (MIDL does this on Windows).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(OM_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${OM_DIR})
add_custom_command(
	OUTPUT ${OM_DIR}/ShareazaOM.h ${OM_DIR}/ShareazaOM_i.cpp
	COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/idl2h.py
		${SHAREAZA_SRC}/shareaza/Shareaza.idl ${OM_DIR}/ShareazaOM.h ${OM_DIR}/ShareazaOM_i.cpp
	DEPENDS ${SHAREAZA_SRC}/shareaza/Shareaza.idl ${CMAKE_CURRENT_SOURCE_DIR}/tools/idl2h.py
	COMMENT "Generating ShareazaOM.h from Shareaza.idl")
list(APPEND ENGINE_SOURCES ${OM_DIR}/ShareazaOM_i.cpp ${SHAREAZA_SRC}/shareaza/ComObject.cpp)

add_library(shareaza-engine STATIC ${ENGINE_SOURCES})
target_include_directories(shareaza-engine PUBLIC ${OM_DIR})
target_include_directories(shareaza-engine PUBLIC ${SHAREAZA_SRC}/shareaza)
target_compile_definitions(shareaza-engine PUBLIC NDEBUG)
# The engine was written for MSVC, which parses templates late (no two-phase
# lookup). Clang can do the same, which avoids rewriting hundreds of
# dependent-base lookups; GCC cannot, so the engine needs Clang.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	message(FATAL_ERROR "The engine needs Clang: cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang")
endif()
target_compile_options(shareaza-engine PUBLIC -fdelayed-template-parsing -fms-extensions
	-Wno-deprecated-declarations -Wno-microsoft -Wno-invalid-token-paste -Wno-writable-strings
	-Wno-invalid-offsetof)	# MFC's interface maps use offsetof on non-standard-layout classes, as on Windows
target_link_libraries(shareaza-engine PUBLIC hashlib shareaza-compat ZLIB::ZLIB BZip2::BZip2)

# --- tests ------------------------------------------------------------------
enable_testing()

add_executable(test-compat tests/test_compat.cpp)
target_link_libraries(test-compat PRIVATE shareaza-compat)
add_test(NAME compat COMMAND test-compat)

add_executable(test-afx tests/test_afx.cpp)
target_link_libraries(test-afx PRIVATE shareaza-compat)
add_test(NAME afx COMMAND test-afx)

add_executable(test-g2packet tests/test_g2packet.cpp)
target_link_libraries(test-g2packet PRIVATE shareaza-engine)
add_test(NAME g2packet COMMAND test-g2packet)

add_executable(test-settings tests/test_settings.cpp)
target_link_libraries(test-settings PRIVATE shareaza-engine)
add_test(NAME settings COMMAND test-settings)

add_executable(test-com tests/test_com.cpp)
target_link_libraries(test-com PRIVATE shareaza-engine)
add_test(NAME com COMMAND test-com)

add_executable(test-hashlib tests/test_hashlib.cpp)
target_link_libraries(test-hashlib PRIVATE hashlib)
add_test(NAME hashlib COMMAND test-hashlib)
