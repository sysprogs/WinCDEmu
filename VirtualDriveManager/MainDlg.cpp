// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "MainDlg.h"
#include "DriverInstaller.h"
#include <algorithm>

using namespace BazisLib;

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// center the dialog on the screen
	CenterWindow();
	BOOL unused;

	SetWindowText(BazisLib::String::sFormat(L"Portable WinCDEmu %d.%d", (WINCDEMU_VER_INT >> 24) & 0xFF, (WINCDEMU_VER_INT >> 16) & 0xFF).c_str());
	DragAcceptFiles(TRUE);

	// set icons
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

	DlgResize_Init();

	m_ListView.m_hWnd = GetDlgItem(IDC_LIST1);
	OnSelChanged(0, NULL, unused);

	m_ListView.AddColumn(_T("Drive"), 0);
	m_ListView.AddColumn(_T("Image"), 1);
	m_ListView.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);
	
	m_ImageList.Create(16, 16, ILC_COLOR24 | ILC_MASK, 0, 100);
	m_ImageList.AddIcon(LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDR_MAINFRAME)));
	m_ListView.SetImageList(m_ImageList.m_hImageList, LVSIL_SMALL);

	m_ListView.SetColumnWidth(0, 60);
	m_ListView.SetColumnWidth(1, 400);
	ActionStatus st;

	for (int iter = 0; iter < 2; iter++)
	{
		bool oldVersionInstalled = false;
		if (IsDriverInstallationPending(&oldVersionInstalled))
		{
			if (!oldVersionInstalled && (MessageBox(_T("Portable WinCDEmu will now install its driver, that is required to create virtual CD/DVD drives. You can uninstall the driver at any moment using this program. Do you wish to continue?"), _T("WinCDEmu Portable"), MB_ICONINFORMATION | MB_YESNO) != IDYES))
			{
				EndDialog(0);
				return 0;
			}
			ActionStatus st = InstallDriver(oldVersionInstalled);
			if (!st.Successful())
			{
				MessageBox(st.GetMostInformativeText().c_str(), _T("Driver installation failed"), MB_ICONERROR);
				EndDialog(0);
				return 0;
			}
		}

		st = StartDriverIfNeeded();
		if ((st.ConvertToHResult() == HRESULT_FROM_WIN32(ERROR_SERVICE_DISABLED)) && !iter)
			continue;
		
		if (!st.Successful())
		{
			MessageBox(st.GetMostInformativeText().c_str(), _T("Failed to start driver"), MB_ICONERROR);
			return TRUE;
		}
		break;
	}

	m_pClient = new VirtualCDClient(&st, true);
	if (!st.Successful())
	{
		delete m_pClient;
		m_pClient = NULL;
		MessageBox(st.GetMostInformativeText().c_str(), _T("Failed to connect to driver"), MB_ICONERROR);
	}

	m_pNonPortableClient = new VirtualCDClient(&st, false);
	if (!st.Successful())
	{
		delete m_pNonPortableClient;
		m_pNonPortableClient = NULL;
	}

	SetTimer(0, 100);

	return TRUE;
}

LRESULT CMainDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// TODO: Add validation code 
	EndDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnBnClickedUninstall(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	DoUnmountAll(true);
	ActionStatus st = UninstallDriver();
	MessageBox(st.GetMostInformativeText().c_str(), _T("Driver uninstallation"), st.Successful() ? MB_ICONINFORMATION : MB_ICONERROR);
	EndDialog(0);
	return 0;
}

LRESULT CMainDlg::OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (m_pClient)
	{
		VirtualCDClient::VirtualCDList lst = m_pClient->GetVirtualDiskList();
		if (m_pNonPortableClient)
		{
			VirtualCDClient::VirtualCDList nplst = m_pNonPortableClient->GetVirtualDiskList();
			for(size_t i = 0; i < nplst.size(); i++)
				lst.push_back(nplst[i]);

			std::sort(lst.begin(), lst.end());
		}


		if (m_LastCDList != lst)
		{
			m_LastCDList = lst;
			m_ListView.DeleteAllItems();
			int i = 0;
			for each(VirtualCDClient::VirtualCDEntry entry in lst)
			{
				m_ListView.AddItem(i, 0, String::sFormat(L"%c:\\", entry.DriveLetter).c_str(), 0);
				m_ListView.AddItem(i, 1, entry.ImageFile.c_str());
				i++;
			}

			BOOL unused;
			OnSelChanged(0, NULL, unused);
		}
	}

	return 0;
}

static BazisLib::String BrowseForImage()
{
	CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY, _T("All images (*.iso;*.cue;*.img;*.nrg;*.mds;*.ccd;*.bin)\0*.iso;*.cue;*.img;*.nrg;*.mds;*.ccd;*.bin\0\0"));
	if (dlg.DoModal() == IDOK)
	{
		return dlg.m_szFileName;
	}
	return String();
}

