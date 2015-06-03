#include "StdAfx.h"
#include "CustomInstaller.h"
#include "resource.h"

BOOL CALLBACK CustomInstaller::sInstallerDialogProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	if (wMsg == WM_INITDIALOG)
		SetWindowLong(hWnd, DWL_USER, lParam);

	CustomInstaller *pInst = (CustomInstaller *)GetWindowLong(hWnd, DWL_USER);
	if (pInst)
		return pInst->InstallerDialogProc(hWnd, wMsg, wParam, lParam);
	else
		return FALSE;
}

bool CustomInstaller::PerformInstallation(const char *ComponentOverrideString /*= 0*/, bool Unattended /*= false*/)
{
	m_bUnattended = Unattended;
	if (!m_bReadyForInstall)
		return false;
	if (m_Header.strDefaultDir)
	{
		if (!g_pPathSub->PerformPathSubstitution(m_Strings.GetStringByHandle(m_Header.strDefaultDir),m_szInstallationDir,sizeof(m_szInstallationDir),true))
		{
			g_pErrorMgr->Error(ERROR_SYNTAX_ERROR,
				ERROR_SYN_INCORRECT_PATH,
				ERROR_CRITICAL,
				m_Strings.GetStringByHandle(m_Header.strDefaultDir));
			return false;
		}
	}
	g_pPathSub->SetMacroValue(IDMACRO_DESTDIR,m_szInstallationDir);
	if (m_Header.InstallationFlags & INSTFLAG_TESTSIGNING)
		if (!EnsureTestsigningEnabled())
			return true;
	if (m_Header.InstallationFlags & INSTFLAG_NOTESTSIGN)
		if (!ProposeDisablingTestsigning())
			return true;
	if (!CheckAllComponentsInstallStatus())
		return false;
	if (!GenerateRuntimeStatusFlags(ComponentOverrideString))
		return false;

	if(m_Header.InstallationFlags & INSTFLAG_SELECT_COMPONENTS)
		MessageBox(0, "Component selection not supported!", NULL, MB_ICONERROR);

	if (!GetSelectedComponentStats())
		return false;

	DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_INSTALLDLG), HWND_DESKTOP, sInstallerDialogProc, (LPARAM)this);

	return true;
}

/*static int CALLBACK FontEnumProc (CONST LOGFONTA *pFont, CONST TEXTMETRICA *pMetric, DWORD FontType, LPARAM lParam)
{
	int *p = (int *)lParam;
	(*p)++;
	return 1;
}

void Test(HWND hWnd)
{
	HDC hDC = GetDC(hWnd);
	int count = 0;
	EnumFontFamilies(hDC, "Calibri", FontEnumProc, (LPARAM)&count);
	EnumFontFamilies(hDC, NULL, FontEnumProc, (LPARAM)&count);
	ReleaseDC(hWnd, hDC);
}*/

#include <ShellAPI.h>

