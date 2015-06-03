#include "StdAfx.h"
#include "SettingsDialog.h"
#include <bzshlp/Win32/i18n.h>
#include "../VirtualCDCtl/VirtualCDClient.h"

LRESULT CSettingsDialog::OnInitDialog( UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled )
{
	m_cbLetters.m_hWnd = GetDlgItem(IDC_DRIVELETTER);
	m_cbLanguages.m_hWnd = GetDlgItem(IDC_LANGSELECT);

	LocalizeLabels();

	switch (m_RegParams.DriveLetterPolicy)
	{
	case drvlAutomatic:
		SendDlgItemMessage(IDC_RADIO2, BM_SETCHECK, BST_CHECKED);
		break;
	case drvlStartingFromGiven:
		SendDlgItemMessage(IDC_RADIO3, BM_SETCHECK, BST_CHECKED);
		break;
	case drvlAskEveryTime:
		SendDlgItemMessage(IDC_RADIO4, BM_SETCHECK, BST_CHECKED);
		break;
	}

	TCHAR tsz[] = _T("X:");
	for (char ch = 'C'; ch <= 'Z'; ch++)
	{
		tsz[0] = ch, m_cbLetters.InsertString(-1, tsz);
		if ((ch - 'A') == m_RegParams.StartingDriveLetterIndex)
			m_cbLetters.SetCurSel(m_cbLetters.GetCount() - 1);
	}

	if (m_cbLetters.GetCurSel() == -1)
		m_cbLetters.SetCurSel(0);

	if (m_RegParams.DisableAutorun)
		SendDlgItemMessage(IDC_CHECK1, BM_SETCHECK, BST_CHECKED);

	if (!m_DriverParametersKey[_T("GrantAccessToEveryone")])
		SendDlgItemMessage(IDC_REQUIREADMIN, BM_SETCHECK, BST_CHECKED);

	return 0;
}

LRESULT CSettingsDialog::OnClose( UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled )
{
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CSettingsDialog::OnBnClickedCancel( WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
{
	EndDialog(IDCANCEL);
	return 0;
}

LRESULT CSettingsDialog::OnBnClickedOk( WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
{
	TCHAR tsz[3] = {0,};
	if (m_cbLetters.GetLBTextLen(m_cbLetters.GetCurSel()) >= _countof(tsz))
	{
		EndDialog(IDCANCEL);
		return 0;
	}
	m_cbLetters.GetLBText(m_cbLetters.GetCurSel(), tsz);

	if (SendDlgItemMessage(IDC_RADIO2, BM_GETCHECK) == BST_CHECKED)
		m_RegParams.DriveLetterPolicy = drvlAutomatic;
	else if (SendDlgItemMessage(IDC_RADIO3, BM_GETCHECK) == BST_CHECKED)
		m_RegParams.DriveLetterPolicy = drvlStartingFromGiven;
	else if (SendDlgItemMessage(IDC_RADIO4, BM_GETCHECK) == BST_CHECKED)
		m_RegParams.DriveLetterPolicy = drvlAskEveryTime;

	m_RegParams.DisableAutorun = (SendDlgItemMessage(IDC_CHECK1, BM_GETCHECK) == BST_CHECKED);

	m_RegParams.StartingDriveLetterIndex = (char)(tsz[0]) - 'A';
	if (m_RegParams.StartingDriveLetterIndex > ('Z' - 'A'))
		m_RegParams.StartingDriveLetterIndex = 'Z' - 'A';

	unsigned idx = m_cbLanguages.GetCurSel();
	if (idx != -1)
	{
		LANGID langID = (LANGID)m_cbLanguages.GetItemData(idx);
		if (langID)
			m_RegParams.Language = langID;
	}

	int prevVal = m_DriverParametersKey[_T("GrantAccessToEveryone")];
	int newVal = (SendDlgItemMessage(IDC_REQUIREADMIN, BM_GETCHECK) == BST_CHECKED) ? 0 : 1;
	m_DriverParametersKey[_T("GrantAccessToEveryone")] = newVal;

	if (prevVal != newVal)
		VirtualCDClient::RestartDriver();

	m_RegParams.SaveToRegistry();
	EndDialog(IDOK);
	return 0;
}

void CSettingsDialog::LocalizeLabels()
{
	LOCALIZE_DIALOG(IDS_SETTINGSHDR, IDD_SETTINGS, m_hWnd);
	LOCALIZE_DLGITEM(IDS_LTRPOLICY, IDC_LTRPOLICY, m_hWnd);
	LOCALIZE_DLGITEM(IDS_LTR_AUTO, IDC_RADIO2, m_hWnd);
	LOCALIZE_DLGITEM(IDS_LTR_STARTINGFROM, IDC_RADIO3, m_hWnd);
	LOCALIZE_DLGITEM(IDS_LTR_ALWAYSASK, IDC_RADIO4, m_hWnd);
	LOCALIZE_DLGITEM(IDS_OK, IDOK, m_hWnd);
	LOCALIZE_DLGITEM(IDS_CANCEL, IDCANCEL, m_hWnd);
	LOCALIZE_DLGITEM(IDS_NOAUTORUNDEFAULT, IDC_CHECK1, m_hWnd);
	LOCALIZE_DLGITEM(IDS_LANGUAGE, IDC_LANGUAGE, m_hWnd);
	LOCALIZE_DLGITEM(IDS_REQUIREADMIN, IDC_REQUIREADMIN, m_hWnd);

	LANGID langCurrent = BazisLib::GetCurrentLanguageId();
	if (!langCurrent)
		langCurrent = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);	//GetCurrentLanguageId() returns 0 if no language was selected and default english strings were used

	m_cbLanguages.ResetContent();
	for each(const BazisLib::InstalledLanguage &lang in BazisLib::GetInstalledLanguageList())
	{
		m_cbLanguages.InsertString(-1, (lang.LangID == langCurrent) ? lang.LangName.c_str() : lang.LangNameEng.c_str());
		m_cbLanguages.SetItemData(m_cbLanguages.GetCount() - 1, lang.LangID);
		if (lang.LangID == langCurrent)
			m_cbLanguages.SetCurSel(m_cbLanguages.GetCount() - 1);
	}


}
LRESULT CSettingsDialog::OnCbnSelchangeLangselect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	unsigned idx = m_cbLanguages.GetCurSel();
	if (idx == -1)
		return 0;
	LANGID langID = (LANGID)m_cbLanguages.GetItemData(idx);
	if (!langID)
		return 0;

	if (langID != BazisLib::GetCurrentLanguageId())
	{
		BazisLib::SelectLanguage(langID);
		LocalizeLabels();
	}

	return 0;
}
