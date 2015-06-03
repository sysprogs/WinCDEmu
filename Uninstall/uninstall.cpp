// uninstall.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <bzshlp/Win32/bzsdev.h>
#include <bzscore/Win32/registry.h>
#include <bzscore/file.h>
#include <bzscore/Win32/security.h>
#include <bzshlp/Win32/process.h>
#include <set>

#pragma comment(lib, "setupapi")

using namespace BazisLib;

bool IsDiskEnumeratorInstalled(LPCTSTR lpHwID)
{
	DeviceInformationSet devs = DeviceInformationSet::GetLocalDevices();
	for (DeviceInformationSet::iterator it = devs.begin(); it != devs.end(); it++)
	{
		if (!it.GetDeviceRegistryProperty(SPDRP_HARDWAREID).compare(lpHwID))
			return true;
	}
	return false;
}

void RemoveDiskEnumerator(LPCTSTR lpHwID)
{
	DeviceInformationSet devs = DeviceInformationSet::GetLocalDevices();
	for (DeviceInformationSet::iterator it = devs.begin(); it != devs.end(); it++)
	{
		if (!it.GetDeviceRegistryProperty(SPDRP_HARDWAREID).compare(lpHwID))
		{
			it.CallClassInstaller(DIF_REMOVE);
		}
	}
}

