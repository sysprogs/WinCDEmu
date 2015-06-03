#include "stdafx.h"
#include <bzscore/file.h>
#include <bzshlp/Win32/services.h>
#include <bzshlp/Win32/wow64.h>
#include "../VirtualCDCtl/VirtualCDClient.h"
#define SKIP_WINCDEMU_GUID_DEFINITION
#include "../BazisVirtualCDBus/DeviceControl.h"

using namespace BazisLib;
using namespace BazisLib::Win32;

static const TCHAR *tszServiceName = _T("BazisPortableCDBus");

static ActionStatus SaveResourceToFile(const String &filePath, LPCTSTR lpResType, DWORD dwID)
{
	HMODULE hModule = GetModuleHandle(NULL);
	HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(dwID), lpResType);
	if (!hRes)
		return MAKE_STATUS(ActionStatus::FailFromLastError());

	DWORD len = SizeofResource(hModule, hRes);
	if (!len)
		return MAKE_STATUS(ActionStatus::FailFromLastError());

	HGLOBAL hResObj = LoadResource(hModule, hRes);
	if (!hResObj)
		return MAKE_STATUS(ActionStatus::FailFromLastError());

	PVOID p = LockResource(hResObj);
	if (!p)
		return MAKE_STATUS(ActionStatus::FailFromLastError());

	ActionStatus st;
	BazisLib::File file(filePath, FileModes::CreateOrTruncateRW, &st);
	if (!st.Successful())
		return st;
	file.Write(p, len, &st);
	if (!st.Successful())
		return st;
	return MAKE_STATUS(Success);
}

bool IsDriverInstallationPending(bool *pOldVersionInstalled)
{
	ServiceControlManager srvMgr;
	ActionStatus st;
	*pOldVersionInstalled = false;
	Service srv(&srvMgr, tszServiceName, SERVICE_START | SERVICE_STOP, &st);
	if (!st.Successful())
		return true;
	VirtualCDClient client(&st, true);
	if (!st.Successful())
		return true;
	if (client.QueryVersion() < WINCDEMU_DRIVER_VERSION_CURRENT)
	{
		*pOldVersionInstalled = true;
		return true;	
	}
	return false;
}

ActionStatus InstallDriver(bool oldVersionInstalled)
{
	String fp = Path::Combine(Path::GetSpecialDirectoryLocation(dirDrivers), (tszServiceName));
	fp += L".sys";
	ActionStatus st;

	if (BazisLib::WOW64APIProvider::sIsWow64Process())
	{
		WOW64FSRedirHolder holder;
		st = SaveResourceToFile(fp, L"DRIVERFILE", 64);
	}
	else
		st = SaveResourceToFile(fp, L"DRIVERFILE", 32);

	if (!st.Successful())
		return st;

	String srvFile = L"system32\\drivers\\";
	srvFile += tszServiceName;
	srvFile += _T(".sys");

	if (oldVersionInstalled)
	{
		{
			VirtualCDClient client(&st, true);
			if (!st.Successful())
				return st;

			for each(const VirtualCDClient::VirtualCDEntry &entry in client.GetVirtualDiskList())
			{
				wchar_t wszFullPath[MAX_PATH] = {0,};
				VirtualCDClient::Win32FileNameToKernelFileName(entry.ImageFile.c_str(), wszFullPath, __countof(wszFullPath));
				st = client.DisconnectDisk(wszFullPath);
				if (!st.Successful())
					return st;
			}
		}

		ServiceControlManager mgr;
		Service newService(&mgr, tszServiceName, SERVICE_START | SERVICE_STOP, &st);
		if (!st.Successful())
			return st;
		st = newService.Stop();
		if (!st.Successful())
			return st;

		for (int i = 0; i < 10; i++)
		{
			st = newService.Start();
			if (st.Successful())
				break;
			if (st.ConvertToHResult() != HRESULT_FROM_WIN32(ERROR_SERVICE_ALREADY_RUNNING))
				break;
			Sleep(100);
		}
	}
	else
	{
		ServiceControlManager srvMgr;
		Service newService;
		for (int iter = 0; iter < 5; iter++)
		{
			st = srvMgr.CreateService(&newService, tszServiceName, _T("Portable WinCDEmu driver"), SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, srvFile.c_str());
			if (st.Successful())
				break;
			else if (st.ConvertToHResult() == HRESULT_FROM_WIN32(ERROR_SERVICE_EXISTS))
				return MAKE_STATUS(Success);
			else if (st.ConvertToHResult() != HRESULT_FROM_WIN32(ERROR_SERVICE_MARKED_FOR_DELETE))
				return st;

			//Windows bug - creating the service right after another service with the same name was deleted results in the ERROR_SERVICE_MARKED_FOR_DELETE error.
			//The workaround is simply to try deleting the service again.
			if (iter != 0)
				Sleep(300);

			Service svc(&srvMgr, tszServiceName, SERVICE_ALL_ACCESS, &st);
			if (!st.Successful())
				continue;

			svc.Stop();
			svc.Delete();
			continue;
		}
	}
	return st;
}

ActionStatus StartDriverIfNeeded()
{
	ServiceControlManager srvMgr;
	ActionStatus st;
	Service svc(&srvMgr, tszServiceName, SERVICE_ALL_ACCESS, &st);
	if (!st.Successful())
		return st;
	if (svc.QueryState() == SERVICE_RUNNING)
		return MAKE_STATUS(Success);
	return svc.Start();
}

ActionStatus UninstallDriver()
{
	ServiceControlManager srvMgr;
	ActionStatus st;
	Service svc(&srvMgr, tszServiceName, SERVICE_ALL_ACCESS, &st);
	if (!st.Successful())
		return st;

	if (svc.QueryState() != SERVICE_STOPPED)
		svc.Stop();

	st = svc.Delete();
	if (!st.Successful())
		return st;

	String fp = Path::Combine(Path::GetSpecialDirectoryLocation(dirDrivers), (tszServiceName));
	fp += L".sys";
	if (BazisLib::WOW64APIProvider::sIsWow64Process())
	{
		WOW64FSRedirHolder holder;
		return BazisLib::File::Delete(fp);
	}
	else
		return BazisLib::File::Delete(fp);
}