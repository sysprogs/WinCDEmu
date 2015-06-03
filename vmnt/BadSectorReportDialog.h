#pragma once
#include "resource.h"
#include "../VirtualCDCtl/BadSectorDatabase.h"

class CBadSectorReportDialog : public CDialogImpl<CBadSectorReportDialog>
{
private:
	BadSectorDatabase *m_pBadSectorDB;

public:
	CBadSectorReportDialog(BadSectorDatabase *pDB)
		: m_pBadSectorDB(pDB)
	{
	}

	enum { IDD = IDD_BADSECTORREPORT };

	BEGIN_MSG_MAP(CLetterSelectionDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnBnClickedOK)
	END_MSG_MAP()

public:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		LocalizeLabels();

		BazisLib::String report;
		report.Format(_TR(IDS_BADSECTORREPORTHDR, "WinCDEmu found %d bad sectors:\r\n"), m_pBadSectorDB->GetTotalCount());
		for each(BadSectorDatabase::Record rec in *m_pBadSectorDB)
			report += BazisLib::String::sFormat(_TR(IDS_BADSECTORREPORTLINE, "0x%08X - 0x%08X (%d sectors)\r\n"), (int)rec.SectorNumber, (int)(rec.SectorNumber + rec.SectorCount), (int)rec.SectorCount);

		SetDlgItemText(IDC_EDIT1, report.c_str());
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
		LOCALIZE_DIALOG(IDS_BADSECTORREPORTDLGTITLE, IDD_BADSECTORREPORT, m_hWnd);
		LOCALIZE_DLGITEM(IDS_CLOSE, IDOK, m_hWnd);
	}
public:
	LRESULT OnBnClickedOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(IDOK);
		return 0;
	}

};
