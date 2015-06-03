#include "StdAfx.h"
#include "ISOBuildProgressDialog.h"
#include <bzshlp/Win32/i18n.h>
#include "../VirtualCDCtl/VirtualCDClient.h"

using namespace BazisLib;

enum { kProgressRangeMax = 10000 };

LRESULT CISOBuildProgressDialog::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
//	DlgResize_Init();
	LocalizeLabels();
	CenterWindow();
	BOOL unused;
	m_cbLetters.m_hWnd = GetDlgItem(IDC_DRIVELETTER);

	// set icons
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDI_ICON2),
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDI_ICON2),
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

	SetTimer(0, 100);

	SetDlgItemText(IDC_FOLDER, m_Folder.c_str());
	SetDlgItemText(IDC_ISOFILE, m_IsoFile.c_str());
	m_ProgressBar.m_hWnd = GetDlgItem(IDC_PROGRESS1);
	m_ProgressBar.SetRange(0, kProgressRangeMax);

	OnBnClickedDetails(0, 0, 0, unused);

	if (m_pTaskbar)
		m_pTaskbar->SetProgressState(m_hWnd, TBPF_NORMAL);

	m_cbLetters.Clear();

	DWORD dwLetterMask = GetLogicalDrives();
	unsigned char ch = 'C';
	TCHAR tsz [] = _T("X:");
	bool selUpdated = false;
	for (unsigned i = (1 << (ch - 'A')); ch <= 'Z'; ch++, i <<= 1)
	if (!(dwLetterMask & i))
	{
		tsz[0] = ch, m_cbLetters.InsertString(-1, tsz);
		if (((unsigned char) (ch - 'A') >= m_RegParams.StartingDriveLetterIndex) && !selUpdated)
			m_cbLetters.SetCurSel(m_cbLetters.GetCount() - 1), selUpdated = true;
	}

	SendDlgItemMessage(IDC_CLOSEWINDOW, BM_SETCHECK, m_ISORegParams.CloseWindowAutomatically ? BST_CHECKED : BST_UNCHECKED);
	SendDlgItemMessage(IDC_MOUNTISO, BM_SETCHECK, m_ISORegParams.MountISOAutomatically ? BST_CHECKED : BST_UNCHECKED);

	if (!m_cbLetters.GetCount())
	{
		::EnableWindow(GetDlgItem(IDC_MOUNTISO), FALSE);
		SendDlgItemMessage(IDC_MOUNTISO, BM_SETCHECK, BST_UNCHECKED);
	}
	else
	{
		if (!selUpdated)
			m_cbLetters.SetCurSel(0);
	}

	return 0;
}

void CISOBuildProgressDialog::SaveParams()
{
	m_ISORegParams.CloseWindowAutomatically = (SendDlgItemMessage(IDC_CLOSEWINDOW, BM_GETCHECK) == BST_CHECKED);
	m_ISORegParams.MountISOAutomatically = (SendDlgItemMessage(IDC_MOUNTISO, BM_GETCHECK) == BST_CHECKED);
	m_ISORegParams.SaveToRegistry();
}


