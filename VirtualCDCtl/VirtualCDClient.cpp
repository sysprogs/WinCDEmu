#include "stdafx.h"

#include "../BazisVirtualCDBus/DeviceControl.h"

#include "VirtualCDClient.h"
#include <bzscore/buffer.h>
#include <bzshlp/Win32/bzsdev.h>
#include <atlcomcli.h>
#include "../VirtualAutorunDisabler/VirtualAutorunDisabler_i.h"
#include <bzscore/datetime.h>
#include "VirtualDriveClient.h"

#pragma comment(lib, "rpcrt4")
#pragma comment(lib, "newdev")

using namespace BazisLib;
using namespace std;

static bool IsObjectRestartingRequired()
{
	CComPtr<IRunningObjectTable> pROT;
	HRESULT hR = GetRunningObjectTable(0, &pROT);
	if (!SUCCEEDED(hR) || !pROT)
		return false;	//Some other error not related to our class

	CComPtr<IMoniker> pMoniker;
	hR = CreateClassMoniker(__uuidof(VirtualAutorunDisablingMonitor), &pMoniker);
	if (!SUCCEEDED(hR) || !pMoniker)
		return false;

	CComPtr<IUnknown> pUnk;
	hR = pROT->GetObject(pMoniker, &pUnk);
	if (!pUnk)
		return true;	//The object is present, but cannot be retrieved via ROT
	return false;
}

static ActionStatus StartAutorunDisablingMonitor()
{
	HRESULT hR = CoInitialize(NULL);
	if (!SUCCEEDED(hR))
		return MAKE_STATUS(ActionStatus::FromHResult(hR));

	enum {kTimeout = 300};

	CComPtr<IVirtualAutorunDisablingMonitor> pMonitor;
	for (DateTime startTime = DateTime::Now(); startTime.MillisecondsElapsed() < kTimeout; Sleep(10))
	{
		if (!pMonitor)
		{
			hR = pMonitor.CoCreateInstance(__uuidof(VirtualAutorunDisablingMonitor));

			if (hR == REGDB_E_CLASSNOTREG)
			{
				CoUninitialize();
				return MAKE_STATUS(ActionStatus::FromHResult(hR));
			}

			if (!SUCCEEDED(hR))
				continue;
		}

		hR = pMonitor->InitializeMonitorAndResetWatchdog();
		if (SUCCEEDED(hR))
		{
			if (IsObjectRestartingRequired())
			{
				pMonitor->TerminateServer();
				pMonitor = NULL;
				continue;
			}

			break;
		}
		
		if (hR == HRESULT_FROM_WIN32(ERROR_RETRY))
			continue;
		else if (hR == E_ABORT)
			pMonitor = NULL;
		else
		{
			CoUninitialize();
			return MAKE_STATUS(ActionStatus::FromHResult(hR));
		}
	}

	CoUninitialize();
	return MAKE_STATUS(Success);
}

BazisLib::ActionStatus VirtualCDClient::ConnectDisk( LPCWSTR lpKernelFilePath, char SuggestedLetter, unsigned MaxWaitTillMount, bool DisableAutorun, bool Persistent, PAUTORUNERRORHANDLER pAutorunErrorHandler, MMCProfile profile)
{
	if (!lpKernelFilePath)
		return MAKE_STATUS(InvalidParameter);
	if (!m_pDevice)
		return MAKE_STATUS(NotInitialized);

	VirtualCDMountParams params = {0,};
	params.DisableAutorun = false;
	params.SuggestedDriveLetter = SuggestedLetter;
	params.StructureVersion = VirtualCDMountParams::CurrentVersion;
	params.DisableAutorun = DisableAutorun;
	params.KeepAfterRestart = Persistent;
	params.MMCProfile = profile;
	wcsncpy(params.wszFilePath, lpKernelFilePath, __countof(params.wszFilePath));

	if (DisableAutorun)
	{
		ActionStatus st = StartAutorunDisablingMonitor();
		if (!st.Successful())
			if (!pAutorunErrorHandler || !pAutorunErrorHandler(st))
				return st;
	}

	ActionStatus status;
	if (!MaxWaitTillMount)
		m_pDevice->DeviceIoControl(IOCTL_PLUGIN_DEVICE, &params, (DWORD)sizeof(params), NULL, 0, &status);
	else
	{
		ULONGLONG ret = -1LL;
		m_pDevice->DeviceIoControl(IOCTL_PLUGIN_DEVICE_RETURN_WAITHANDLE, &params, (DWORD)sizeof(params), &ret, sizeof(ULONGLONG), &status);
		if (!status.Successful())
			return status;
		if (ret != -1LL)
		{
			WaitForSingleObject((HANDLE)ret, MaxWaitTillMount);
			CloseHandle((HANDLE)ret);
		}
	}
	return status;
}

