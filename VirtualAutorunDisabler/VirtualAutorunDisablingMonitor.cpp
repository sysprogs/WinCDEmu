// VirtualAutorunDisablingMonitor.cpp : Implementation of CVirtualAutorunDisablingMonitor

#include "stdafx.h"
#include "VirtualAutorunDisablingMonitor.h"
#include "../VirtualCDCtl/VirtualDriveClient.h"

using namespace BazisLib;

// CVirtualAutorunDisablingMonitor


HRESULT STDMETHODCALLTYPE CVirtualAutorunDisablingMonitor::AllowAutoPlay(/* [string][in] */ __RPC__in LPCWSTR pszPath, /* [in] */ DWORD dwContentType, /* [string][in] */ __RPC__in LPCWSTR pszLabel, /* [in] */ DWORD dwSerialNumber)
{
	wchar_t wszPath[] = L"\\\\.\\0:";

	if (GetDriveType(pszPath) != DRIVE_CDROM)
		return S_OK;

	if (!pszPath[0])
		return E_INVALIDARG;
	
	wszPath[4] = pszPath[0];
	ActionStatus status;
	VirtualDriveClient client(wszPath, &status);
	if (!client.IsAutorunSuppressionFlagged())
		return S_OK;

	m_CurrentState = stRunning;
	ResetTimeout();
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CVirtualAutorunDisablingMonitor::InitializeMonitorAndResetWatchdog(void)
{
	if (m_CurrentState == stInit)
		return HRESULT_FROM_WIN32(ERROR_RETRY); //The watchdog thread has not initialized yet. Hold the existing reference and retry attempt.

	if (m_CurrentState == stHardTermination)
		return E_ABORT;	//The current server is terminating. The caller should release the reference, call CoCreateInstance() again and retry.

	if (!SUCCEEDED(m_ROTRegisterResult))
		return m_ROTRegisterResult;

	m_CurrentState = stRunning;
	ResetTimeout();
	return S_OK;
}

HRESULT CVirtualAutorunDisablingMonitor::TerminateServer()
{
	m_CurrentState = stHardTermination;
	PostQuitMessage(0);
	return S_OK;
}

int CVirtualAutorunDisablingMonitor::WatchdogThreadBody(void *pArg)
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	enum {kPollingPeriod = (kTimeoutInSeconds * 1000) / 2};
	CComPtr<IVirtualAutorunDisablingMonitorInternal> pMonitor;
	do
	{
		HRESULT hR;
		if (pMonitor == NULL)
			hR = pMonitor.CoCreateInstance(CLSID_VirtualAutorunDisablingMonitor);
		if (pMonitor == NULL)
			continue;

		hR = pMonitor->CheckForTimeout(GetCurrentProcessId());
		if (hR != S_OK)
			pMonitor = NULL;

	} 	while (!((CVirtualAutorunDisablingMonitor *)pArg)->m_ThreadStopEvent.TryWait(kPollingPeriod));

	return 0;
}

HRESULT STDMETHODCALLTYPE CVirtualAutorunDisablingMonitor::CheckForTimeout(int PID)
{
	if (PID != GetCurrentProcessId())
		return E_INVALIDARG;

	if (m_CurrentState == stInit)
		m_CurrentState = stRunning;

	//Returned S_FALSE means "watchdog thread should release the handle"
	if (m_TimeoutResetTime.MillisecondsElapsed() < (kTimeoutInSeconds * 1000))
		return (m_CurrentState == stRunning) ? S_OK  : S_FALSE;	//Time-out not expired yet

	switch(m_CurrentState)
	{
	case stRunning:
		m_CurrentState = stSoftTermination;
		ResetTimeout();
		return S_FALSE;
	case stSoftTermination:
	default:
		m_CurrentState = stHardTermination;
		TerminateServer();
		return S_FALSE;
	}
}