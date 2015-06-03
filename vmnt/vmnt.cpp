#include "stdafx.h"
#include "../VirtualCDCtl/VirtualCDClient.h"
#include <newdev.h>
#include "LetterSelectionDialog.h"
#include "SettingsDialog.h"
#include <bzshlp/Win32/i18n.h>
#include <bzshlp/Win32/process.h>
#include "UACInvokerDialog.h"
#include "../VirtualCDCtl/DriveReadingThread.h"
#include "ISOProgressDialog.h"
#include "RegistryParams.h"
#include "ISOBuildProgressDialog.h"
#include <bzscore/Win32/registry.h>

using namespace std;
using namespace BazisLib;

CAppModule _Module;

int AppMain();

int __stdcall _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR lpCmdLine, int)
{
	HRESULT hRes = ::CoInitialize(NULL);
	// If you are running on NT 4.0 or higher you can use the following call instead to 
	// make the EXE free threaded. This means that calls come in on a random RPC thread.
	//	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

#ifdef BAZISLIB_LOCALIZATION_ENABLED
	RegistryParams params;
	params.LoadFromRegistry();
	InitializeLNGBasedTranslationEngine(params.Language);
#endif

	int nRet = AppMain();

#ifdef BAZISLIB_LOCALIZATION_ENABLED
	ShutdownTranslationEngine();
#endif

	_Module.Term();
	::CoUninitialize();
	return nRet;
}

int RunSelfElevated(LPCTSTR lpCmdLine)
{
	BazisLib::ActionStatus status;
	BazisLib::Win32::Process proc(lpCmdLine, _T("runas"), &status);
	if (!status.Successful())
	{
		MessageBox(HWND_DESKTOP, BazisLib::String::sFormat(_TR(IDS_UACFAIL, "Cannot run elevated process: %s\n"), status.GetMostInformativeText().c_str()).c_str(), NULL, MB_ICONERROR);
		return 1;
	}
	proc.Wait();
	return proc.GetExitCode();
}

