#include "stdafx.h"
#include "ISOProgressDialog.h"
#include <bzshlp/Win32/i18n.h>
#include <bzshlp/Win32/process.h>
#include <bzshlp/ratecalc.h>
#include "../VirtualCDCtl/VirtualCDClient.h"
#include "../VirtualCDCtl/CDSpeed.h"
#include "BadSectorDialog.h"

using namespace BazisLib;

enum {kProgressRangeMax = 10000};

LRESULT CISOProgressDialog::OnInitDialog( UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled )
{
	m_cbLetters.m_hWnd = GetDlgItem(IDC_DRIVELETTER);
	m_ProgressBar.m_hWnd = GetDlgItem(IDC_PROGRESS1);
	m_cbReadSpeeds.m_hWnd = GetDlgItem(IDC_CDREADSPEED);

	LocalizeLabels();
	DisplayParams();

	const wchar_t *pwszSourceUser =  m_pReader->GetSourcePath();
	if (!memcmp(pwszSourceUser, L"\\\\.\\", 8))
		pwszSourceUser += 4;

	SetDlgItemText(IDC_SOURCEDRIVE, String::sFormat(_TR(IDS_SRCDRIVEFMT, "Source drive: %s (%sB)"), pwszSourceUser, RateCalculator::FormatByteCount(m_pReader->GetVolumeSize()).c_str()).c_str());
	SetDlgItemText(IDC_IMAGEFILE, String::sFormat(_TR(IDS_IMAGEFILEFMT, "Image file: %s"), m_pReader->GetImagePath()).c_str());

	m_ProgressBar.SetRange(0, kProgressRangeMax);

	bool bDVD = false;
	m_CDReadSpeeds = QueryAvailableReadSpeeds(m_pReader->GetSourcePath(), &bDVD);

	if (m_CDReadSpeeds.empty())
	{
		::EnableWindow(GetDlgItem(IDC_CDREADSPEED), FALSE);
		::EnableWindow(GetDlgItem(IDC_READSPEEDLBL), FALSE);
	}
	else
		for(size_t i = 0; i < m_CDReadSpeeds.size(); i++)
		{
			int dividerX10 = bDVD ? 13850 : 1765;
			int multiplier = (m_CDReadSpeeds[i] * 10 + (dividerX10/ 2)) / dividerX10;
			m_cbReadSpeeds.InsertString(-1, String::sFormat(_T("%dx (%d KB/s)"), multiplier, m_CDReadSpeeds[i]).c_str());
		}

	SetDlgItemText(IDC_READSPEEDLBL, String::sFormat(_TR(IDS_READSPEEDFMT, "%s read speed"), bDVD ? _T("DVD") : _T("CD")).c_str());

	SetTimer(0, 100);
	OnTimer(0, 0, 0, bHandled);

	SetDlgItemText(IDC_PAUSE, _TR(IDS_PAUSE, "Pause"));

	if (m_pTaskbar)
		m_pTaskbar->SetProgressState(m_hWnd, TBPF_NORMAL);

	return 0;
}

LRESULT CISOProgressDialog::OnClose( UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled )
{
	EndDialog(IDCANCEL);
	SaveParams();
	return 0;
}

LRESULT CISOProgressDialog::OnBnClickedCancel( WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/ )
{
	if (m_bBadSectorDialogInvoked && m_pReader->GetProgress().TotalBad)
	{
		m_pReader->CancelBadSectorReReading();
		return 0;
	}

	EndDialog(IDCANCEL);
	SaveParams();

	return 0;
}

void CISOProgressDialog::DisplayParams()
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

	SendDlgItemMessage(IDC_CLOSEWINDOW, BM_SETCHECK, m_ISORegParams.CloseWindowAutomatically ? BST_CHECKED : BST_UNCHECKED);
	SendDlgItemMessage(IDC_EJECTDISC, BM_SETCHECK, m_ISORegParams.EjectDiscAutomatically ? BST_CHECKED : BST_UNCHECKED);
	SendDlgItemMessage(IDC_MOUNTISO, BM_SETCHECK, m_ISORegParams.MountISOAutomatically ? BST_CHECKED : BST_UNCHECKED);
	SendDlgItemMessage(IDC_OPENFOLDER, BM_SETCHECK, m_ISORegParams.OpenFolderAutomatically ? BST_CHECKED : BST_UNCHECKED);

	if (!m_cbLetters.GetCount())
	{
		::EnableWindow(GetDlgItem(IDC_MOUNTISO), FALSE);
		SendDlgItemMessage(IDC_MOUNTISO, BM_SETCHECK, BST_UNCHECKED);
	}
	else
	{
		if (!selUpdated)
			m_cbLetters.SetCurSel(0);
	}
}