LRESULT CISOBuildProgressDialog::OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	const size_t readChunkSize = 4096;
	bool textChanged = false;
	for (;;)
	{
		char* pBuf = m_AllOutput.PreAllocate(readChunkSize + m_AllOutput.length(), true) + m_AllOutput.length();
		DWORD done = 0;
		if (!ReadFile(m_hOutputPipe, pBuf, readChunkSize, &done, NULL) || done == 0)
			break;

		m_AllOutput.SetLength(m_AllOutput.length() + done);
		textChanged = true;
	}

	CEdit txtProgress(GetDlgItem(IDC_PROGRESSTEXT).m_hWnd);
	if (textChanged)
	{
		txtProgress.SetWindowText(BazisLib::ANSIStringToString(m_AllOutput).c_str());
		txtProgress.SetSel(m_AllOutput.length(), m_AllOutput.length(), FALSE);

		int idx = m_AllOutput.rfind("% done, estimate finish");
		if (idx != -1 && idx > m_LastProgressMessageOffset)
		{
			m_LastProgressMessageOffset = idx;
			int idx2 = m_AllOutput.rfind("\n", idx);
			if (idx2 != -1)
			{
				BazisLib::DynamicStringA strProgress = m_AllOutput.substr(idx2, idx - idx2);
				int start = 0;
				for (int i = 0; i < strProgress.size(); i++)
				{
					if (!start && strProgress[i] != '\n' && strProgress[i] != ' ' && strProgress[i] != '\t')
						start = i;						

					if (strProgress[i] == ',')
						strProgress[i] = '.';
				}

				double progress = atof(strProgress.c_str() + start);
				if (progress < 0)
					progress = 0;
				else if (progress > 100)
					progress = 100;

				int mappedProgress = (progress * kProgressRangeMax) / 100;
				m_ProgressBar.SetPos(mappedProgress);

				if (m_pTaskbar)
					m_pTaskbar->SetProgressValue(m_hWnd, mappedProgress, kProgressRangeMax);
			}
		}
	}

	if (!m_ExecStatus.Successful() || !m_pProcess->IsRunning())
	{
		KillTimer(0);
		int exitCode;
		bool keepWindow = false;
		if (!m_ExecStatus.Successful())
			MessageBox(String::sFormat(_TR(IDS_CANNOTEXECUTE, "Failed to execute %s: %s"), L"mkisofs.exe", m_ExecStatus.GetMostInformativeText().c_str()).c_str(), NULL, MB_ICONERROR);
		else if ((exitCode = m_pProcess->GetExitCode()) != 0)
		{
			BOOL unused;
			MessageBox(String::sFormat(_TR(IDS_MKISOFAILED, "%s reported an error. Examine the output log for more details."), L"mkisofs.exe").c_str(), NULL, MB_ICONERROR);
			if (!m_DetailsShown)
				OnBnClickedDetails(0, 0, 0, unused);
			keepWindow = true;
		}
		else
		{
			m_ProgressBar.SetPos(kProgressRangeMax);

			if (m_pTaskbar)
				m_pTaskbar->SetProgressValue(m_hWnd, kProgressRangeMax, kProgressRangeMax);

			if ((SendDlgItemMessage(IDC_CLOSEWINDOW, BM_GETCHECK) != BST_CHECKED))
			{
				MessageBox(_TR(IDS_ISOCREATED, "The ISO image has been created successfully"), _TR(IDS_INFORMATION, "Information"), MB_ICONINFORMATION);
				m_Succeeded = true;
				GetDlgItem(IDCANCEL).SetWindowText(_TR(IDS_CLOSE, "Close"));
				keepWindow = true;
			}

			if (SendDlgItemMessage(IDC_MOUNTISO, BM_GETCHECK) == BST_CHECKED)
			{
				wchar_t wszKernelPath[512];
				if (!VirtualCDClient::Win32FileNameToKernelFileName(m_IsoFile.c_str(), wszKernelPath, __countof(wszKernelPath)))
					MessageBox(_TR(IDS_BADIMGFN, "Invalid image file name!"), NULL, MB_ICONERROR);
				else
				{
					extern bool AutorunErrorHandler(ActionStatus st);

					TCHAR tsz[3] = { 0, };
					if (m_cbLetters.GetLBTextLen(m_cbLetters.GetCurSel()) < _countof(tsz))
						m_cbLetters.GetLBText(m_cbLetters.GetCurSel(), tsz);

					VirtualCDClient().ConnectDisk(wszKernelPath, (char) (tsz[0]), 0, m_RegParams.DisableAutorun, false, AutorunErrorHandler);
				}
			}

		}

		if (!keepWindow)
		{
			SaveParams();
			EndDialog(0);
		}
	}

	return 0;
}

LRESULT CISOBuildProgressDialog::OnBnClickedCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_pProcess)
	{
		if (!m_Succeeded)
		{
			TerminateProcess(m_pProcess->GetProcessHandle(), -1);
			m_pProcess->Wait(10000);
			DeleteFile(m_IsoFile.c_str());
		}

		SaveParams();
		EndDialog(-1);
	}
	return 0;
}

