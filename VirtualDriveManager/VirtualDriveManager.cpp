// VirtualDriveManager.cpp : main source file for VirtualDriveManager.exe
//

#include "stdafx.h"
#include "resource.h"
#include "MainDlg.h"
#include "DriverInstaller.h"

CAppModule _Module;

#define MSGFLT_ADD 1
#define MSGFLT_REMOVE 2

typedef
BOOL
(WINAPI *PChangeWindowMessageFilter)(
__in UINT message,
__in DWORD dwFlag);

void EnableFileDragDrop()
{
	HMODULE hUser32 = GetModuleHandle(_T("user32.dll"));
	PChangeWindowMessageFilter pChangeWindowMessageFilter = (PChangeWindowMessageFilter) GetProcAddress(hUser32, "ChangeWindowMessageFilter");
	if (pChangeWindowMessageFilter)
	{
		pChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
		pChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD);
		pChangeWindowMessageFilter(0x0049, MSGFLT_ADD);
	}
}

int BatchMounterMain(int argc, _TCHAR* argv [], bool portable);

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int /*nCmdShow*/)
{
	int argc = 0;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argc > 1)
	{
		bool ConsoleExisted = (AttachConsole(ATTACH_PARENT_PROCESS) != FALSE);
		if (!ConsoleExisted)
			AllocConsole();
		else
			printf("\n");

		freopen("CON", "w", stdout);
		freopen("CON", "r", stdin);

		SetConsoleOutputCP(CP_ACP);
		SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_INSERT_MODE);

		int ret;
		if (argc >= 2 && !_tcsicmp(argv[1], _T("/install")))
		{
			bool oldVersionInstalled;
			if (IsDriverInstallationPending(&oldVersionInstalled))
			{
				BazisLib::ActionStatus st;

				for (int i = 0; i < 2; i++)
				{
					st = InstallDriver(oldVersionInstalled);
					if (!st.Successful() && !i)
						break;

					st = StartDriverIfNeeded();
					if ((st.ConvertToHResult() == HRESULT_FROM_WIN32(ERROR_SERVICE_DISABLED)) && !i)
						continue;
				}

				ret = st.GetErrorCode();
				printf("%S\n", st.GetMostInformativeText().c_str());
			}
			else
			{
				ret = 0;
				printf("The driver is already installed\n");
			}
		}
		else if (argc >= 2 && !_tcsicmp(argv[1], _T("/uninstall")))
		{
			BazisLib::ActionStatus st;

			{
				VirtualCDClient clt(&st, true, false);
				clt.DisconnectDisk(L"*");
			}

			st = UninstallDriver();
			ret = st.GetErrorCode();
			printf("%S\n", st.GetMostInformativeText().c_str());
		}
		else
			ret = BatchMounterMain(argc, argv, true);

		LocalFree(argv);
		FreeConsole();
		return ret;
	}


	HRESULT hRes = ::CoInitialize(NULL);
// If you are running on NT 4.0 or higher you can use the following call instead to 
// make the EXE free threaded. This means that calls come in on a random RPC thread.
//	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	EnableFileDragDrop();

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	int nRet = 0;
	// BLOCK: Run application
	{
		CMainDlg dlgMain;
		nRet = dlgMain.DoModal();
	}

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