void CISOProgressDialog::LocalizeLabels()
{
	LOCALIZE_DIALOG(IDS_CREATINGIMAGE, IDD_ISOPROGRESS, m_hWnd);
	LOCALIZE_DLGITEM(IDS_WHENDONE, IDC_WHENDONE, m_hWnd);
	LOCALIZE_DLGITEM(IDS_AUTOCLOSEWND, IDC_CLOSEWINDOW, m_hWnd);
	LOCALIZE_DLGITEM(IDS_EJECTDISC, IDC_EJECTDISC, m_hWnd);
	LOCALIZE_DLGITEM(IDS_MOUNTISOTO, IDC_MOUNTISO, m_hWnd);
	LOCALIZE_DLGITEM(IDS_OPENFOLDER, IDC_OPENFOLDER, m_hWnd);
	LOCALIZE_DLGITEM(IDS_CANCEL, IDCANCEL, m_hWnd);
	LOCALIZE_DLGITEM(NULL, IDC_SOURCEDRIVE, m_hWnd);
	LOCALIZE_DLGITEM(NULL, IDC_IMAGEFILE, m_hWnd);
	LOCALIZE_DLGITEM(NULL, IDC_PAUSE, m_hWnd);
	LOCALIZE_DLGITEM(NULL, IDC_PROGRESS1, m_hWnd);
	LOCALIZE_DLGITEM(NULL, IDC_PROGRESSLINE, m_hWnd);
	LOCALIZE_DLGITEM(NULL, IDC_READSPEEDLBL, m_hWnd);
}

LRESULT CISOProgressDialog::OnShowWindow(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	return 0;
}

static int EjectDriveProc(LPVOID lp)
{
	File((wchar_t *)lp, FileModes::OpenReadOnly.ShareAll()).DeviceIoControl(IOCTL_STORAGE_EJECT_MEDIA);
	return 0;
}

LRESULT CISOProgressDialog::OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	extern bool AutorunErrorHandler(ActionStatus st);

	BackgroundDriveReader::ProgressRecord progress = m_pReader->GetProgress();
	unsigned progressVal = (unsigned)((progress.Done * kProgressRangeMax) / progress.Total);

	if (progress.TotalBad)
	{
		if (m_pTaskbar)
			m_pTaskbar->SetProgressState(m_hWnd, TBPF_INDETERMINATE);
		m_ProgressBar.SetPos((int)((progress.ProcessedBad * kProgressRangeMax) / progress.TotalBad));
		SetDlgItemText(IDC_PROGRESSLINE, String::sFormat(_TR(IDS_BADSECTORPROGRESSFMT, "Re-reading bad sectors (%d/%d)"), (int)progress.ProcessedBad, (int)progress.TotalBad).c_str());
	}
	else
	{
		if (m_pTaskbar)
			m_pTaskbar->SetProgressValue(m_hWnd, progress.Done, progress.Total);
		m_ProgressBar.SetPos(progressVal);
		unsigned badSectorCount = m_pReader->GetTotalBadSectorCount();
		if (!badSectorCount)
		{
			bool remainingTimeAccurate = false;
			TimeSpan remainingTime = m_pReader->GetEstimatedRemainingTime(&remainingTimeAccurate);
			String label = String::sFormat(_TR(IDS_SPEEDSTATFMT, "Average read speed: %s/s"), RateCalculator::FormatByteCount(progress.BytesPerSecond).c_str());
			if (remainingTimeAccurate && (remainingTime.GetTotalSeconds() > 3))
				label += String::sFormat(_TR(IDS_REMAININGTIMESUFFIX, ", remaining time: %02d:%02d"), remainingTime.GetTotalMinutes(), remainingTime.GetSeconds());
			SetDlgItemText(IDC_PROGRESSLINE, label.c_str());
		}
		else
			SetDlgItemText(IDC_PROGRESSLINE, String::sFormat(_TR(IDS_BADSECTFMT, "Average read speed: %s/s, %d bad sector(s) found"), RateCalculator::FormatByteCount(progress.BytesPerSecond).c_str(), badSectorCount).c_str());
	}

	if (m_pReader->QueryStatus().GetErrorCode() != Pending)
	{
		KillTimer(0);
		::EnableWindow(GetDlgItem(IDC_PAUSE), FALSE);
		ActionStatus st = m_pReader->QueryStatus();

		if (st.Successful())
		{
			FunctionThread ejectThread(&EjectDriveProc, (PVOID)m_pReader->GetSourcePath());
			if (SendDlgItemMessage(IDC_EJECTDISC, BM_GETCHECK) == BST_CHECKED)
				ejectThread.Start();

			if (SendDlgItemMessage(IDC_OPENFOLDER, BM_GETCHECK) == BST_CHECKED)
				Win32::Process(Path::GetDirectoryName(ConstString(m_pReader->GetImagePath())).c_str(), _T("open"));

			if (SendDlgItemMessage(IDC_MOUNTISO, BM_GETCHECK) == BST_CHECKED)
			{
				wchar_t wszKernelPath[512];
				if (!VirtualCDClient::Win32FileNameToKernelFileName(m_pReader->GetImagePath(), wszKernelPath, __countof(wszKernelPath)))
					MessageBox(_TR(IDS_BADIMGFN, "Invalid image file name!"), NULL, MB_ICONERROR);
				else
				{
					TCHAR tsz[3] = {0,};
					if (m_cbLetters.GetLBTextLen(m_cbLetters.GetCurSel()) < _countof(tsz))
						m_cbLetters.GetLBText(m_cbLetters.GetCurSel(), tsz);

					VirtualCDClient().ConnectDisk(wszKernelPath, (char)(tsz[0]), 0, m_RegParams.DisableAutorun, false, AutorunErrorHandler);
				}
			}

			ejectThread.Join();

			if (SendDlgItemMessage(IDC_CLOSEWINDOW, BM_GETCHECK) == BST_CHECKED)
			{
				EndDialog(IDOK);
				SaveParams();
			}
			else
			{
				SetDlgItemText(IDCANCEL, _TR(IDS_CLOSE, "Close"));
				m_ProgressBar.SetPos(kProgressRangeMax);
				::SetFocus(GetDlgItem(IDCANCEL));
				MessageBox(_TR(IDS_ISOCREATED, "The ISO image has been created successfully"), _TR(IDS_INFORMATION, "Information"), MB_ICONINFORMATION);
			}
		}
		else
		{
			if (st.GetErrorCode() != OperationAborted)
				MessageBox(st.GetMostInformativeText().c_str(), NULL, MB_ICONERROR);
			DeleteFile(m_pReader->GetImagePath());
			EndDialog(IDNO);
		}
	}
	return 0;
}

