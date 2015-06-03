// batchmnt.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../VirtualCDCtl/VirtualCDClient.h"
#include "../VirtualCDCtl/VirtualDriveClient.h"
#include <bzshlp/Win32/process.h>
#include <conio.h>

using namespace BazisLib;

int DisplayHelp(bool portable)
{
	wchar_t wszExe[MAX_PATH] = { 0, };
	GetModuleFileName(GetModuleHandle(NULL), wszExe, _countof(wszExe));
	wchar_t *pExe = wcsrchr(wszExe, '\\');
	if (!pExe)
		pExe = L"batchmnt.exe";
	else
		pExe++;

	if (portable)
		printf("Portable WinCDEmu - console mode.\r\n");
	else
		printf("%S - WinCDEmu batch mounter.\r\n", pExe);

	wchar_t *pDot = wcschr(pExe, '.');
	if (pDot)
		pDot[0] = 0;

	printf("Usage:\r\n");
	printf("%S <image file> [<drive letter>] [/wait] - mount image file\r\n", pExe);
	printf("%S /unmount <image file>    - unmount image file\r\n", pExe);
	printf("%S /unmount <drive letter>: - unmount image file\r\n", pExe);
	printf("%S /check   <image file>    - return drive letter as ERORLEVEL\r\n", pExe);
	printf("%S /unmountall              - unmount all images\r\n", pExe);
	printf("%S /list                    - list mounted images\r\n", pExe);

	if (portable)
	{
		printf("%S /install                 - install portable driver\r\n", pExe);
		printf("%S /uninstall               - remove portable driver\r\n", pExe);
	}
	return 0;
}

int ListVirtualCDs(VirtualCDClient &clt, bool listHandleCount = false)
{
	VirtualCDClient::VirtualCDList devs = clt.GetVirtualDiskList();
	printf("Listing connected virtual CDs:\n");
	printf("Letter  Image file");
	if (listHandleCount)
		printf("      refs/drefs/handles");
	printf("\n");
	for each(const VirtualCDClient::VirtualCDEntry &entry in devs)
	{
		printf("%c:      %S", entry.DriveLetter, entry.ImageFile.c_str());
		if (listHandleCount)
			printf("       %d/%d/%d", entry.RefCount, entry.DeviceRefCount, entry.HandleCount);
		printf("\n");
	}
	printf("------------------------------\n");
	return 0;
}

int CheckCDImageLetter(VirtualCDClient &clt, const wchar_t *pCDImage, bool Silent = false)
{
	if (!pCDImage)
	{
		printf("BATCHMNT: no CD image specified\n");
		return 0;
	}
	wchar_t wszFullPath[MAX_PATH] = {0,};

	if ((pCDImage[1] == ':') && !pCDImage[2])
		wcsncpy(wszFullPath, pCDImage, _countof(wszFullPath));
	else
		VirtualCDClient::Win32FileNameToKernelFileName(pCDImage, wszFullPath, __countof(wszFullPath));
	
	VirtualCDClient::VirtualCDList devs = clt.GetVirtualDiskList(wszFullPath);
	if (devs.empty())
	{
		if (!Silent)
			printf("%S is not mounted\r\n", pCDImage);
		return 0;
	}
	else
	{
		if (!Silent)
			printf("%S is mounted to %c:\r\n", devs[0].ImageFile.c_str(), devs[0].DriveLetter);
		return devs[0].DriveLetter;
	}
}

int DoUnmount(VirtualCDClient &clt, const wchar_t *pCDImage)
{
	if (!pCDImage)
	{
		printf("BATCHMNT: no CD image specified\n");
		return 0;
	}
	wchar_t wszFullPath[MAX_PATH] = {0,};

	if ((pCDImage[1] == ':') && !pCDImage[2])
	{
		VirtualCDClient::VirtualCDList devs = clt.GetVirtualDiskList(pCDImage);
		if (devs.empty())
		{
			printf("%S is not a WinCDEmu drive\n", pCDImage);
			return 1;
		}
		return DoUnmount(clt, devs[0].ImageFile.c_str());
	}

	VirtualCDClient::Win32FileNameToKernelFileName(pCDImage, wszFullPath, __countof(wszFullPath));
	ActionStatus st = clt.DisconnectDisk(wszFullPath);
	printf("%S\n", st.GetMostInformativeText().c_str());
	return 0;
}

int DoMount(VirtualCDClient &clt, const wchar_t *pCDImage, const wchar_t *pLetter, unsigned WaitTimeout)
{
	if (!pCDImage)
	{
		printf("BATCHMNT: no CD image specified\n");
		return 0;
	}
	wchar_t wszFullPath[MAX_PATH] = {0,};
	VirtualCDClient::Win32FileNameToKernelFileName(pCDImage, wszFullPath, __countof(wszFullPath));
	ActionStatus st;
	bool remountPerformed = false;

	if (clt.IsDiskConnected(wszFullPath))
	{
		printf("%S is already mounted!\n", pCDImage);
		return 1;
	}

	if (pLetter && pLetter[0])
	{
		wchar_t wszPath [] = L"\\\\.\\?:";
		wszPath[4] = pLetter[0];
		ActionStatus connectStatus;
		VirtualDriveClient driveClient(wszPath, &connectStatus);
		if (connectStatus.Successful())
		{
			remountPerformed = true;
			//This is a WinCDEmu drive letter. Mount the new image over the old one.
			st = driveClient.MountNewImage(wszFullPath, WaitTimeout);
		}
	}

	if (!remountPerformed)
		st = clt.ConnectDisk(wszFullPath, pLetter ? pLetter[0] : 0, WaitTimeout);

	printf("%S\n", st.GetMostInformativeText().c_str());
	return !st.Successful();
}

