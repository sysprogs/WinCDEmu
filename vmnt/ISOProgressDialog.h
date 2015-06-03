#pragma once

#include "resource.h"

#include "RegistryParams.h"
#include "../VirtualCDCtl/DriveReadingThread.h"
#include <atlctrls.h>
#include <vector>
#include "TaskBarInterface.h"

class CISOProgressDialog : public CDialogImpl<CISOProgressDialog>, public BackgroundDriveReader::IErrorHandler
{
private:
	CComboBox m_cbLetters, m_cbReadSpeeds;
	CProgressBarCtrl m_ProgressBar;

	RegistryParams m_RegParams;
	ISOCreatorRegistryParams m_ISORegParams;

	BackgroundDriveReader *m_pReader;
	std::vector<int> m_CDReadSpeeds;
	bool m_bBadSectorDialogInvoked;
	CComPtr<ITaskbarList3> m_pTaskbar;

private:
	enum {WMX_RETRYCANCEL = WM_USER + 13, WMX_BADSECTORS};

public:
	CISOProgressDialog(BackgroundDriveReader *pReader)
		: m_pReader(pReader)
		, m_bBadSectorDialogInvoked(false)
	{
		m_pTaskbar.CoCreateInstance(CLSID_TaskbarList);
	}

public:
	enum { IDD = IDD_ISOPROGRESS };

	BEGIN_MSG_MAP(CISOProgressDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SHOWWINDOW, OnShowWindow)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WMX_RETRYCANCEL, OnRetryCancel)
		MESSAGE_HANDLER(WMX_BADSECTORS, OnBadSectors)
		COMMAND_ID_HANDLER(IDCANCEL, OnBnClickedCancel)
		COMMAND_HANDLER(IDC_CDREADSPEED, CBN_SELCHANGE, OnCbnSelchangeCdreadspeed)
		COMMAND_HANDLER(IDC_PAUSE, BN_CLICKED, OnBnClickedPause)
	END_MSG_MAP()

public:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnShowWindow(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnRetryCancel(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnBadSectors(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnBnClickedCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	void LocalizeLabels();
	void DisplayParams();
	void SaveParams();
public:
	LRESULT OnCbnSelchangeCdreadspeed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedPause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

public:
	virtual bool OnWriteError(BazisLib::ActionStatus status);
	virtual BackgroundDriveReader::IErrorHandler::BadSectorsAction OnBadSectorsFound(BadSectorDatabase *pDB);
};
