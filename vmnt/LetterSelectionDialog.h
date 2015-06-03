#pragma once

#include "resource.h"
#include "RegistryParams.h"

#define SKIP_WINCDEMU_GUID_DEFINITION
#include "../BazisVirtualCDBus/DeviceControl.h"
#include "../VirtualCDCtl/VirtualCDClient.h"

class CLetterSelectionDialog : public CDialogImpl<CLetterSelectionDialog>
{
private:
	CComboBox m_cbLetters, m_cbDiscTypes;
	char m_DriveLetter;
	
	RegistryParams m_RegParams;
	bool m_bForciblyInvoked;
	unsigned m_SelectedMMCProfile;

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
	bool m_bEnableDebugLogging;
#endif

private:
	void DisplayParams();

public:
	CLetterSelectionDialog(bool ForciblyInvoked, unsigned detectedMMCProfile)
		: m_DriveLetter(0)
		, m_bForciblyInvoked(ForciblyInvoked)
		, m_SelectedMMCProfile(detectedMMCProfile)
	{
		if (m_SelectedMMCProfile == mpInvalid)
			m_SelectedMMCProfile = m_RegParams.DefaultMMCProfile;
	}

public:
	enum { IDD = IDD_SELECTFOLDER };

	BEGIN_MSG_MAP(CLetterSelectionDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SHOWWINDOW, OnShowWindow)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		COMMAND_ID_HANDLER(IDOK, OnBnClickedOk)
		COMMAND_ID_HANDLER(IDCANCEL, OnBnClickedCancel)
		COMMAND_ID_HANDLER(IDC_SETTINGS, OnBnClickedSettings)
	END_MSG_MAP()

public:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnShowWindow(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnBnClickedCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedOk(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedSettings(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

public:
	char GetDriveLetter()
	{
		return m_DriveLetter;
	}

	bool IsAutorunDisabled()
	{
		return m_RegParams.DisableAutorun;
	}

	bool IsPersistent()
	{
		return m_RegParams.PersistentDefault;
	}

	enum MMCProfile GetMMCProfile()
	{
		return (enum MMCProfile)m_SelectedMMCProfile;
	}

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
	bool IsDebugLoggingEnabled() {return m_bEnableDebugLogging;}
#endif

private:
	void LocalizeLabels();
};
