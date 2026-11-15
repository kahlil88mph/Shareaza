//
// handles.cpp (Linux compatibility layer): events, threads and waits.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#include <compat/handles.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_set>
#include <sched.h>

namespace compat {

namespace {
	std::mutex& WaitLock() { static std::mutex m; return m; }
	std::condition_variable& WaitCond() { static std::condition_variable c; return c; }

	// Threads that ResumeThread releases.
	struct SuspendedStart
	{
		std::mutex m;
		std::condition_variable c;
		bool bGo = false;
	};
	std::mutex& GateLock() { static std::mutex m; return m; }
	std::vector< std::pair< ThreadObject*, SuspendedStart* > >& Gates()
	{
		static std::vector< std::pair< ThreadObject*, SuspendedStart* > > v;
		return v;
	}
}

// Live objects, so a stale handle (closed, object freed) is rejected with
// ERROR_INVALID_HANDLE as Windows does instead of being dereferenced.
namespace {
	std::mutex& LiveLock() { static std::mutex m; return m; }
	std::unordered_set< const KernelObject* >& Live() { static std::unordered_set< const KernelObject* > s; return s; }
}

KernelObject::KernelObject() : m_nRef( 1 )
{
	std::lock_guard< std::mutex > l( LiveLock() );
	Live().insert( this );
}

KernelObject::~KernelObject()
{
	std::lock_guard< std::mutex > l( LiveLock() );
	Live().erase( this );
}

KernelObject* FromHandle(HANDLE h)
{
	if ( ! h || h == INVALID_HANDLE_VALUE ) return NULL;
	KernelObject* p = static_cast< KernelObject* >( h );
	std::lock_guard< std::mutex > l( LiveLock() );
	return Live().count( p ) ? p : NULL;
}

void SetEventState(EventObject* p, bool bSignalled, bool bPulse)
{
	{
		std::lock_guard< std::mutex > l( WaitLock() );
		p->m_bSignalled = bSignalled;
	}
	WaitCond().notify_all();
	if ( bPulse )
	{
		// Give current waiters a chance to see the signal, then clear it.
		std::this_thread::yield();
		std::lock_guard< std::mutex > l( WaitLock() );
		p->m_bSignalled = false;
	}
}

HANDLE StartThread(std::function< DWORD() > fn, bool bSuspended, DWORD* pnThreadID)
{
	ThreadObject* pThread = new ThreadObject();
	pThread->AddRef();	// held by the running thread
	SuspendedStart* pSuspend = bSuspended ? new SuspendedStart() : NULL;
	std::mutex mStarted;
	std::condition_variable cStarted;
	bool bStarted = false;
	std::thread t( [=, &mStarted, &cStarted, &bStarted]
	{
		{
			std::lock_guard< std::mutex > l( mStarted );
			pThread->m_nThreadID = GetCurrentThreadId();
			bStarted = true;
		}
		cStarted.notify_all();
		if ( pSuspend )
		{
			{
				std::unique_lock< std::mutex > l( pSuspend->m );
				pSuspend->c.wait( l, [&] { return pSuspend->bGo; } );
			}
			delete pSuspend;
		}
		const DWORD nExit = fn();
		{
			std::lock_guard< std::mutex > l( WaitLock() );
			pThread->m_nExitCode = nExit;
			pThread->m_bExited = true;
		}
		WaitCond().notify_all();
		pThread->Release();
	} );
	t.detach();
	{
		std::unique_lock< std::mutex > l( mStarted );
		cStarted.wait( l, [&] { return bStarted; } );
	}
	if ( pnThreadID ) *pnThreadID = pThread->m_nThreadID;
	if ( pSuspend )
	{
		std::lock_guard< std::mutex > l( GateLock() );
		Gates().emplace_back( pThread, pSuspend );
	}
	return pThread;
}

} // namespace compat

DWORD ResumeThread(HANDLE hThread)
{
	compat::SuspendedStart* pGate = NULL;
	{
		std::lock_guard< std::mutex > l( compat::GateLock() );
		auto& v = compat::Gates();
		for ( auto i = v.begin(); i != v.end(); ++i )
			if ( i->first == hThread ) { pGate = i->second; v.erase( i ); break; }
	}
	if ( ! pGate ) return 0;
	{
		// Notify under the lock: the thread frees the gate once it gets past it.
		std::lock_guard< std::mutex > l( pGate->m );
		pGate->bGo = true;
		pGate->c.notify_all();
	}
	return 1;
}

// --- events -----------------------------------------------------------------

HANDLE CreateEventW(LPSECURITY_ATTRIBUTES, BOOL bManualReset, BOOL bInitialState, LPCWSTR)
{
	return new compat::EventObject( bManualReset != FALSE, bInitialState != FALSE );
}

static compat::EventObject* AsEvent(HANDLE h)
{
	return dynamic_cast< compat::EventObject* >( compat::FromHandle( h ) );
}

BOOL SetEvent(HANDLE h)
{
	compat::EventObject* p = AsEvent( h );
	if ( ! p ) return FALSE;
	compat::SetEventState( p, true );
	return TRUE;
}

BOOL ResetEvent(HANDLE h)
{
	compat::EventObject* p = AsEvent( h );
	if ( ! p ) return FALSE;
	compat::SetEventState( p, false );
	return TRUE;
}

BOOL PulseEvent(HANDLE h)
{
	compat::EventObject* p = AsEvent( h );
	if ( ! p ) return FALSE;
	compat::SetEventState( p, true, true );
	return TRUE;
}

BOOL CloseHandle(HANDLE h)
{
	compat::KernelObject* p = compat::FromHandle( h );
	if ( ! p ) return FALSE;
	p->Release();
	return TRUE;
}

