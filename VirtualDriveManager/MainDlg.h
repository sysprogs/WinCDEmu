// MainDlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "../VirtualCDCtl/VirtualCDClient.h"

class CMainDlg : public CDialogImpl<CMainDlg>, public CDialogResize<CMainDlg>
{
private:
	CListViewCtrl m_ListView;
	CImageList m_ImageList;
	VirtualCDClient *m_pClient;
	VirtualCDClient *m_pNonPortableClient;
	VirtualCDClient::VirtualCDList m_LastCDList;

public:
	CMainDlg()
		: m_pClient(NULL)
		, m_pNonPortableClient(NULL)
	{
	}

	~CMainDlg()
	{
		delete m_pClient;
		delete m_pNonPortableClient;
	}

public:
	enum { IDD = IDD_MAINDLG };

	BEGIN_MSG_MAP(CMainDlg)
		COMMAND_HANDLER(IDC_UNINSTALL, BN_CLICKED, OnBnClickedUninstall)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles)
		COMMAND_HANDLER(IDC_MOUNTNEW, BN_CLICKED, OnBnClickedMountnew)
		COMMAND_HANDLER(IDC_REPLACEIMG, BN_CLICKED, OnBnClickedReplaceImage)
		COMMAND_HANDLER(IDC_UNMOUNT, BN_CLICKED, OnBnClickedUnmount)
		COMMAND_HANDLER(IDC_UNMOUNTALL, BN_CLICKED, OnBnClickedUnmountall)
		CHAIN_MSG_MAP(CDialogResize)
		NOTIFY_HANDLER(IDC_LIST1, LVN_ITEMCHANGED, OnSelChanged)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

	BEGIN_DLGRESIZE_MAP(CMainDlg)
		DLGRESIZE_CONTROL(IDC_LIST1, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDOK,  DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_MOUNTNEW,  DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_UNMOUNT, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_UNMOUNTALL, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_REPLACEIMG, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_UNINSTALL,  DLSZ_MOVE_Y)
	END_DLGRESIZE_MAP()

	bool DoUnmountAll(bool suppressErrorMessages);
	BazisLib::ActionStatus DoUnmount(LPCTSTR lpRoot);


// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedUninstall(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT OnBnClickedMountnew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedUnmount(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedUnmountall(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedReplaceImage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSelChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
};