using namespace std;
#include <map>
#include <vector>

class CommandLineArguments
{
public:
	vector<String> NonOptionArgs;
	map<String, String> OptionArgs;

public:
	CommandLineArguments(int argc, _TCHAR* argv[], TCHAR _OptionPrefix = '/', TCHAR _OptionSeparator = ':')
	{
		for (int i = 1; i < argc; i++)
		{
			TCHAR *pArg = argv[i];
			if (!pArg || !pArg[0])
				continue;
			if (pArg[0] != _OptionPrefix)
				NonOptionArgs.push_back(pArg);
			else
			{
				pArg++;
				TCHAR *pSep = _tcschr(pArg, _OptionSeparator);
				if (pSep)
					OptionArgs[String(pArg, pSep - pArg)] = pSep + 1;
				else
					OptionArgs[pArg] = String();
			}
		}
	}

	bool OptionExists(TCHAR *pOption)
	{
		return OptionArgs.find(pOption) != OptionArgs.end();
	}

	const TCHAR *GetNonOptionArg(unsigned index, TCHAR *pDefault)
	{
		if (index >= NonOptionArgs.size())
			return pDefault;
		return NonOptionArgs[index].c_str();
	}

	const TCHAR *GetOptionArg(TCHAR *pOption, TCHAR *pDefaultIfNotReferenced, TCHAR *pDefaultIfEmpty)
	{
		if (!OptionExists(pOption))
			return pDefaultIfNotReferenced;
		if (OptionArgs[pOption].empty())
			return pDefaultIfEmpty;
		return OptionArgs[pOption].c_str();
	}
};

#include <fcntl.h>
#include <io.h>

int BatchMounterMain(int argc, _TCHAR* argv [], bool portable)
{
	if (argc < 2)
		return DisplayHelp(portable);
	if (!_tcsicmp(argv[1], _T("/?")) || !_tcsicmp(argv[1], _T("/help")) || !_tcsicmp(argv[1], _T("-help")) || !_tcsicmp(argv[1], _T("--help")))
		return DisplayHelp(portable);

	CommandLineArguments args(argc, argv);
	if (args.OptionExists(_T("UACPIPE")))
	{
		ActionStatus st;
		File pipe(args.OptionArgs[_T("UACPIPE")].c_str(), FileModes::OpenReadWrite, &st);
		HANDLE hPipe = pipe.DetachHandle();
		if (hPipe != INVALID_HANDLE_VALUE)
			*stdout = *_fdopen(_open_osfhandle((intptr_t)hPipe, _O_WRONLY), "w");
	}

	ActionStatus status;
	VirtualCDClient clt(&status, portable, true);
	if (!clt.Valid())
	{
		if (status.GetErrorCode() == BazisLib::AccessDenied)
		{
			String pipeName;
			pipeName.Format(_T("\\\\.\\pipe\\WinCDEmu_BatchMnt_UAC_%d"), GetCurrentProcessId());

			HANDLE hResultPipe = CreateNamedPipe(pipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 1, 0, 32768, 32768, NULL);
			String cmdLine;


			if ((hResultPipe != INVALID_HANDLE_VALUE) || !_tcschr(GetCommandLine(), '>'))
				cmdLine.Format(_T("%s /UACPIPE:%s"), GetCommandLine(), pipeName.c_str());
			else
				cmdLine = GetCommandLine();

			BazisLib::Win32::Process proc(cmdLine.c_str(), _T("runas"), &status, SW_HIDE);
			if (!status.Successful())
			{
				printf("Cannot run elevated process: %S\n", status.GetMostInformativeText().c_str());
				return 1;
			}
			proc.Wait();

			for (;;)
			{
				DWORD dw = 0;
				PeekNamedPipe(hResultPipe, NULL, 0, 0, &dw, NULL);
				if (!dw)
					break;
				CBuffer buf;
				buf.EnsureSize(dw);
				ReadFile(hResultPipe, buf.GetData(), dw, &dw, NULL);
				WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf.GetData(), dw, &dw, NULL);
			}
			CloseHandle(hResultPipe);

			return proc.GetExitCode();
		}
		else
		{
			printf("Cannot connect to BazisVirtualCDBus.sys: %S\n", status.GetMostInformativeText().c_str());
			return 1;
		}
	}

	if (!_tcsicmp(argv[1], _T("/list")))
	{
		if ((argc >= 3) && !_tcsicmp(argv[2], _T("/handles")))
			return ListVirtualCDs(clt, true);
		else
			return ListVirtualCDs(clt, false);
	}
	else if (!_tcsicmp(argv[1], _T("/unmountall")))
	{
		printf("Unmounting all devices...");
		clt.DisconnectDisk(L"*");
		return 0;
	}
	else if (!_tcsicmp(argv[1], _T("/check")))
		return CheckCDImageLetter(clt, (argc >= 3) ? argv[2] : NULL);
	else if (!_tcsicmp(argv[1], _T("/unmount")))
		return DoUnmount(clt, (argc >= 3) ? argv[2] : NULL);
	else
	{
		if (args.NonOptionArgs.size() < 1)
			return DisplayHelp(portable);
		return DoMount(clt, args.GetNonOptionArg(0, NULL), args.GetNonOptionArg(1, NULL), _ttoi(args.GetOptionArg(_T("wait"), _T("0"), _T("-1"))));
	}
	return 0;
}