int ShowSettingsDialog()
{
	ActionStatus status;
	RegistryKey key(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\BazisVirtualCDBus\\Parameters"), 0, true, &status);

	if (!key.Valid() && (status.GetErrorCode() == AccessDenied))
	{
		TCHAR tsz[MAX_PATH + 2] = {0,};
		tsz[0] = '\"';
		GetModuleFileName(GetModuleHandle(0), tsz + 1, _countof(tsz) - 1);
		size_t len = _tcslen(tsz);
		tsz[len] = '\"';
		tsz[len + 1] = 0;
		return (int)CUACInvokerDialog(tsz).DoModal();
	}
	else
	{
		CSettingsDialog dlg;
		dlg.DoModal();
	}
	return 0;
}

bool AutorunErrorHandler(ActionStatus st)
{
	return MessageBox(NULL, String::sFormat(_TR(IDS_AUTORUNHOOKFAILFMT, "Failed to disable autorun for this image (%s). Mount the image anyway?"), st.GetMostInformativeText().c_str()).c_str(),
		NULL, MB_ICONERROR | MB_YESNO) == IDYES;
}

static ActionStatus BuildISOFromFolder(const wchar_t *pwszFolder)
{
	const TCHAR *pwszFilter = _TR(IDS_ISOFILTER, "ISO images (*.iso)|*.iso|All files (*.*)|*.*");
	TCHAR tszFilter[128] = { 0, };
	_tcsncpy(tszFilter, pwszFilter, _countof(tszFilter) - 1);
	for (size_t i = 0; i < _countof(tszFilter); i++)
	if (tszFilter[i] == '|')
		tszFilter[i] = '\0';

	CFileDialog dlg(false, _T("iso"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, tszFilter);
	dlg.m_ofn.lpstrTitle = _TR(IDS_CREATEISO_DLGTITLE, "Create ISO image");

	if (dlg.DoModal() != IDOK)
		return MAKE_STATUS(OperationAborted);

	TCHAR tsz[MAX_PATH] = { 0, };
	GetModuleFileName(GetModuleHandle(0), tsz, _countof(tsz) - 1);
	TCHAR *pSlash = _tcsrchr(tsz, '\\');
	if (!pSlash)
		return MAKE_STATUS(UnknownError);
	pSlash[1] = 0;
	_tcscat_s(tsz, _T("mkisofs.exe"));

	String mkisofsOptions;
	RegistryKey key(HKEY_LOCAL_MACHINE, tszWinCDEmuRegistryPath, 0, false);
	if (!key[_T("MkISOFsFlags")].ReadValue(&mkisofsOptions).Successful())
		mkisofsOptions = _T("-r -l");

	String mkisofsCommandLine = String::sFormat(L"\"%s\" %s -o \"%s\" \"%s\"", tsz, mkisofsOptions.c_str(), dlg.m_szFileName, pwszFolder);

	DeleteFile(dlg.m_szFileName);

	CISOBuildProgressDialog progressDlg(mkisofsCommandLine, dlg.m_szFileName, pwszFolder);
	progressDlg.DoModal();
	return MAKE_STATUS(Success);
}


static ActionStatus CreateISOFile(const wchar_t *pwszDrivePath)
{
	ActionStatus st;
	TCHAR tszPath[] = _T("\\\\.\\?:");
	tszPath[4] = pwszDrivePath[0];

	BackgroundDriveReader driveReader(tszPath, ISOCreatorRegistryParams().ReadBufferSize, &st);
	if (!st.Successful())
		return st;

	if (!driveReader.GetVolumeSize())
		return MAKE_STATUS(ReadError);

	BackgroundDriveReader::TOCError additionalError = BackgroundDriveReader::teCannotReadTOC;
	st = driveReader.CheckTOC(&additionalError);

	String err;
	if (!st.Successful())
		switch(additionalError)
		{
		case BackgroundDriveReader::teCannotReadTOC:
			return st;
		case BackgroundDriveReader::teMultipleTracks:
			err.Format(_TR(IDS_MULTITRACK, "The CD inserted in %s has multiple tracks. The ISO image may be completely or partially unreadable. Do you want to continue?"), pwszDrivePath);
			if (MessageBox(0, err.c_str(), _T("WinCDEmu"), MB_ICONQUESTION | MB_YESNO) != IDYES)
				return MAKE_STATUS(OperationAborted);
			break;
		case BackgroundDriveReader::teNonDataTrack:
			err.Format(_TR(IDS_NONDATATRACK, "The CD inserted in %s is not a regular data CD. The ISO image will most likely be unreadable. Do you want to continue?"), pwszDrivePath);
			if (MessageBox(0, err.c_str(), _T("WinCDEmu"), MB_ICONQUESTION | MB_YESNO) != IDYES)
				return MAKE_STATUS(OperationAborted);
			break;
		}


	//TODO: start reading asynchronously

	const TCHAR *pwszFilter = _TR(IDS_ISOFILTER, "ISO images (*.iso)|*.iso|All files (*.*)|*.*");
	TCHAR tszFilter[128] = {0,};
	_tcsncpy(tszFilter, pwszFilter, _countof(tszFilter) - 1);
	for (size_t i = 0; i < _countof(tszFilter); i++)
		if (tszFilter[i] == '|')
			tszFilter[i] = '\0';


	CFileDialog dlg(false, _T("iso"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, tszFilter);
	dlg.m_ofn.lpstrTitle = _TR(IDS_CREATEISO_DLGTITLE, "Create ISO image");

	for (;;)
	{
		if (dlg.DoModal() != IDOK)
			return MAKE_STATUS(OperationAborted);

		String rootFolder(dlg.m_szFileName, 3);
		ULARGE_INTEGER bytesAvailable = {0,};
		if (!GetDiskFreeSpaceEx(rootFolder.c_str(), &bytesAvailable, NULL, NULL))
			break;

		if (bytesAvailable.QuadPart < (driveReader.GetVolumeSize() + (1024 * 1024)))
			switch (MessageBox(NULL, 
						String::sFormat(_TR(IDS_LOWSPACEFMT, "Insufficient free space on %s (%sB). You need at least %sB to create an image of %s."), 
							rootFolder.c_str(),
							RateCalculator::FormatByteCount(bytesAvailable.QuadPart).c_str(),
							RateCalculator::FormatByteCount(driveReader.GetVolumeSize() + (1024 * 1024)).c_str(),
							pwszDrivePath).c_str(), 
						_TR(IDS_LOWSPACE, "Insufficient disk space"), 
						MB_ICONWARNING | MB_ABORTRETRYIGNORE))
		{
			case IDABORT:
				return MAKE_STATUS(OperationAborted);
			case IDRETRY:
				continue;
		}
		break;
	}

	st = driveReader.SetTargetFile(dlg.m_szFileName);
	if (!st.Successful())
		return st;

	st = driveReader.StartReading();
	if (!st.Successful())
		return st;

	CISOProgressDialog progressDlg(&driveReader);
	driveReader.SetErrorHandler(&progressDlg);
	if (progressDlg.DoModal() == IDNO)
		return MAKE_STATUS(OperationAborted);	//The error message has been displayed by the dialog
	driveReader.SetErrorHandler(NULL);

	driveReader.AbortIfStillRunning();

	return driveReader.GetFinalStatus();
}


#include "../VirtualCDCtl/CDSpeed.h"
#include "../ImageFormats/ImageFormats.h"
#include "../ImageFormats/UDFAnalyzer.h"

using namespace ImageFormats;

int AppMain()
{
	int argc;
	LPCWSTR lpCmdLine = GetCommandLineW();
	LPWSTR *ppArgv = CommandLineToArgvW(lpCmdLine, &argc);
	LPCWSTR lpDiskPath = NULL;
	bool ShowSettingsDialog = false, ForceMountOptions = false, AdminModeOnNetworkShare = false;
	bool bDisableUAC = false;
	char DriveLetterToRemount = 0;
	BazisLib::String tmpString;

	for (int i = 1; i < argc; i++)
	{
		if (ppArgv[i][0] != '/')
		{
			if (!lpDiskPath)
				lpDiskPath = ppArgv[i];
		}
		else
		{
			if (!_wcsicmp(ppArgv[i], L"/settings"))
				ShowSettingsDialog = true;
			else if (!_wcsicmp(ppArgv[i], L"/ltrselect"))
				ForceMountOptions = true;
			else if (!_wcsicmp(ppArgv[i], L"/uac_on_network_share"))
				AdminModeOnNetworkShare = true;
			else if (!_wcsicmp(ppArgv[i], L"/uacdisable"))
				bDisableUAC = true;
			else if (!_wcsicmp(ppArgv[i], L"/createiso") || !_wcsicmp(ppArgv[i], L"/isofromfolder"))
			{
				if (argc < (i + 2))
				{
					MessageBox(HWND_DESKTOP, _TR(IDS_BADCMDLINE, "Invalid command line!"), NULL, MB_ICONERROR);
					return 1;
				}
				ActionStatus st;
				if (!_wcsicmp(ppArgv[i], L"/isofromfolder"))
				{
					wchar_t *pwszFolder = ppArgv[i + 1];
					if (pwszFolder[0] && pwszFolder[wcslen(pwszFolder) - 1] == '\"')
						pwszFolder[wcslen(pwszFolder) - 1] = '\\';

					st = BuildISOFromFolder(pwszFolder);
				}
				else
					st = CreateISOFile(ppArgv[i + 1]);

				if (!st.Successful() && st.GetErrorCode() != OperationAborted)
					MessageBox(HWND_DESKTOP, st.GetMostInformativeText().c_str(), NULL, MB_ICONERROR);
				return !st.Successful();
			}
			else if (!_wcsnicmp(ppArgv[i], L"/remount:", 9))
				DriveLetterToRemount = (char)ppArgv[i][9];
		}
	}

	if (bDisableUAC)
	{
		RegistryKey driverParametersKey(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\BazisVirtualCDBus\\Parameters"));
		int prevVal = driverParametersKey[_T("GrantAccessToEveryone")];
		int newVal = 1;
		driverParametersKey[_T("GrantAccessToEveryone")] = newVal;

		if (prevVal != newVal)
			VirtualCDClient::RestartDriver();

		return 0;
	}

	if (GetKeyState(VK_SHIFT) & (1 << 31))
		ForceMountOptions = true;

	if (!lpDiskPath && DriveLetterToRemount)
	{
		const TCHAR *pwszFilter = _TR(IDS_ISOFILTER, "ISO images (*.iso)|*.iso|All files (*.*)|*.*");
		TCHAR tszFilter[128] = { 0, };
		_tcsncpy(tszFilter, pwszFilter, _countof(tszFilter) - 1);
		for (size_t i = 0; i < _countof(tszFilter); i++)
		if (tszFilter[i] == '|')
			tszFilter[i] = '\0';

		CFileDialog dlg(true, _T("iso"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, tszFilter);
		dlg.m_ofn.lpstrTitle = _TR(IDS_MOUNTIMAGE, "Mount a disc image");

		if (dlg.DoModal() != IDOK)
			return 1;

		tmpString.assign(dlg.m_szFileName);
		lpDiskPath = tmpString.c_str();
	}

	if ((argc < 2) || !ppArgv || !lpDiskPath || ShowSettingsDialog)
	{
		return ::ShowSettingsDialog();
	}

	MMCProfile detectedProfile = mpInvalid;
	bool bFileOnNetworkShare = false;

	{
		TCHAR tszFullPath[MAX_PATH] = {0,};
		GetFullPathName(lpDiskPath, __countof(tszFullPath), tszFullPath, NULL);
		TCHAR tszRoot[4] = {tszFullPath[0], ':', '\\', 0};
		if (GetDriveType(tszRoot) == DRIVE_REMOTE)
			bFileOnNetworkShare = true;

		ImageFormatDatabase imageFormatDB;
		ManagedPointer<AIParsedCDImage> pImage = imageFormatDB.OpenCDImage(ManagedPointer<AIFile>(new ACFile(tszFullPath, FileModes::OpenReadOnly.ShareAll())), tszFullPath);
		if (!pImage)
		{
			if (AdminModeOnNetworkShare)
				MessageBox(HWND_DESKTOP, _TR(IDS_UACNETWORKWARNING, "Cannot open image file. To mount images from network drives, go to WinCDEmu settings and allow mounting drives without administrator privileges."), NULL, MB_ICONERROR);
			else
				MessageBox(HWND_DESKTOP, _TR(ISD_CORRUPTIMAGE, "Unknown or corrupt image file!"), NULL, MB_ICONERROR);
			return 1;
		}
		if (pImage->GetTrackCount() != 1)
			detectedProfile = mpCdrom;
		else
		{
			UDFAnalysisResult result = AnalyzeUDFImage(pImage);
			if (!result.isUDF)
				detectedProfile = mpInvalid;
			else if (result.foundBDMV)
				detectedProfile = mpBDRSequentialWritable;
			else if (result.foundVIDEO_TS)
				detectedProfile = mpDvdRecordable;
		}

		/*if ((detectedProfile == mpCdrom) || (detectedProfile == mpInvalid))
		{
			if (pImage->GetSectorCount() >= ((10LL * 1024LL * 1024LL * 1024LL / 2048)))
				detectedProfile = mpBDRSequentialWritable;
			else if (pImage->GetSectorCount() >= ((1LL * 1024LL * 1024LL * 1024LL / 2048)))
				detectedProfile = mpDvdRecordable;
		}*/
	}

	wchar_t wszKernelPath[512];
	if (!VirtualCDClient::Win32FileNameToKernelFileName(lpDiskPath, wszKernelPath, __countof(wszKernelPath)))
	{
		MessageBox(HWND_DESKTOP, _TR(IDS_BADIMGFN, "Invalid image file name!"), NULL, MB_ICONERROR);
		return 1;
	}

	bool bDiskConnected = false;
	ActionStatus status;

	{
		VirtualCDClient clt(&status);
		if (!clt.Valid())
		{
			if (status.GetErrorCode() == BazisLib::AccessDenied)
			{
				if (bFileOnNetworkShare)
					return (int)CUACInvokerDialog((String(GetCommandLine()) + L" /uac_on_network_share").c_str()).DoModal();
				else
					return (int)CUACInvokerDialog(GetCommandLine()).DoModal();
			}

			MessageBox(HWND_DESKTOP, _TR(IDS_NOBUS, "Cannot connect to BazisVirtualCD.sys!"), NULL, MB_ICONERROR);
			return 1;
		}


		bDiskConnected = clt.IsDiskConnected(wszKernelPath);
	}

	if (DriveLetterToRemount != 0)
	{
		if (bDiskConnected)
		{
			MessageBox(HWND_DESKTOP, String::sFormat(_TR(IDS_ALREADYMOUNTED, "%s is already mounted."), lpDiskPath).c_str(), NULL, MB_ICONERROR);
			status = MAKE_STATUS(OperationAborted);
		}
		else
			status = VirtualCDClient::MountImageOnExistingDrive(wszKernelPath, DriveLetterToRemount);
	}
	else if (!bDiskConnected)
	{
		RegistryParams params;

		char driveLetter = 0;
		bool disableAutorun = false, persistent = false;
		if ((params.DriveLetterPolicy == drvlAskEveryTime) || ForceMountOptions)
		{
			CLetterSelectionDialog dlg(ForceMountOptions, detectedProfile);
			if (dlg.DoModal() != IDOK)
				return 3;
			driveLetter = dlg.GetDriveLetter();
			disableAutorun = dlg.IsAutorunDisabled();
			persistent = dlg.IsPersistent();
			detectedProfile = dlg.GetMMCProfile();

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
			if (dlg.IsDebugLoggingEnabled())
			{
				FilePath fpDir = FilePath::GetSpecialDirectoryLocation(SpecialDirFromCSIDL(CSIDL_APPDATA)).PlusPath(L"WinCDEmu.debug");
				bool cancel = false;
				if (!Directory::Exists(fpDir))
					cancel = MessageBox(0, String::sFormat(L"WinCDEmu will create a directory for debugging log files (%s). If you encounter problems using WinCDEmu, please submit the most recent log file to support@sysprogs.org. Continue?", fpDir.c_str()).c_str() , L"Information", MB_OKCANCEL | MB_ICONINFORMATION) != IDOK;
				if (!cancel)
				{
					CreateDirectory(fpDir.c_str(), NULL);
					wchar_t wszKernelDirPath[512];
					if (VirtualCDClient::Win32FileNameToKernelFileName(fpDir.c_str(), wszKernelDirPath, __countof(wszKernelDirPath)))
						VirtualCDClient().SetDebugLogDirectory(wszKernelDirPath);
				}
			}
			else
				VirtualCDClient().SetDebugLogDirectory(NULL);
#endif
		}
		else
		{
			disableAutorun = params.DisableAutorun;
			persistent = params.PersistentDefault;
			if (detectedProfile == mpInvalid)
				detectedProfile = (MMCProfile)params.DefaultMMCProfile;
			if (params.DriveLetterPolicy == drvlStartingFromGiven)
			{
				DWORD dwMask = GetLogicalDrives();
				unsigned char ch = 'A' + params.StartingDriveLetterIndex;
				for (unsigned i = (1 << (ch - 'A')); ch <= 'Z'; ch++, i <<= 1)
					if (!(dwMask & i))
					{
						driveLetter = ch;
						break;
					}

			}
		}
		status = VirtualCDClient().ConnectDisk(wszKernelPath, driveLetter, 0, disableAutorun, persistent, AutorunErrorHandler, detectedProfile);
	}
	else
		status = VirtualCDClient().DisconnectDisk(wszKernelPath);

	if (!status.Successful())
	{
		if (status.GetErrorCode() != OperationAborted)
			MessageBox(HWND_DESKTOP, status.GetMostInformativeText().c_str(), NULL, MB_ICONERROR);
		return 1;
	}
	return 0;
}