BOOL DuplicateHandle(HANDLE, HANDLE hSource, HANDLE, LPHANDLE lpTarget, DWORD, BOOL, DWORD)
{
	compat::KernelObject* p = compat::FromHandle( hSource );
	if ( ! p || ! lpTarget ) return FALSE;
	p->AddRef();
	*lpTarget = hSource;
	return TRUE;
}

HANDLE GetCurrentProcess() { return (HANDLE)(LONG_PTR)-1; }
HANDLE GetCurrentThread() { return (HANDLE)(LONG_PTR)-2; }

// --- waits ------------------------------------------------------------------

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* pHandles, BOOL bWaitAll, DWORD dwMilliseconds)
{
	if ( nCount == 0 || nCount > MAXIMUM_WAIT_OBJECTS )
	{
		SetLastError( ERROR_INVALID_PARAMETER );
		return WAIT_FAILED;
	}
	std::vector< compat::KernelObject* > oObjects( nCount );
	for ( DWORD i = 0; i < nCount; ++i )
	{
		oObjects[ i ] = compat::FromHandle( pHandles[ i ] );
		if ( ! oObjects[ i ] )
		{
			SetLastError( ERROR_INVALID_HANDLE );
			return WAIT_FAILED;
		}
	}
	DWORD nResult = WAIT_TIMEOUT;
	auto ready = [&]() -> bool
	{
		if ( bWaitAll )
		{
			for ( compat::KernelObject* p : oObjects ) if ( ! p->IsSignalled() ) return false;
			for ( compat::KernelObject* p : oObjects ) p->Acquire();
			nResult = WAIT_OBJECT_0;
			return true;
		}
		for ( DWORD i = 0; i < nCount; ++i )
		{
			if ( oObjects[ i ]->IsSignalled() )
			{
				oObjects[ i ]->Acquire();
				nResult = WAIT_OBJECT_0 + i;
				return true;
			}
		}
		return false;
	};
	std::unique_lock< std::mutex > l( compat::WaitLock() );
	if ( dwMilliseconds == INFINITE )
		compat::WaitCond().wait( l, ready );
	else if ( ! compat::WaitCond().wait_for( l, std::chrono::milliseconds( dwMilliseconds ), ready ) )
		return WAIT_TIMEOUT;
	return nResult;
}

DWORD WaitForSingleObject(HANDLE h, DWORD dwMilliseconds)
{
	return WaitForMultipleObjects( 1, &h, FALSE, dwMilliseconds );
}

DWORD MsgWaitForMultipleObjects(DWORD nCount, const HANDLE* pHandles, BOOL bWaitAll, DWORD dwMilliseconds, DWORD)
{
	if ( nCount == 0 )
	{
		Sleep( dwMilliseconds == INFINITE ? 1000 : dwMilliseconds );
		return WAIT_TIMEOUT;
	}
	return WaitForMultipleObjects( nCount, pHandles, bWaitAll, dwMilliseconds );
}

DWORD MsgWaitForMultipleObjectsEx(DWORD nCount, const HANDLE* pHandles, DWORD dwMilliseconds, DWORD dwWakeMask, DWORD)
{
	return MsgWaitForMultipleObjects( nCount, pHandles, FALSE, dwMilliseconds, dwWakeMask );
}

// --- threads ----------------------------------------------------------------

BOOL GetExitCodeThread(HANDLE h, LPDWORD pnExitCode)
{
	compat::ThreadObject* p = dynamic_cast< compat::ThreadObject* >( compat::FromHandle( h ) );
	if ( ! p || ! pnExitCode ) return FALSE;
	std::lock_guard< std::mutex > l( compat::WaitLock() );
	*pnExitCode = p->m_nExitCode;
	return TRUE;
}

// Linux thread priorities need privileges to raise; the engine's are hints.
BOOL SetThreadPriority(HANDLE, int) { return TRUE; }
int GetThreadPriority(HANDLE) { return THREAD_PRIORITY_NORMAL; }
BOOL SwitchToThread() { sched_yield(); return TRUE; }

// Linux cannot kill a single thread safely; report failure so callers that
// check fall back, as they must on Windows when TerminateThread fails.
BOOL TerminateThread(HANDLE, DWORD) { return FALSE; }

// --- CWinThread ---------------------------------------------------------------

#include <compat/afxcore.h>

BOOL CWinThread::CreateThread(DWORD dwCreateFlags, UINT, LPSECURITY_ATTRIBUTES)
{
	m_hThread = compat::StartThread( [this]() -> DWORD
	{
		DWORD nExit;
		if ( m_pfnThreadProc )
			nExit = ( *m_pfnThreadProc )( m_pThreadParams );
		else if ( InitInstance() )
			nExit = (DWORD)Run();
		else
			nExit = (DWORD)ExitInstance();
		Delete();
		return nExit;
	}, true, &m_nThreadID );	// always start suspended, so m_hThread is set before the thread can end
	if ( m_hThread && ! ( dwCreateFlags & CREATE_SUSPENDED ) )
		::ResumeThread( m_hThread );
	return m_hThread != NULL;
}

CWinThread* AfxBeginThread(AFX_THREADPROC pfnThreadProc, LPVOID pParam, int nPriority, UINT nStackSize,
	DWORD dwCreateFlags, LPSECURITY_ATTRIBUTES lpSecurityAttrs)
{
	CWinThread* pThread = new CWinThread( pfnThreadProc, pParam );
	if ( ! pThread->CreateThread( dwCreateFlags | CREATE_SUSPENDED, nStackSize, lpSecurityAttrs ) )
	{
		delete pThread;
		return NULL;
	}
	pThread->SetThreadPriority( nPriority );
	if ( ! ( dwCreateFlags & CREATE_SUSPENDED ) )
		pThread->ResumeThread();
	return pThread;
}
