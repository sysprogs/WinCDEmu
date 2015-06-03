#pragma once

#include "resource.h"
#include "RegistryParams.h"
#include <bzscore/Win32/registry.h>

class CSettingsDialog : public CDialogImpl<CSettingsDialog>
{
private:
	RegistryParams m_RegParams;
	CComboBox m_cbLetters;
	CComboBox m_cbLanguages;
	BazisLib::RegistryKey m_DriverParametersKey;

public:
	CSettingsDialog()
		: m_DriverParametersKey(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\BazisVirtualCDBus\\Parameters"))
	{
	}

public:
	enum { IDD = IDD_SETTINGS };

	BEGIN_MSG_MAP(CPropertiesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		COMMAND_ID_HANDLER(IDOK, OnBnClickedOk)
		COMMAND_ID_HANDLER(IDCANCEL, OnBnClickedCancel)
		COMMAND_HANDLER(IDC_LANGSELECT, CBN_SELCHANGE, OnCbnSelchangeLangselect)
	END_MSG_MAP()

public:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnBnClickedCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedOk(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	void LocalizeLabels();
public:
	LRESULT OnCbnSelchangeLangselect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};