BazisLib::ActionStatus VirtualCDClient::DisconnectDisk( LPCWSTR lpImageFilePath )
{
	if (!lpImageFilePath)
		return MAKE_STATUS(InvalidParameter);
	if (!m_pDevice)
		return MAKE_STATUS(NotInitialized);

	ActionStatus status;
	m_pDevice->DeviceIoControl(IOCTL_SOFT_PLUGOUT_DEVICE, lpImageFilePath, (DWORD)(wcslen(lpImageFilePath) + 1) * sizeof(wchar_t), NULL, 0, &status);
	if (status.Successful())
		return status;

	if (status.GetErrorCode() == InvalidParameter)
	{
		if (TryDisconnectingUsingEject(lpImageFilePath).Successful())
			return MAKE_STATUS(Success);
	}	

	status = ActionStatus();
	m_pDevice->DeviceIoControl(IOCTL_PLUGOUT_DEVICE, lpImageFilePath, (DWORD)(wcslen(lpImageFilePath) + 1) * sizeof(wchar_t), NULL, 0, &status);
	return status;
}

VirtualCDClient::VirtualCDClient(BazisLib::ActionStatus *pStatus, bool usePortableDriver, bool tryBothDrivers)
{
	//Do 2 iterations (abort if a driver was found, or the fallback was disabled)
	for (int i = 0; i < 2; i++)
	{
		if (usePortableDriver)
			m_pDevice = new ACFileWithIOCTL(L"\\\\.\\BazisPortableCDBus", FileModes::OpenReadWrite, pStatus);
		else
		{
			list<String> devs = EnumerateDevicesByInterface(&GUID_BazisVirtualCDBus);
			if (devs.size() < 1)
			{
				ASSIGN_STATUS(pStatus, FileNotFound);
				return;
			}
			m_pDevice = new ACFileWithIOCTL(*devs.begin(), FileModes::OpenReadWrite, pStatus);
		}

		if (m_pDevice->Valid() || !tryBothDrivers)
			break;

		//Now let's try another driver
		usePortableDriver = !usePortableDriver;
	}
}

bool VirtualCDClient::IsDiskConnected( LPCWSTR lpImageFilePath )
{
	if (!lpImageFilePath)
		return false;
	if (!m_pDevice)
		return false;

	ActionStatus status;
	bool present = false;
	m_pDevice->DeviceIoControl(IOCTL_IS_DEVICE_PRESENT, lpImageFilePath, (DWORD)(wcslen(lpImageFilePath) + 1) * sizeof(wchar_t), &present, sizeof(present), &status);
	if (!status.Successful())
		return false;
	return present;
}

bool VirtualCDClient::Win32FileNameToKernelFileName( LPCWSTR lpWin32Name, LPWSTR lpKernelName, size_t MaxKernelNameSize )
{
	if (!lpWin32Name || !lpKernelName || !MaxKernelNameSize)
		return false;
	lpKernelName[0] = 0;
	TCHAR tszFullPath[MAX_STR_SIZE];// = _T("\\??\\");
	GetFullPathName(lpWin32Name, _countof(tszFullPath), tszFullPath, NULL);
	if ((lpWin32Name[0] == '\\') && (lpWin32Name[1] == '\\'))
	{
		wcscpy(lpKernelName, L"\\Device\\LanmanRedirector\\");
		memmove(tszFullPath, tszFullPath + 2, (_tcslen(tszFullPath) - 1) * sizeof(TCHAR));
	}
	else
	{
		TCHAR tszRoot[4] = {tszFullPath[0], ':', '\\', 0};
		/*if (GetDriveType(tszRoot) == DRIVE_REMOTE)
		{
			tszRoot[2] = 0;
			if (!QueryDosDevice(tszRoot, tszFullPath, MAX_PATH))
				return false;
			wcsncpy(lpKernelName, tszFullPath, MAX_PATH);
			GetFullPathName(lpWin32Name, MAX_PATH, tszFullPath, NULL);
			memmove(tszFullPath, tszFullPath + 2, (_tcslen(tszFullPath) - 1) * sizeof(TCHAR));
		}
		else*/
			wcsncpy(lpKernelName, L"\\??\\", MaxKernelNameSize);
	}
	wcsncat(lpKernelName, tszFullPath, MaxKernelNameSize);
	return true;
}

