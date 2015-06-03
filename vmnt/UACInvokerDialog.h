#pragma once

#include "resource.h"
#include <bzscore/string.h>

int RunSelfElevated(LPCTSTR lpCmdLine = GetCommandLine());

class CUACInvokerDialog : public CDialogImpl<CUACInvokerDialog>
{
private:
	BazisLib::String m_CmdLine;

public:
	CUACInvokerDialog(BazisLib::String cmdLine)
		: m_CmdLine(cmdLine)
	{
	}

public:
	enum { IDD = IDD_EMPTY };

	BEGIN_MSG_MAP(CUACInvokerDialog)
		MESSAGE_HANDLER(WM_SHOWWINDOW, OnShowWindow)
	END_MSG_MAP()

public:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnShowWindow(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
	{
		BringWindowToTop();
		SetForegroundWindow(m_hWnd);
		EndDialog(RunSelfElevated(m_CmdLine.c_str()));
		return 0;
	}
};
