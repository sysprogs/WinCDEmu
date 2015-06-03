#include "stdafx.h"
#include "LetterSelectionDialog.h"
#include "SettingsDialog.h"
#include <bzshlp/Win32/i18n.h>
#include "../VirtualCDCtl/VirtualCDClient.h"

static const struct DiscTypeDesc
{
	MMCProfile Profile;
	LPTSTR Label;
} s_DiscTypes[] = {
	{mpInvalid				    , _T("Undefined")},
	{mpCdrom                    , _T("CD-ROM")},
	{mpCdRecordable             , _T("CD-R")},
	{mpCdRewritable             , _T("CD-RW")},
	{mpDvdRom                   , _T("DVD-ROM")},
	{mpDvdRecordable            , _T("DVD-R")},
	{mpDvdRam                   , _T("DVD-RAM")},
	{mpDvdRewritable            , _T("DVD-RW")},
	{mpDvdRWSequential          , _T("DVD-RW SEQ")},
	{mpDvdPlusRW                , _T("DVD+RW")},
	{mpDvdPlusR                 , _T("DVD+R")},
	{mpDvdPlusRWDualLayer       , _T("DVD+RW DL")},
	{mpDvdPlusRDualLayer        , _T("DVD+R DL")},
	{mpBDRom                    , _T("BD-ROM")},
	{mpBDRSequentialWritable    , _T("BD-R SEQ")},
	{mpBDRRandomWritable        , _T("BD-R")},
	{mpBDRewritable             , _T("BD-RE")},
	{mpHDDVDRom                 , _T("HD-DVD-ROM")},
	{mpHDDVDRecordable          , _T("HD-DVD-R")},
	{mpHDDVDRam                 , _T("HD-DVD-RAM")},
	{mpHDDVDRewritable          , _T("HD-DVD-RW")},
	{mpHDDVDRDualLayer          , _T("HD-DVD-R DL")},
	{mpHDDVDRWDualLayer         , _T("HD-DVD-RW DL")},
};


LRESULT CLetterSelectionDialog::OnInitDialog( UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled )
{
	m_cbLetters.m_hWnd = GetDlgItem(IDC_DRIVELETTER);
	m_cbDiscTypes.m_hWnd = GetDlgItem(IDC_DISCTYPE);

	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDI_ICON2), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDI_ICON2), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);



	if (m_bForciblyInvoked)
		::EnableWindow(GetDlgItem(IDC_AUTOLETTERS), FALSE);

	LocalizeLabels();

	for each (const DiscTypeDesc &desc in s_DiscTypes)
	{
		if (desc.Profile == mpInvalid)
			m_cbDiscTypes.InsertString(-1, _TR(IDS_DATADISC, "Data disc"));
		else
			m_cbDiscTypes.InsertString(-1, desc.Label);

		if (desc.Profile == m_SelectedMMCProfile)
			m_cbDiscTypes.SetCurSel(m_cbDiscTypes.GetCount() - 1);
	}

	DisplayParams();
	return 0;
}