VirtualCDClient::VirtualCDList VirtualCDClient::GetVirtualDiskList(const wchar_t *pFilter)
{
	VirtualCDList devices;

	char buf[8192] = {0,};

	ActionStatus status;
	size_t done = m_pDevice->DeviceIoControl(IOCTL_LIST_DEVICES, pFilter, pFilter ? (wcslen(pFilter) + 1) * sizeof(wchar_t) : 0, &buf, sizeof(buf), &status);
	if (!status.Successful())
		return devices;

	VirtualCDRecordHeader *pHdr = (VirtualCDRecordHeader *)buf;
	if (pHdr->Version != VirtualCDRecordHeader::CurrentVersion)
		return devices;

	if (pHdr->RecordCount > 50)
		pHdr->RecordCount = 50;

	devices.reserve(pHdr->RecordCount);

	for (unsigned i = 0; i < pHdr->RecordCount; i++)
	{
		wchar_t *pImgName = (wchar_t *)(buf + pHdr->Records[i].ImageNameOffset);
		if (!wcsncmp(pImgName, L"\\??\\", 4))
			pImgName += 4;
		else if (!wcsncmp(pImgName, L"\\Device\\LanmanRedirector", 24))
			pImgName += 23, pImgName[0] = '\\';

		VirtualCDEntry entry(pImgName, pHdr->Records[i].DriveLetter);
		entry.HandleCount = pHdr->Records[i].HandleCount;
		entry.RefCount = pHdr->Records[i].RefCount;
		entry.DeviceRefCount = pHdr->Records[i].DeviceRefCount;
		devices.push_back(entry);
	}

	return devices;
}

void VirtualCDClient::RestartDriver(bool unmountAllDrives)
{
	if (unmountAllDrives)
	{
		VirtualCDClient clt;
		VirtualCDList devices = clt.GetVirtualDiskList(NULL);
		for each(const VirtualCDEntry &entry in devices)
			clt.DisconnectDisk(entry.ImageFile.c_str());
	}

	std::list<String> devices;
	DeviceInformationSet devs = DeviceInformationSet::GetLocalDevices(&GUID_BazisVirtualCDBus);
	for (DeviceInformationSet::iterator it = devs.begin(); it != devs.end(); it++)
	{
		it.ChangeState(DICS_DISABLE);
		it.ChangeState(DICS_ENABLE);
	}
}

unsigned VirtualCDClient::QueryVersion()
{
	unsigned version = 0;
	if (!m_pDevice)
		return 0;
	size_t done = m_pDevice->DeviceIoControl(IOCTL_QUERY_DRIVER_VERSION, NULL, 0, &version, sizeof(version));
	if (done != sizeof(version))
		return 0;
	return version;
}

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
BazisLib::ActionStatus VirtualCDClient::SetDebugLogDirectory(LPCWSTR lpKernelDirPath)
{
	ActionStatus status;
	m_pDevice->DeviceIoControl(IOCTL_SET_DEBUG_LOG_DIR, lpKernelDirPath, lpKernelDirPath ? (wcslen(lpKernelDirPath) + 1) * sizeof(wchar_t) : 0, NULL, 0, &status);
	return status;
}
#endif

BazisLib::ActionStatus VirtualCDClient::TryDisconnectingUsingEject(LPCWSTR lpKernelFilePath)
{
	char buf[8192] = {0,};
	ActionStatus status;
	size_t done = m_pDevice->DeviceIoControl(IOCTL_LIST_DEVICES, lpKernelFilePath, (wcslen(lpKernelFilePath) + 1) * sizeof(wchar_t), &buf, sizeof(buf), &status);
	if (!status.Successful())
		return status;

	VirtualCDRecordHeader *pHdr = (VirtualCDRecordHeader *)buf;
	if (pHdr->Version != VirtualCDRecordHeader::CurrentVersion)
		return MAKE_STATUS(UnknownError);

	if (pHdr->RecordCount != 1)
		return MAKE_STATUS(UnknownError);

	wchar_t wszPath[] = L"\\\\.\\?:";
	wszPath[4] = pHdr->Records[0].DriveLetter;

	{
		status = ActionStatus();
		VirtualDriveClient clt(wszPath, &status);
		if (!status.Successful())
			return status;

		if (_wcsicmp(clt.GetNativeMountedImagePath().c_str(), lpKernelFilePath))
			return MAKE_STATUS(UnknownError);
	}

	status = ActionStatus();
	File file(wszPath, FileModes::OpenReadOnly.ShareAll(), &status);
	if (!status.Successful())
		return status;

	status = ActionStatus();
	file.DeviceIoControl(IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &status);
	return status;
}

BazisLib::ActionStatus VirtualCDClient::MountImageOnExistingDrive(const wchar_t *pImage, char driveLetter, unsigned timeout)
{
	wchar_t wszPath [] = L"\\\\.\\?:";
	wszPath[4] = driveLetter;

	ActionStatus status = ActionStatus();
	VirtualDriveClient clt(wszPath, &status);
	if (!status.Successful())
		return status;

	return clt.MountNewImage(pImage, timeout);
}