void RemoveRegistryKeys()
{
	RegistryKey key(HKEY_CLASSES_ROOT, _T("Applications"));
	key.DeleteSubKeyRecursive(_T("vmnt.exe"));
	key.DeleteSubKeyRecursive(_T("vmnt64.exe"));
	RegistryKey key3(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"));
	key3.DeleteSubKeyRecursive(_T("WinCDEmu"));
}

static const wchar_t *g_Extensions[] = {L"ISO", L"CUE", L"IMG", L"NRG", L"MDS", L"CCD"};

void RemoveSelectLetterCommandKeys()
{
	std::set<String, String::_CaseInsensitiveLess> classes;
	for each(const wchar_t *pExt in g_Extensions)
	{
		classes.insert(String::sFormat(L"BazisVirtualCD.%s", pExt));
		classes.insert(RegistryKey(HKEY_CLASSES_ROOT, String::sFormat(L".%s", pExt).c_str())[NULL]);
	}

	for each(const String &str in classes)
	{
		if (str.empty())
			continue;
		RegistryKey key(HKEY_CLASSES_ROOT, str.c_str());
		if (!key.Valid())
			continue;
		RegistryKey keyShell = key[L"shell"];
		if (keyShell.Valid())
			keyShell.DeleteSubKeyRecursive(L"Select drive letter && mount");
	}

}

void RemoveTreeRecursive(LPCTSTR pTreePath)
{
	TCHAR tszMask[MAX_PATH];
	int pos = _sntprintf(tszMask, sizeof(tszMask)/sizeof(tszMask[0]), _T("%s\\*.*"), pTreePath) - 3;
	WIN32_FIND_DATA WFD;
	HANDLE hFind = FindFirstFile(tszMask, &WFD);
	if ((hFind == INVALID_HANDLE_VALUE) || !hFind)
		return;
	do
	{
		if ((WFD.cFileName[0] == '.') && ((WFD.cFileName[1] == '.') || (WFD.cFileName[1] == 0)))
			continue;
		_tcsncpy(&tszMask[pos], WFD.cFileName, sizeof(tszMask)/sizeof(tszMask[0]) - pos);
		if (WFD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			RemoveTreeRecursive(tszMask);
		if (!DeleteFile(tszMask))
			MoveFileEx(tszMask, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
	} while (FindNextFile(hFind, &WFD));
	FindClose(hFind);
	if (!RemoveDirectory(pTreePath))
		MoveFileEx(pTreePath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
}

void DeleteFiles(BazisLib::String path)
{
	TCHAR tsz[MAX_PATH] = {0,};
	GetModuleFileName(GetModuleHandle(NULL), tsz, sizeof(tsz)/sizeof(tsz[0]));
	TCHAR *p = _tcsrchr(tsz, '\\');
	if (!p)
		return;
	p[0] = 0;
	if (path.empty())
		path = tsz;

	RemoveTreeRecursive((Path::Combine(path, _T("x86"))).c_str());
	RemoveTreeRecursive((Path::Combine(path, _T("x64"))).c_str());
	RemoveTreeRecursive((Path::Combine(path, _T("langfiles"))).c_str());
	File::Delete(Path::Combine(path, _T("vmnt.exe")));
	File::Delete(Path::Combine(path, _T("vmnt64.exe")));
	File::Delete(Path::Combine(path, _T("batchmnt.exe")));
	File::Delete(Path::Combine(path, _T("batchmnt64.exe")));
	File::Delete(Path::Combine(path, _T("BazisVirtualCD.inf")));
	File::Delete(Path::Combine(path, _T("VirtDiskBus.inf")));
	File::Delete(Path::Combine(path, _T("BazisVirtualCDBus.inf")));
	File::Delete(Path::Combine(path, _T("BazisVirtualCD.cat")));
	File::Delete(Path::Combine(path, _T("VirtDiskBus.cat")));
	File::Delete(Path::Combine(path, _T("BazisVirtualCDBus.cat")));
	File::Delete(Path::Combine(path, _T("uninstall.exe")));
	File::Delete(Path::Combine(path, _T("uninstall64.exe")));
	Directory::Remove(path);
	p[0] = '\\';
	MoveFileEx(tsz, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
}

#pragma comment(lib, "ntdll")

using namespace BazisLib::Win32::Security;


ActionStatus SetKeyOwnerRecursive(HKEY hParentKey, String subKey, const Sid &sid)
{
	HKEY hKey = 0;
	LSTATUS err = RegOpenKeyEx(hParentKey, subKey.c_str(), 0, WRITE_OWNER, &hKey);
	if (err)
		return MAKE_STATUS(ActionStatus::FromWin32Error(err));

	TranslatedSecurityDescriptor desc;
	desc.Owner = sid;
	err = RegSetKeySecurity(hKey, OWNER_SECURITY_INFORMATION, desc.BuildNTSecurityDescriptor());
	RegCloseKey(hKey);

	if (err)
		return MAKE_STATUS(ActionStatus::FromWin32Error(err));

	hKey = 0;
	err = RegOpenKeyEx(hParentKey, subKey.c_str(), 0, READ_CONTROL | WRITE_OWNER | WRITE_DAC, &hKey);
	if (err)
		return MAKE_STATUS(ActionStatus::FromWin32Error(err));

	DWORD sdSize = 65536;
	char *pBuf = new char[sdSize];
	err = RegGetKeySecurity(hKey, DACL_SECURITY_INFORMATION, (PSECURITY_DESCRIPTOR)pBuf, &sdSize);
	if (err)
	{
		CloseHandle(hKey);
		delete pBuf;
		return MAKE_STATUS(ActionStatus::FromWin32Error(err));
	}

	TranslatedSecurityDescriptor keySD((PSECURITY_DESCRIPTOR)pBuf);
	delete pBuf;

	keySD.DACL.AddAllowingAce(KEY_ALL_ACCESS, sid);

	err = RegSetKeySecurity(hKey, DACL_SECURITY_INFORMATION, desc.BuildNTSecurityDescriptor());
	RegCloseKey(hKey);

	if (err)
		return MAKE_STATUS(ActionStatus::FromWin32Error(err));


	hKey = 0;
	err = RegOpenKeyEx(hParentKey, subKey.c_str(), 0, KEY_ENUMERATE_SUB_KEYS, &hKey);
	if (err)
		return MAKE_STATUS(ActionStatus::FromWin32Error(err));

	for (unsigned i = 0; ; i++)
	{
		TCHAR tszName[512] = {0, };
		err = RegEnumKey(hKey, i, tszName, __countof(tszName));

		if (err == ERROR_NO_MORE_ITEMS)
			break;
		else if (err)
		{
			RegCloseKey(hKey);
			return MAKE_STATUS(ActionStatus::FromWin32Error(err));
		}

		ActionStatus st = SetKeyOwnerRecursive(hParentKey, subKey + _T("\\") + tszName, sid);
		if (!st.Successful())
		{
			RegCloseKey(hKey);
			return st;
		}
	}
	RegCloseKey(hKey);
	return MAKE_STATUS(Success);
}

int WINAPI CALLBACK WinMain (
		 __in HINSTANCE hInstance,
		 __in_opt HINSTANCE hPrevInstance,
		 __in_opt LPSTR lpCmdLine,
		 __in int nShowCmd
		 )
{
	TCHAR tszMainDir[MAX_PATH] = {0,};
	GetModuleFileName(GetModuleHandle(NULL), tszMainDir, _countof(tszMainDir));
	TCHAR *p = _tcsrchr(tszMainDir, '\\');
	if (!p)
		return 0;
	p[0] = 0;

	if (strstr(lpCmdLine, "/UPDATE"))
	{
		PrivilegeHolder holder(SE_TAKE_OWNERSHIP_NAME);
		ActionStatus st = SetKeyOwnerRecursive(HKEY_LOCAL_MACHINE,  _T("SYSTEM\\CurrentControlSet\\Enum\\BazisVirtualCDBus"), Sid::CurrentUserSid());
		RegistryKey key(HKEY_LOCAL_MACHINE,  _T("SYSTEM\\CurrentControlSet\\Enum\\BazisVirtualCDBus"));
		if (st.Successful())
			st = key.DeleteSubKeyRecursive(_T("StandardDevice"));

		if (!st.Successful() && (st.ConvertToHResult() != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)))
			MessageBox(0, String::sFormat(_T("Warning: could not cleanup registry key under SYSTEM\\CurrentControlSet\\Enum\\BazisVirtualCDBus - %s\nConsider removing it manually."), st.GetMostInformativeText().c_str()).c_str(), _T("Warning"), MB_ICONWARNING);

		RemoveDiskEnumerator(_T("root\\bzsvdisk"));

		String path(tszMainDir);
		File::Delete(Path::Combine(path, _T("BazisVirtualCD.inf")));
		File::Delete(Path::Combine(path, _T("BazisVirtualCD.cat")));
		File::Delete(Path::Combine(path, _T("VirtDiskBus.inf")));
		File::Delete(Path::Combine(path, _T("VirtDiskBus.cat")));
		RemoveSelectLetterCommandKeys();

		ShellExecute(HWND_DESKTOP, _T("open"), String::sFormat(L"%s\\x86\\VirtualAutorunDisabler.exe", tszMainDir).c_str(), _T("/RegServer"), NULL, SW_SHOW);
		ShellExecute(HWND_DESKTOP, _T("open"), _T("regsvr32.exe"), String::sFormat(L"/s \"%s\\x86\\VirtualAutorunDisablerPS.dll\"", tszMainDir).c_str(), NULL, SW_SHOW);
		ShellExecute(HWND_DESKTOP, _T("open"), _T("regsvr32.exe"), String::sFormat(L"/s \"%s\\x86\\WinCDEmuContextMenu.dll\"", tszMainDir).c_str(), NULL, SW_SHOW);
#ifdef _WIN64
		ShellExecute(HWND_DESKTOP, _T("open"), String::sFormat(L"%s\\x64\\VirtualAutorunDisabler.exe", tszMainDir).c_str(), _T("/RegServer"), NULL, SW_SHOW);
		ShellExecute(HWND_DESKTOP, _T("open"), _T("regsvr32.exe"), String::sFormat(L"/s \"%s\\x64\\VirtualAutorunDisablerPS.dll\"", tszMainDir).c_str(), NULL, SW_SHOW);
		ShellExecute(HWND_DESKTOP, _T("open"), _T("regsvr32.exe"), String::sFormat(L"/s \"%s\\x64\\WinCDEmuContextMenu.dll\"", tszMainDir).c_str(), NULL, SW_SHOW);
#endif
		return 0;
	}

	TCHAR *pszProgramDir = _tcsstr(GetCommandLine(), _T("/WINCDEMUDIR:"));
	if (pszProgramDir)
		pszProgramDir += 13;
	else
	{
		if (!_tcsstr(GetCommandLine(), _T("/UNATTENDED")))
			if (MessageBox(0, _T("Do you really want to uninstall WinCDEmu?"), _T("Question"), MB_ICONQUESTION | MB_YESNO) != IDYES)
				return 0;

		TCHAR tszTemp[MAX_PATH] = {0,}, tszThis[MAX_PATH] = {0,};
		GetModuleFileName(0, tszThis, __countof(tszThis));
		GetTempPath(__countof(tszTemp), tszTemp);
		_tcsncat(tszTemp, _T("\\wcduninst.exe"), _countof(tszTemp));

		String fpThis = tszThis;
		if (CopyFile(tszThis, tszTemp, FALSE))
		{
			String cmdLine = String::sFormat(_T("%s /WINCDEMUDIR:%s"), tszTemp, Path::GetDirectoryName(fpThis).c_str());
			ActionStatus st;
			Win32::Process proc(cmdLine.c_str(), &st);
			if (st.Successful())
				return 0;
		}
	}

	RemoveDiskEnumerator(_T("root\\BazisVirtualCDBus"));
	RemoveDiskEnumerator(_T("root\\bzsvdisk"));
	RemoveRegistryKeys();

	BazisLib::Win32::Process::RunCommandLineSynchronously((LPTSTR)String::sFormat(L"%s\\x86\\VirtualAutorunDisabler.exe /UnregServer", pszProgramDir).c_str());
	BazisLib::Win32::Process::RunCommandLineSynchronously((LPTSTR)String::sFormat(L"regsvr32.exe /s /u \"%s\\x86\\VirtualAutorunDisablerPS.dll\"", pszProgramDir).c_str());
	BazisLib::Win32::Process::RunCommandLineSynchronously((LPTSTR)String::sFormat(L"regsvr32.exe /s /u \"%s\\x86\\WinCDEmuContextMenu.dll\"", pszProgramDir).c_str());
#ifdef _WIN64
	BazisLib::Win32::Process::RunCommandLineSynchronously((LPTSTR)String::sFormat(L"%s\\x64\\VirtualAutorunDisabler.exe /UnregServer", pszProgramDir).c_str());
	BazisLib::Win32::Process::RunCommandLineSynchronously((LPTSTR)String::sFormat(L"regsvr32.exe /s /u \"%s\\x64\\VirtualAutorunDisablerPS.dll\"", pszProgramDir).c_str());
	BazisLib::Win32::Process::RunCommandLineSynchronously((LPTSTR)String::sFormat(L"regsvr32.exe /s /u \"%s\\x64\\WinCDEmuContextMenu.dll\"", pszProgramDir).c_str());
#endif

	DeleteFiles(pszProgramDir);
	MessageBox(0, _T("WinCDEmu was successfully uninstalled."), _T("Information"), MB_ICONINFORMATION);
	return 0;
}
