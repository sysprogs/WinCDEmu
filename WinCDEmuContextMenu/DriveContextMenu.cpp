// DriveContextMenu.cpp : Implementation of CDriveContextMenu

#include "stdafx.h"
#include "DriveContextMenu.h"
#include "../vmnt/ContextMenuLabels.h"
#include "../VirtualCDCtl/VirtualDriveClient.h"

#define IDM_CREATEISO		0
#define IDM_CHANGEIMAGE		1
#define IDM_BUILDISO		2

// CDriveContextMenu

#include <bzscore/file.h>
#include "DebugLog.h"
using namespace BazisLib;

HRESULT STDMETHODCALLTYPE CDriveContextMenu::Initialize(/* [unique][in] */ __in_opt PCIDLIST_ABSOLUTE pidlFolder, /* [unique][in] */ __in_opt IDataObject *pdtobj, /* [unique][in] */ __in_opt HKEY hkeyProgID)
{
	WINCDEMU_LOG_LINE(L"CDriveContextMenu::Initialize()");

	HRESULT hR = ContextMenuBase::Initialize(pidlFolder, pdtobj, hkeyProgID);
	if (!SUCCEEDED(hR))
		return hR;

	if (m_FileName.length() == 3 && m_FileName[1] == ':' && GetDriveType(m_FileName.c_str()) == DRIVE_CDROM)
	{
		m_bIsOpticalDrive = true;

		if (m_FileName.length() == 3 && m_FileName[1] == ':' && m_FileName[2] == '\\')
		{
			wchar_t wszPath [] = L"\\\\.\\?:";
			wszPath[4] = m_FileName[0];

			ActionStatus status;
			VirtualDriveClient clt(wszPath, &status);
			m_bIsWinCDEmuDrive = status.Successful();
		}
	}

	WINCDEMU_LOG_LINE(L"CDriveContextMenu::Initialize(): isDrive = %d, isWinCDEmu = %d", m_bIsOpticalDrive, m_bIsWinCDEmuDrive);
	return S_OK;
}

extern HINSTANCE g_hModule;
#include "TransparentMenuBitmap.h"


HRESULT STDMETHODCALLTYPE CDriveContextMenu::QueryContextMenu(/* [in] */ __in HMENU hmenu, /* [in] */ __in UINT indexMenu, /* [in] */ __in UINT idCmdFirst, /* [in] */ __in UINT idCmdLast, /* [in] */ __in UINT uFlags)
{
	WINCDEMU_LOG_LINE(L"CDriveContextMenu::QueryContextMenu()");

	if (uFlags & CMF_DEFAULTONLY)
		return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));

	unsigned lastID = IDM_CREATEISO;
	WINCDEMU_LOG_LINE(L"CDriveContextMenu: inserting menu items...");

	if (m_bIsOpticalDrive)
	{
		InsertMenu(hmenu, indexMenu, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_CREATEISO, txtCreateISO());

		if (m_bIsWinCDEmuDrive)
		{
			InsertMenu(hmenu, indexMenu + 1, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_CHANGEIMAGE, txtSelectAnotherImage());
			lastID = IDM_CHANGEIMAGE;
		}
	}
	else
	{
		//This a non-optical drive or a folder. We cannot *capture* an ISO image, but we can build one using mkisofs
		InsertMenu(hmenu, indexMenu, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_BUILDISO, txtBuildISO());
		lastID = IDM_BUILDISO;
	}

	WINCDEMU_LOG_LINE(L"CDriveContextMenu: loading icon...");
	HICON hIcon = (HICON) LoadImage(g_hModule, MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON, 16, 16, LR_SHARED);
	if (hIcon)
	{
		CComPtr<IWICImagingFactory> pFactory;
		HRESULT hR = pFactory.CoCreateInstance(CLSID_WICImagingFactory);
		if (SUCCEEDED(hR))
		{
			AddIconToMenuItem(pFactory, hmenu, indexMenu, TRUE, hIcon, FALSE, NULL);
		}
	}

	WINCDEMU_LOG_LINE(L"CDriveContextMenu::QueryContextMenu() returning...");
	return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(lastID + 1));
}

HRESULT STDMETHODCALLTYPE CDriveContextMenu::InvokeCommand(/* [in] */ __in CMINVOKECOMMANDINFO *pici)
{
	WINCDEMU_LOG_LINE(L"CDriveContextMenu::InvokeCommand()");

	if (!pici)
		return E_NOTIMPL;

	extern HINSTANCE g_hModule;
	TCHAR tszPath[MAX_PATH];
	GetModuleFileName(g_hModule, tszPath, _countof(tszPath));
	BazisLib::String path = String::sFormat(L"%s\\%s", BazisLib::Path::GetDirectoryName(BazisLib::Path::GetDirectoryName(String(tszPath))).c_str(), VMNT_EXE_W);

	switch ((int)pici->lpVerb)
	{
	case IDM_CREATEISO:
		ShellExecute(pici->hwnd, _T("open"), path.c_str(), String::sFormat(L"/createiso %s", m_FileName.c_str()).c_str(), NULL, pici->nShow);
		break;
	case IDM_CHANGEIMAGE:
		ShellExecute(pici->hwnd, _T("open"), path.c_str(), String::sFormat(L"/remount:%c", m_FileName[0]).c_str(), NULL, pici->nShow);
		break;
	case IDM_BUILDISO:
		ShellExecute(pici->hwnd, _T("open"), path.c_str(), String::sFormat(L"/isofromfolder \"%s\"", m_FileName.c_str()).c_str(), NULL, pici->nShow);
		break;
	default:
		return E_FAIL;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDriveContextMenu::GetCommandString(/* [in] */ __in UINT_PTR idCmd, /* [in] */ __in UINT uType, /* [in] */ __reserved UINT *pReserved, /* [out] */ __out_awcount(!(uType & GCS_UNICODE), cchMax) LPSTR pszName, /* [in] */ __in UINT cchMax)
{
	return E_NOTIMPL;
}
