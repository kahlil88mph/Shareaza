//
// compat/handles.h (Linux compatibility layer)
//
// Waitable kernel objects: events and threads behind HANDLEs, with
// WaitForSingleObject / WaitForMultipleObjects semantics (auto-reset events
// are consumed by the waiter that wakes; WaitAll is atomic).
//
// All waitable state changes go through one process-wide mutex and condition
// variable. That keeps multi-object waits simple and correct; the engine has
// tens of threads, not thousands, so contention is not a concern.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include <windows.h>
#include <atomic>
#include <functional>

#define WAIT_OBJECT_0    0x00000000u
#define WAIT_ABANDONED_0 0x00000080u
#define WAIT_TIMEOUT     0x00000102u
#define WAIT_FAILED      0xFFFFFFFFu
#define MAXIMUM_WAIT_OBJECTS 64
#define QS_ALLINPUT       0x04FFu
#define QS_ALLPOSTMESSAGE 0x0100u
#define MWMO_INPUTAVAILABLE 0x0004u
#define STILL_ACTIVE      259u
#define CREATE_SUSPENDED  0x00000004u
#define THREAD_PRIORITY_IDLE          (-15)
#define THREAD_PRIORITY_LOWEST        (-2)
#define THREAD_PRIORITY_BELOW_NORMAL  (-1)
#define THREAD_PRIORITY_NORMAL        0
#define THREAD_PRIORITY_ABOVE_NORMAL  1
#define THREAD_PRIORITY_HIGHEST       2
#define THREAD_PRIORITY_TIME_CRITICAL 15
#define DUPLICATE_SAME_ACCESS 0x00000002u

typedef struct _SECURITY_ATTRIBUTES {
	DWORD  nLength;
	LPVOID lpSecurityDescriptor;
	BOOL   bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

namespace compat {

// Base of everything a HANDLE can refer to. Reference counted: CloseHandle
// drops one reference, and a running thread holds its own.
class KernelObject
{
public:
	KernelObject();
	virtual ~KernelObject();
	void AddRef() { ++m_nRef; }
	void Release() { if ( --m_nRef == 0 ) delete this; }
	// Called with the global wait lock held.
	virtual bool IsSignalled() const = 0;
	virtual void Acquire() {}	// a satisfied wait consumes the signal (auto-reset events)
private:
	std::atomic< int > m_nRef;
};

class EventObject : public KernelObject
{
public:
	EventObject(bool bManual, bool bInitial) : m_bManual( bManual ), m_bSignalled( bInitial ) {}
	bool IsSignalled() const override { return m_bSignalled; }
	void Acquire() override { if ( ! m_bManual ) m_bSignalled = false; }
	bool m_bManual;
	bool m_bSignalled;
};

class ThreadObject : public KernelObject
{
public:
	ThreadObject() : m_bExited( false ), m_nExitCode( STILL_ACTIVE ), m_nThreadID( 0 ) {}
	bool IsSignalled() const override { return m_bExited; }
	bool m_bExited;
	DWORD m_nExitCode;
	DWORD m_nThreadID;
};

void SetEventState(EventObject* p, bool bSignalled, bool bPulse = false);
KernelObject* FromHandle(HANDLE h);

// Start a thread running fn; returns its handle (released by CloseHandle).
HANDLE StartThread(std::function< DWORD() > fn, bool bSuspended, DWORD* pnThreadID);

} // namespace compat

HANDLE CreateEventW(LPSECURITY_ATTRIBUTES, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName);
#define CreateEvent CreateEventW
BOOL SetEvent(HANDLE hEvent);
BOOL ResetEvent(HANDLE hEvent);
BOOL PulseEvent(HANDLE hEvent);
BOOL CloseHandle(HANDLE hObject);
BOOL DuplicateHandle(HANDLE, HANDLE hSource, HANDLE, LPHANDLE lpTarget, DWORD, BOOL, DWORD);
HANDLE GetCurrentProcess();
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds);
// No window message queues on Linux: these wait on the handles only.
DWORD MsgWaitForMultipleObjects(DWORD nCount, const HANDLE* pHandles, BOOL bWaitAll, DWORD dwMilliseconds, DWORD dwWakeMask);
DWORD MsgWaitForMultipleObjectsEx(DWORD nCount, const HANDLE* pHandles, DWORD dwMilliseconds, DWORD dwWakeMask, DWORD dwFlags);
BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode);
DWORD ResumeThread(HANDLE hThread);
BOOL SetThreadPriority(HANDLE hThread, int nPriority);
int GetThreadPriority(HANDLE hThread);
BOOL SwitchToThread();
BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode);
HANDLE GetCurrentThread();