BOOL CustomInstaller::InstallerDialogProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	switch(wMsg)
	{
	case WM_INITDIALOG:
		{
			m_hWnd = hWnd;
			HICON hIcon = (HICON)::LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON2), 
				IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
			SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
			SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

			SendDlgItemMessage(m_hWnd, IDC_PROGRESS1, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
			HFONT hFont = CreateFont(-48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ANTIALIASED_QUALITY, 0, _T("Calibri"));
			SendDlgItemMessage(hWnd, IDC_CAPTION, WM_SETFONT, (WPARAM)hFont, TRUE);
			SetDlgItemText(hWnd, IDC_CAPTION, BazisLib::DynamicStringA::sFormat("WinCDEmu %d.%d", (m_Header.InstallationVersion >> 8) & 0xFF, m_Header.InstallationVersion & 0xFF).c_str());
			SetDlgItemText(hWnd, IDC_EDIT1, m_Strings.GetStringByHandle(m_Header.strReadme));
			ShowExtendedOptions(false);
			SetDlgItemText(hWnd, IDC_INSTALLDIR, m_szInstallationDir);
			if (m_bUnattended)
				PostMessage(hWnd, WM_COMMAND, IDOK, 0);
		}
		break;
	case WM_CLOSE:
		EndDialog(hWnd, IDCANCEL);
		break;
	case WM_COMMAND:
		switch(wParam)
		{
		case IDOK:
			GetDlgItemText(hWnd, IDC_INSTALLDIR, m_szInstallationDir, sizeof(m_szInstallationDir));
			UpdateInstallationState(true);
			g_pPathSub->SetMacroValue(IDMACRO_DESTDIR,m_szInstallationDir);
			m_InstallThread.Start();
			break;
		case IDCANCEL:
			EndDialog(hWnd, IDCANCEL);
			break;
		case IDC_CHECK1:
			ShowExtendedOptions(SendDlgItemMessage(hWnd, IDC_CHECK1, BM_GETCHECK, 0, 0) & BST_CHECKED);
			break;
		}
		break;
	case WMX_DONE:
		if (wParam) 
		{
			if (!m_bUnattended)
				MessageBox(hWnd, "Installation complete", "Information", MB_ICONINFORMATION);
		}
		else
		{
			if(!m_LastPostInstallError.empty())
				MessageBox(hWnd, m_LastPostInstallError.c_str(), NULL, MB_ICONERROR);
			else
				MessageBox(hWnd, "Installation failed", NULL, MB_ICONERROR);
		}
		EndDialog(m_hWnd, wParam ? IDOK : IDCANCEL);
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case NM_CLICK:
			if (((LPNMHDR)lParam)->idFrom == IDC_SYSLINK2)
			{
				ShellExecute(hWnd, "open", "http://wincdemu.sysprogs.org/", NULL, NULL, SW_SHOW);
			}
			break; 
		}
		break;
	}
	return FALSE;
}

DWORD _stdcall FileExtractionThreadBody(LPVOID lp);

int CustomInstaller::InstallationThread(LPVOID lpParam)
{
	CustomInstaller *pInstaller = (CustomInstaller *)lpParam;
	pInstaller->m_hProgressDlg = NULL;

	bool bSuccess = pInstaller->DecompressFiles();
	if (bSuccess)
	{
		HWND hProgressBar = GetDlgItem(pInstaller->m_hWnd, IDC_PROGRESS1);
		DWORD dwStyle = GetWindowLong(hProgressBar, GWL_STYLE);
		SetWindowLong(hProgressBar, GWL_STYLE, dwStyle | 8);
		SendDlgItemMessage(pInstaller->m_hWnd, IDC_PROGRESS1, PBM_SETMARQUEE, TRUE, 100);
		SetDlgItemText(pInstaller->m_hWnd, IDC_INSTSTATUS, "Updating configuration...");
		bSuccess = pInstaller->ProcessRegistryAndStartMenuEntries();
	}

	if (bSuccess)
	{
		pInstaller->ApplyCustomSettings();
	}

	PostMessage(pInstaller->m_hWnd, WMX_DONE, bSuccess, 0);
	return bSuccess;
}

void CustomInstaller::UpdateInstallationState(bool running)
{
	static const int dlgIDs[] = {IDOK, IDCANCEL};
	for each(int ID in dlgIDs)
		EnableWindow(GetDlgItem(m_hWnd, ID), !running);
	SetFocus(GetDlgItem(m_hWnd, IDC_SYSLINK2));
}

bool CustomInstaller::OnPartialCompletion(int FileIndex, ULONGLONG FileSize, ULONGLONG BytesDone, ULONGLONG TotalSize, ULONGLONG TotalDone)
{
	unsigned progress = (unsigned)((TotalDone * 1000) / TotalSize);
	if (m_hWnd)
		SendDlgItemMessage(m_hWnd, IDC_PROGRESS1, PBM_SETPOS, progress, 0);
	return true;
}

