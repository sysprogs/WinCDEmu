// VirtualAutorunDisablingMonitor.h : Declaration of the CVirtualAutorunDisablingMonitor

#pragma once
#include "resource.h"       // main symbols

#include "VirtualAutorunDisabler_i.h"
#include <bzscore/thread.h>
#include <bzscore/sync.h>
#include <bzscore/datetime.h>

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif



// CVirtualAutorunDisablingMonitor

class ATL_NO_VTABLE CVirtualAutorunDisablingMonitor :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CVirtualAutorunDisablingMonitor, &CLSID_VirtualAutorunDisablingMonitor>,
	public IVirtualAutorunDisablingMonitor,
	public IQueryCancelAutoPlay,
	public IVirtualAutorunDisablingMonitorInternal
{
private:
	static int WatchdogThreadBody(void *pArg);

	enum {kTimeoutInSeconds = 60};

	void ResetTimeout()
	{
		m_TimeoutResetTime = BazisLib::DateTime::Now();
	}

private:
	BazisLib::FunctionThread m_WatchdogThread;
	BazisLib::Win32::Event m_ThreadStopEvent;
	BazisLib::DateTime m_TimeoutResetTime;
	HRESULT m_ROTRegisterResult;
	DWORD m_dwROTCookie;

	enum
	{
		stInit,
		stRunning,
		stSoftTermination,
		stHardTermination,
	} m_CurrentState;
	
public:
	CVirtualAutorunDisablingMonitor()
		: m_WatchdogThread(WatchdogThreadBody, this)
		, m_CurrentState(stInit)
		, m_ROTRegisterResult(E_FAIL)
		, m_dwROTCookie(0)
	{
		ResetTimeout();
	}

DECLARE_REGISTRY_RESOURCEID(IDR_VIRTUALAUTORUNDISABLINGMONITOR)
DECLARE_CLASSFACTORY_SINGLETON(CVirtualAutorunDisablingMonitor);

DECLARE_NOT_AGGREGATABLE(CVirtualAutorunDisablingMonitor)

BEGIN_COM_MAP(CVirtualAutorunDisablingMonitor)
	COM_INTERFACE_ENTRY(IVirtualAutorunDisablingMonitor)
	COM_INTERFACE_ENTRY(IQueryCancelAutoPlay)
	COM_INTERFACE_ENTRY(IVirtualAutorunDisablingMonitorInternal)
END_COM_MAP()


	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		if (!m_WatchdogThread.Start())
			return E_FAIL;

		CComPtr<IRunningObjectTable> pROT;
		GetRunningObjectTable(NULL, &pROT);
		CComPtr<IMoniker> pMoniker;
		CreateClassMoniker(__uuidof(VirtualAutorunDisablingMonitor), &pMoniker);
		if (pROT && pMoniker)
			m_ROTRegisterResult = pROT->Register(ROTFLAGS_ALLOWANYCLIENT, (IQueryCancelAutoPlay *)this, pMoniker, &m_dwROTCookie);
		return S_OK;
	}

	void FinalRelease()
	{
		if (SUCCEEDED(m_ROTRegisterResult))
		{
			CComPtr<IRunningObjectTable> pROT;
			GetRunningObjectTable(NULL, &pROT);
			if (pROT)
				pROT->Revoke(m_dwROTCookie);
		}
		m_ThreadStopEvent.Set();
		if (!m_WatchdogThread.Join(10000))
			m_WatchdogThread.Terminate();
	}

public:
	virtual HRESULT STDMETHODCALLTYPE AllowAutoPlay( 
		/* [string][in] */ __RPC__in LPCWSTR pszPath,
		/* [in] */ DWORD dwContentType,
		/* [string][in] */ __RPC__in LPCWSTR pszLabel,
		/* [in] */ DWORD dwSerialNumber);
		
	virtual HRESULT STDMETHODCALLTYPE InitializeMonitorAndResetWatchdog( void);
	virtual HRESULT STDMETHODCALLTYPE TerminateServer();

	virtual HRESULT STDMETHODCALLTYPE CheckForTimeout(int PID);
};

OBJECT_ENTRY_AUTO(__uuidof(VirtualAutorunDisablingMonitor), CVirtualAutorunDisablingMonitor)
