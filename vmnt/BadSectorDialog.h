#pragma once
#include "resource.h"
#include "../VirtualCDCtl/BadSectorDatabase.h"
#include "BadSectorReportDialog.h"

class CBadSectorDialog : public CDialogImpl<CBadSectorDialog>
{
private:
	BadSectorDatabase *m_pBadSectorDB;

public:
	CBadSectorDialog(BadSectorDatabase *pDB)
		: m_pBadSectorDB(pDB)
	{
	}

	enum { IDD = IDD_BADSECTORS };

	BEGIN_MSG_MAP(CLetterSelectionDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		COMMAND_HANDLER(IDC_REREADONCE, BN_CLICKED, OnBnClickedCmd)
		COMMAND_HANDLER(IDC_REREADLOOP, BN_CLICKED, OnBnClickedCmd)
		COMMAND_HANDLER(IDC_REPORTBADSECTORS, BN_CLICKED, OnBnClickedReportbadsectors)
		COMMAND_HANDLER(IDC_IGNOREBAD, BN_CLICKED, OnBnClickedCmd)
	END_MSG_MAP()

public:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		SetDlgItemText(IDC_BADSECTORSREPORT, BazisLib::String::sFormat(_TR(IDS_BADSECTORACTIONFMT, "Found %d bad sectors while reading the disc. Please select an action:"), m_pBadSectorDB->GetTotalCount()).c_str());
		LocalizeLabels();
		return 0;
	}

	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		EndDialog(IDCANCEL);
		return 0;
	}

private:
	void LocalizeLabels()
	{
		LOCALIZE_DIALOG(IDS_BADSECTORSDLGTITLE, IDD_BADSECTORS, m_hWnd);
		LOCALIZE_DLGITEM(IDS_REREADONCE, IDC_REREADONCE, m_hWnd);
		LOCALIZE_DLGITEM(IDS_REREADLOOP, IDC_REREADLOOP, m_hWnd);
		LOCALIZE_DLGITEM(IDS_REPORTBADSECTORS, IDC_REPORTBADSECTORS, m_hWnd);
		LOCALIZE_DLGITEM(IDS_IGNOREBADSECTORS, IDC_IGNOREBAD, m_hWnd);
		LOCALIZE_DLGITEM(NULL, IDC_BADSECTORSREPORT, m_hWnd);
	}
public:
	LRESULT OnBnClickedCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}


	LRESULT OnBnClickedReportbadsectors(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CBadSectorReportDialog dlg(m_pBadSectorDB);
		dlg.DoModal();
		return 0;
	}
};
