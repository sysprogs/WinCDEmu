#pragma once

#include "resource.h"

#include "RegistryParams.h"
#include "../VirtualCDCtl/DriveReadingThread.h"
#include <atlctrls.h>
#include <vector>
#include <bzscore/status.h>
#include <bzscore/string.h>
#include <bzshlp/Win32/process.h>
#include "TaskBarInterface.h"

//A WTL bug prevents correct display of group boxes when using CDialogResize.
class CISOBuildProgressDialog : public CDialogImpl<CISOBuildProgressDialog> /*, public CDialogResize<CISOBuildProgressDialog>*/
{
private:
	CProgressBarCtrl m_ProgressBar;
	CComPtr<ITaskbarList3> m_pTaskbar;
	BazisLib::String m_CommandLine, m_IsoFile, m_Folder;
	bool m_DetailsShown;
	unsigned m_ExpandedHeight;
	BazisLib::Win32::Process *m_pProcess;
	HANDLE m_hOutputPipe;
	BazisLib::ActionStatus m_ExecStatus;
	BazisLib::DynamicStringA m_AllOutput;
	int m_LastProgressMessageOffset;
	CComboBox m_cbLetters;
	bool m_Succeeded;

	RegistryParams m_RegParams;
	ISOCreatorRegistryParams m_ISORegParams;


public:
	CISOBuildProgressDialog(const BazisLib::String &mkisofsCommandLine, const BazisLib::String &isoFile, const BazisLib::String &folder);

	~CISOBuildProgressDialog()
	{
		delete m_pProcess;
		CloseHandle(m_hOutputPipe);
	}

public:
	enum { IDD = IDD_ISOBUILDPROGRESS };

	BEGIN_MSG_MAP(CISOBuildProgressDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		COMMAND_ID_HANDLER(IDCANCEL, OnBnClickedCancel)
		COMMAND_ID_HANDLER(IDC_DETAILS, OnBnClickedDetails)
		//CHAIN_MSG_MAP(CDialogResize)
	END_MSG_MAP()

	/*
	BEGIN_DLGRESIZE_MAP(CISOProgressDialog)
		DLGRESIZE_CONTROL(IDC_ISOFILE, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_FOLDER, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_PROGRESS1, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_DETAILS, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_PROGRESSTEXT, DLSZ_SIZE_X | DLSZ_SIZE_Y)

		DLGRESIZE_CONTROL(IDC_WHENDONE, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_MOUNTISO, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_DRIVELETTER, DLSZ_MOVE_X)
	END_DLGRESIZE_MAP()*/

public:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnBnClickedCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedDetails(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	void SaveParams();
	void LocalizeLabels();
};