LRESULT CMainDlg::OnBnClickedMountnew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!m_pClient)
		return 0;
	String filePath = BrowseForImage();
	if (!filePath.empty())
	{
		wchar_t wszFullPath[MAX_PATH] = {0,};
		VirtualCDClient::Win32FileNameToKernelFileName(filePath.c_str(), wszFullPath, __countof(wszFullPath));

		ActionStatus st = m_pClient->ConnectDisk(wszFullPath, 0, 10000);
		if (!st.Successful())
			MessageBox(st.GetMostInformativeText().c_str(), _T("Error"), MB_ICONERROR);
	}
	return 0;
}

LRESULT CMainDlg::OnBnClickedUnmount(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!m_pClient)
		return 0;

	int sel = m_ListView.GetSelectedIndex();
	if (sel == -1)
		return 0;

	TCHAR tszPath[MAX_PATH] = {0,};
	m_ListView.GetItemText(sel, 1, tszPath, __countof(tszPath));

	ActionStatus st = DoUnmount(tszPath);
	if (!st.Successful())
		MessageBox(st.GetMostInformativeText().c_str(), _T("Error"), MB_ICONERROR);
	return 0;
}

LRESULT CMainDlg::OnBnClickedUnmountall(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	DoUnmountAll(false);
	return 0;
}

bool CMainDlg::DoUnmountAll(bool suppressErrorMessages)
{
	if (!m_pClient)
		return false;

	for (int i = 0; i < m_ListView.GetItemCount(); i++)
	{
		TCHAR tszPath[MAX_PATH] = {0,};
		m_ListView.GetItemText(i, 1, tszPath, __countof(tszPath));

		ActionStatus st = DoUnmount(tszPath);
		if (!st.Successful() && !suppressErrorMessages)
		{
			MessageBox(st.GetMostInformativeText().c_str(), _T("Error"), MB_ICONERROR);
			break;
		}
	}

	m_ListView.DeleteAllItems();
	return true;
}

BazisLib::ActionStatus CMainDlg::DoUnmount(LPCTSTR lpRoot)
{
	wchar_t wszFullPath[MAX_PATH] = {0,};
	VirtualCDClient::Win32FileNameToKernelFileName(lpRoot, wszFullPath, __countof(wszFullPath));
	ActionStatus st = m_pClient->DisconnectDisk(wszFullPath);
	if (st.ConvertToHResult() == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
		if (m_pNonPortableClient)
			st = m_pNonPortableClient->DisconnectDisk(wszFullPath);
	return st;
}

LRESULT CMainDlg::OnDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	unsigned nFiles = DragQueryFile((HDROP)wParam, -1, NULL, 0);
	for (unsigned i = 0; i < nFiles; i++)
	{
		BazisLib::String filePath;
		size_t done = DragQueryFile((HDROP) wParam, 0, filePath.PreAllocate(MAX_PATH, false), MAX_PATH);
		if (done)
		{
			filePath.SetLength(done);

			wchar_t wszFullPath[MAX_PATH] = { 0, };
			VirtualCDClient::Win32FileNameToKernelFileName(filePath.c_str(), wszFullPath, __countof(wszFullPath));

			ActionStatus st = m_pClient->ConnectDisk(wszFullPath, 0, 10000);
			if (!st.Successful())
				MessageBox(st.GetMostInformativeText().c_str(), _T("Error"), MB_ICONERROR);
		}
	}

	DragFinish((HDROP) wParam);
	return 0;
}

LRESULT CMainDlg::OnBnClickedReplaceImage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!m_pClient)
		return 0;

	int sel = m_ListView.GetSelectedIndex();
	if (sel == -1)
		return 0;

	TCHAR tszDriveLetter[MAX_PATH] = { 0, };
	m_ListView.GetItemText(sel, 0, tszDriveLetter, __countof(tszDriveLetter));

	String filePath = BrowseForImage();
	if (!filePath.empty())
	{
		wchar_t wszFullPath[MAX_PATH] = {0,};
		VirtualCDClient::Win32FileNameToKernelFileName(filePath.c_str(), wszFullPath, __countof(wszFullPath));

		if (m_pClient->IsDiskConnected(wszFullPath))
			MessageBox(String::sFormat(L"%s is already mounted", filePath.c_str()).c_str(), _T("Error"), MB_ICONERROR);
		else
		{
			ActionStatus st = VirtualCDClient::MountImageOnExistingDrive(wszFullPath, tszDriveLetter[0]);
			if (!st.Successful())
				MessageBox(st.GetMostInformativeText().c_str(), _T("Error"), MB_ICONERROR);
		}
	}
	return 0;
}

LRESULT CMainDlg::OnSelChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	bool selected = m_ListView.GetSelectedIndex() != -1;
	GetDlgItem(IDC_REPLACEIMG).EnableWindow(selected);
	GetDlgItem(IDC_UNMOUNT).EnableWindow(selected);
	GetDlgItem(IDC_UNMOUNTALL).EnableWindow(m_ListView.GetItemCount() > 0);
	return 0;
}