LRESULT CLetterSelectionDialog::OnClose( UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled )
{
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CLetterSelectionDialog::OnBnClickedCancel( WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
{
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CLetterSelectionDialog::OnBnClickedOk( WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
{
	TCHAR tsz[3] = {0,};
	if (m_cbLetters.GetLBTextLen(m_cbLetters.GetCurSel()) >= _countof(tsz))
	{
		EndDialog(IDCANCEL);
		return 0;
	}
	m_cbLetters.GetLBText(m_cbLetters.GetCurSel(), tsz);
	m_DriveLetter = (char)(tsz[0]);

	if (SendDlgItemMessage(IDC_AUTOLETTERS, BM_GETCHECK) == BST_CHECKED)
		m_RegParams.DriveLetterPolicy = drvlStartingFromGiven;

	m_RegParams.StartingDriveLetterIndex = m_DriveLetter - 'A';
	m_RegParams.DisableAutorun = (SendDlgItemMessage(IDC_NOAUTORUN, BM_GETCHECK) == BST_CHECKED);
	m_RegParams.PersistentDefault = (SendDlgItemMessage(IDC_PERSISTENT, BM_GETCHECK) == BST_CHECKED);

	size_t idx = m_cbDiscTypes.GetCurSel();
	if (idx < __countof(s_DiscTypes))
		m_SelectedMMCProfile = s_DiscTypes[idx].Profile;

	if (!m_bForciblyInvoked)
		m_RegParams.SaveToRegistry();

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
	m_bEnableDebugLogging = (SendDlgItemMessage(IDC_MAKELOGFILE, BM_GETCHECK) == BST_CHECKED);
#endif

	EndDialog(IDOK);
	return 0;
}

void CLetterSelectionDialog::DisplayParams()
{
	m_cbLetters.Clear();

	DWORD dwLetterMask = GetLogicalDrives();
	unsigned char ch = 'C';
	TCHAR tsz[] = _T("X:");
	bool selUpdated = false;
	for (unsigned i = (1 << (ch - 'A')); ch <= 'Z'; ch++, i <<= 1)
		if (!(dwLetterMask & i))
		{
			tsz[0] = ch, m_cbLetters.InsertString(-1, tsz);
			if (((unsigned char)(ch - 'A') >= m_RegParams.StartingDriveLetterIndex) && !selUpdated)
				m_cbLetters.SetCurSel(m_cbLetters.GetCount() - 1), selUpdated = true;
		}

	if (!m_cbLetters.GetCount())
	{
		MessageBox(_TR(IDS_NOFREELETTERS, "No free drive letters found!"), NULL, MB_ICONERROR);
		EndDialog(IDCANCEL);
	}

	if (!selUpdated)
		m_cbLetters.SetCurSel(0);
	m_cbLetters.SetFocus();

	SendDlgItemMessage(IDC_NOAUTORUN, BM_SETCHECK, m_RegParams.DisableAutorun ? BST_CHECKED : BST_UNCHECKED);
	SendDlgItemMessage(IDC_PERSISTENT, BM_SETCHECK, m_RegParams.PersistentDefault ? BST_CHECKED : BST_UNCHECKED);
	SendDlgItemMessage(IDC_AUTOLETTERS, BM_SETCHECK, (m_RegParams.DriveLetterPolicy != drvlAskEveryTime) ? BST_CHECKED : BST_UNCHECKED);
}

int ShowSettingsDialog();

LRESULT CLetterSelectionDialog::OnBnClickedSettings( WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
{
	ShowSettingsDialog();

	m_RegParams.LoadFromRegistry();
	BazisLib::SelectLanguage(m_RegParams.Language);
	LocalizeLabels();
	DisplayParams();
	return 0;
}

void CLetterSelectionDialog::LocalizeLabels()
{
	LOCALIZE_DIALOG(IDS_MOUNTIMAGE, IDD_SELECTFOLDER, m_hWnd);
	LOCALIZE_DLGITEM(IDS_AUTOLETTERS, IDC_AUTOLETTERS, m_hWnd);
	LOCALIZE_DLGITEM(IDS_NOAUTORUN, IDC_NOAUTORUN, m_hWnd);
	LOCALIZE_DLGITEM(IDS_PERSISTENTDRV, IDC_PERSISTENT, m_hWnd);
	LOCALIZE_DLGITEM(IDS_OK, IDOK, m_hWnd);
	LOCALIZE_DLGITEM(IDS_DISCTYPE, IDC_DISCTYPELBL, m_hWnd);
	LOCALIZE_DLGITEM(IDS_CANCEL, IDCANCEL, m_hWnd);
	LOCALIZE_DLGITEM(IDS_ELLIPSIS, IDC_SETTINGS, m_hWnd);
	LOCALIZE_DLGITEM(IDS_CHOOSELETTER, IDC_CHOOSELETTER, m_hWnd);
	LOCALIZE_DLGITEM(IDS_MAKELOGFILE, IDC_MAKELOGFILE, m_hWnd);
}

LRESULT CLetterSelectionDialog::OnShowWindow(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	return 0;
}