LRESULT CISOBuildProgressDialog::OnBnClickedDetails(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_DetailsShown = !m_DetailsShown;
	LONG style = GetWindowLong(GWL_STYLE);

	RECT progressTextRect, mainWindowRect;
	GetDlgItem(IDC_PROGRESSTEXT).GetWindowRect(&progressTextRect);
	GetWindowRect(&mainWindowRect);

	if (m_DetailsShown)
	{
		GetDlgItem(IDC_DETAILS).SetWindowText(L"<<<");
		//style |= WS_SIZEBOX;
		mainWindowRect.bottom = mainWindowRect.top + m_ExpandedHeight;
	}
	else
	{
		GetDlgItem(IDC_DETAILS).SetWindowText(L">>>");
		m_ExpandedHeight = mainWindowRect.bottom - mainWindowRect.top;
		mainWindowRect.bottom = mainWindowRect.top + (progressTextRect.top - mainWindowRect.top) + GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYSMCAPTION);
		//style &= ~WS_SIZEBOX;
		//m_ptMinTrackSize.y = mainWindowRect.bottom - mainWindowRect.top;
	}

	GetDlgItem(IDC_PROGRESSTEXT).ShowWindow(m_DetailsShown ? SW_SHOW : SW_HIDE);
	MoveWindow(&mainWindowRect);
	SetWindowLong(GWL_STYLE, style);
	return 0;
}

CISOBuildProgressDialog::CISOBuildProgressDialog(const BazisLib::String &mkisofsCommandLine, const BazisLib::String &isoFile, const BazisLib::String &folder) 
	: m_CommandLine(mkisofsCommandLine)
	, m_IsoFile(isoFile)
	, m_Folder(folder)
	, m_DetailsShown(true)
	, m_ExpandedHeight(0)
	, m_hOutputPipe(INVALID_HANDLE_VALUE)
	, m_pProcess(NULL)
	, m_LastProgressMessageOffset(0)
	, m_Succeeded(false)
{
	m_ExecStatus = MAKE_STATUS(NotInitialized);
	m_pTaskbar.CoCreateInstance(CLSID_TaskbarList);
	STARTUPINFO startupInfo = { sizeof(STARTUPINFO) };
	startupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_HIDE;
	HANDLE hStdOut = INVALID_HANDLE_VALUE;
	CreatePipe(&m_hOutputPipe, &hStdOut, NULL, 65536);

	DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
	SetNamedPipeHandleState(m_hOutputPipe, &mode, NULL, NULL);

	DuplicateHandle(GetCurrentProcess(), hStdOut, GetCurrentProcess(), &startupInfo.hStdOutput, GENERIC_ALL, TRUE, DUPLICATE_SAME_ACCESS);
	DuplicateHandle(GetCurrentProcess(), hStdOut, GetCurrentProcess(), &startupInfo.hStdError, GENERIC_ALL, TRUE, DUPLICATE_SAME_ACCESS);
	CloseHandle(hStdOut);

	m_pProcess = new Win32::Process(mkisofsCommandLine.c_str(), &startupInfo, true, &m_ExecStatus);
}

void CISOBuildProgressDialog::LocalizeLabels()
{
	LOCALIZE_DIALOG(IDS_CREATINGIMAGE, IDD_ISOBUILDPROGRESS, m_hWnd);
	LOCALIZE_DLGITEM(IDS_WHENDONE, IDC_WHENDONE, m_hWnd);
	LOCALIZE_DLGITEM(IDS_AUTOCLOSEWND, IDC_CLOSEWINDOW, m_hWnd);
	LOCALIZE_DLGITEM(IDS_MOUNTISOTO, IDC_MOUNTISO, m_hWnd);
	LOCALIZE_DLGITEM(IDS_CANCEL, IDCANCEL, m_hWnd);

	LOCALIZE_DLGITEM(IDS_FOLDER, IDC_FOLDERLABEL, m_hWnd);
	LOCALIZE_DLGITEM(IDS_ISOFILE, IDC_ISOFILELABEL, m_hWnd);
}