void CISOProgressDialog::SaveParams()
{
	m_ISORegParams.CloseWindowAutomatically = (SendDlgItemMessage(IDC_CLOSEWINDOW, BM_GETCHECK) == BST_CHECKED);
	m_ISORegParams.MountISOAutomatically = (SendDlgItemMessage(IDC_MOUNTISO, BM_GETCHECK) == BST_CHECKED);
	m_ISORegParams.EjectDiscAutomatically = (SendDlgItemMessage(IDC_EJECTDISC, BM_GETCHECK) == BST_CHECKED);
	m_ISORegParams.OpenFolderAutomatically = (SendDlgItemMessage(IDC_OPENFOLDER, BM_GETCHECK) == BST_CHECKED);
	m_ISORegParams.SaveToRegistry();
}

LRESULT CISOProgressDialog::OnCbnSelchangeCdreadspeed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	size_t idx = m_cbReadSpeeds.GetCurSel();
	if (idx >= m_CDReadSpeeds.size())
		return 0;

	int speed = m_CDReadSpeeds[idx];
	ActionStatus st = SetCDReadSpeed(m_pReader->GetSourcePath(), speed);
	if (!st.Successful())
		MessageBox(st.GetMostInformativeText().c_str(), NULL, MB_ICONERROR);

	m_pReader->ResetRemainingTimeCalculation();
	return 0;
}

LRESULT CISOProgressDialog::OnBnClickedPause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_pReader->IsPaused())
	{
		SetDlgItemText(IDC_PAUSE, _TR(IDS_PAUSE, "Pause"));
		m_pReader->Resume();
		m_pReader->ResetRemainingTimeCalculation();
		if (m_pTaskbar)
			m_pTaskbar->SetProgressState(m_hWnd, TBPF_NORMAL);
	}
	else
	{
		SetDlgItemText(IDC_PAUSE, _TR(IDS_RESUME, "Resume"));
		m_pReader->Pause();
		m_pReader->ResetRemainingTimeCalculation();
		if (m_pTaskbar)
			m_pTaskbar->SetProgressState(m_hWnd, TBPF_PAUSED);
	}

	return 0;
}

bool CISOProgressDialog::OnWriteError(BazisLib::ActionStatus status)
{
	return SendMessage(WMX_RETRYCANCEL, 0, (LPARAM)String::sFormat(_TR(IDS_WRITEERRORFMT, "Cannot write to ISO file: %s"), status.GetMostInformativeText().c_str()).c_str()) == 1;
}

LRESULT CISOProgressDialog::OnRetryCancel(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	LPTSTR lpMsg = (LPTSTR)lParam;
	if (IsBadStringPtr(lpMsg, 2048))
		return 0;

	return MessageBox(lpMsg, NULL, MB_RETRYCANCEL | MB_ICONERROR) == IDRETRY;
}

BackgroundDriveReader::IErrorHandler::BadSectorsAction CISOProgressDialog::OnBadSectorsFound(BadSectorDatabase *pDB)
{
	return (BackgroundDriveReader::IErrorHandler::BadSectorsAction)SendMessage(WMX_BADSECTORS, 0, (LPARAM)pDB);
}

LRESULT CISOProgressDialog::OnBadSectors(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	m_bBadSectorDialogInvoked = true;

	CBadSectorDialog dlg((BadSectorDatabase *)lParam);
	switch (dlg.DoModal())
	{
	case IDC_REREADONCE:
		return BackgroundDriveReader::IErrorHandler::RereadOnce;
	case IDC_REREADLOOP:
		return BackgroundDriveReader::IErrorHandler::RereadContinuous;
	default:
		return BackgroundDriveReader::IErrorHandler::Ignore;
	}
}