bool CustomInstaller::OnNextFile(char *pszFileName)
{
	if (pszFileName && m_hWnd)
		SetDlgItemText(m_hWnd, IDC_INSTSTATUS, BazisLib::DynamicStringA::sFormat("Extracting %s...", pszFileName).c_str());
	return true;
}

void CustomInstaller::OnPostInstallCommand(PCOMMAND pCommand)
{
	if (!pCommand)
		return;
	switch (pCommand->CommandID & 0xFF0000)
	{
	case SEQUENCE_REGISTRY:
		SetDlgItemText(m_hWnd, IDC_INSTSTATUS, "Updating registry...");
		break;
	case SEQUENCE_STARTMENU:
		SetDlgItemText(m_hWnd, IDC_INSTSTATUS, "Updating start menu...");
		break;
	}

	switch(pCommand->CommandID)
	{
	case COMMAND_INSTROOT:
	case COMMAND_COPYINF:
	case COMMAND_UPDATEDRV:
		SetDlgItemText(m_hWnd, IDC_INSTSTATUS, "Installing drivers...");
		break;
	case COMMAND_REGSVR:
		SetDlgItemText(m_hWnd, IDC_INSTSTATUS, "Registering shell extensions...");
		break;
	case COMMAND_EXECCMDLINE:
	case COMMAND_MKDIR:
	case COMMAND_PARSETEXTFILE:
		SetDlgItemText(m_hWnd, IDC_INSTSTATUS, "Finalizing installation...");
		break;
	}
}

static void MoveWnd(HWND hWnd, int deltaY)
{
	RECT rect;
	GetWindowRect(hWnd, &rect);
	POINT clientPoint = {rect.left, rect.top};
	ScreenToClient(GetParent(hWnd), &clientPoint);
	MoveWindow(hWnd, clientPoint.x, clientPoint.y + deltaY, rect.right - rect.left, rect.bottom - rect.top, TRUE);
}

void CustomInstaller::ShowExtendedOptions(bool bShow)
{
	if (bShow == m_bExtOptionsVisible)
		return;

	RECT groupRect = {0,}, wndRect = {0,};
	GetWindowRect(GetDlgItem(m_hWnd, IDC_GROUPBOX1), &groupRect);
	GetWindowRect(m_hWnd, &wndRect);

	int delta = groupRect.bottom - groupRect.top;
	if (!bShow)
		delta = -delta;
	MoveWindow(m_hWnd, wndRect.left, wndRect.top, wndRect.right - wndRect.left, wndRect.bottom - wndRect.top + delta, TRUE);

	static int movedIDs[] = {IDC_INSTSTATUS, IDC_PROGRESS1, IDOK, IDCANCEL};
	static int toggledIDs[] = {IDC_DIRLABEL, IDC_INSTALLDIR, IDC_CHECK2};
	for each(int id in movedIDs)
		MoveWnd(GetDlgItem(m_hWnd, id), delta);
	for each(int id in toggledIDs)
		ShowWindow(GetDlgItem(m_hWnd, id), bShow ? SW_SHOW : SW_HIDE);


	m_bExtOptionsVisible = bShow;
}

#include <bzshlp/Win32/process.h>
#include <bzshlp/Win32/wow64.h>

void CustomInstaller::ApplyCustomSettings()
{
	bool bNeedUAC = !!(SendDlgItemMessage(m_hWnd, IDC_CHECK2, BM_GETCHECK, 0, 0) & BST_CHECKED);
	if (!bNeedUAC)
	{
		char *px64Suffix = BazisLib::WOW64APIProvider::sIsWow64Process() ? "64" : "";
		SetDlgItemText(m_hWnd, IDC_INSTSTATUS, "Disabling UAC for image mounting...");
		BazisLib::Win32::Process::RunCommandLineSynchronously((LPTSTR)BazisLib::String::sFormat("%s\\vmnt%s /uacdisable", m_szInstallationDir, px64Suffix).c_str());
	}
}