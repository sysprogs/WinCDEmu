// VCDImgContextMenu.cpp : Implementation of CVCDImgContextMenu

#include "stdafx.h"
#include "VCDImgContextMenu.h"
#include "../vmnt/ContextMenuLabels.h"
#include "../vmnt/RegistryParams.h"
#include "../VirtualCDCtl/VirtualCDClient.h"

#define IDM_SELECTANDMOUNT	0
#define IDM_REMOUNTLTRBASE	1

// CVCDImgContextMenu

#include <bzscore/file.h>
using namespace BazisLib;
#include "DebugLog.h"

extern HINSTANCE g_hModule;

HRESULT STDMETHODCALLTYPE CVCDImgContextMenu::Initialize(/* [unique][in] */ __in_opt PCIDLIST_ABSOLUTE pidlFolder, /* [unique][in] */ __in_opt IDataObject *pdtobj, /* [unique][in] */ __in_opt HKEY hkeyProgID)
{
	WINCDEMU_LOG_LINE(L"CVCDImgContextMenu::Initialize()");
	HRESULT hR = ContextMenuBase::Initialize(pidlFolder, pdtobj, hkeyProgID);
	if (!SUCCEEDED(hR))
	{
		WINCDEMU_LOG_LINE(L"ContextMenuBase::Initialize() failed: %x", hR);
		return hR;
	}

	WINCDEMU_LOG_LINE(L"CVCDImgContextMenu: file name size =  %d", m_FileName.size());
	WINCDEMU_LOG_LINE(L"CVCDImgContextMenu: file name =  %s", m_FileName.c_str());

	String ext = BazisLib::Path::GetExtensionExcludingDot(m_FileName);
	if (ext.length() > 0 && ext[0] == '.')
		ext = ext.substr(1);

	for each (LPCTSTR lpExt in lpSupportedExtensions)
		if (!ext.icompare(lpExt))
		{
			WINCDEMU_LOG_LINE(L"CVCDImgContextMenu: extension matches", m_FileName.c_str());
			return S_OK;
		}

	WINCDEMU_LOG_LINE(L"CVCDImgContextMenu: extension does not match", m_FileName.c_str());
	return E_FAIL;
}

extern HINSTANCE g_hModule;
#include "TransparentMenuBitmap.h"

HRESULT STDMETHODCALLTYPE CVCDImgContextMenu::QueryContextMenu(/* [in] */ __in HMENU hmenu, /* [in] */ __in UINT indexMenu, /* [in] */ __in UINT idCmdFirst, /* [in] */ __in UINT idCmdLast, /* [in] */ __in UINT uFlags)
{
	WINCDEMU_LOG_LINE(L"CVCDImgContextMenu::QueryContextMenu()");

	if (uFlags & CMF_DEFAULTONLY)
		return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));

	DWORD lastCommand = IDM_SELECTANDMOUNT;
	InsertMenu(hmenu,
		indexMenu,
		MF_STRING | MF_BYPOSITION,
		idCmdFirst + IDM_SELECTANDMOUNT,
		txtSelectAndMount()
		);

	WINCDEMU_LOG_LINE(L"CVCDImgContextMenu: loading icon...");
	HICON hIcon = (HICON)LoadImage(g_hModule, MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON, 16, 16, LR_SHARED);
	if (hIcon)
	{
		CComPtr<IWICImagingFactory> pFactory;
		HRESULT hR = pFactory.CoCreateInstance(CLSID_WICImagingFactory);
		if (SUCCEEDED(hR))
		{
			AddIconToMenuItem(pFactory, hmenu, indexMenu, TRUE, hIcon, FALSE, NULL);
		}
	}

	WINCDEMU_LOG_LINE(L"CVCDImgContextMenu: querying devices...");
	VirtualCDClient clt;
	VirtualCDClient::VirtualCDList devs = clt.GetVirtualDiskList();
	if (!devs.empty())
	{
		HMENU hSubmenu = CreatePopupMenu();
		unsigned submenuPosition = 0;

		for each(const VirtualCDClient::VirtualCDEntry &entry in devs)
		{
			unsigned ID = IDM_REMOUNTLTRBASE + (entry.DriveLetter & ~0x40);
			TCHAR tszDrive[] = _T("?:");
			tszDrive[0] = entry.DriveLetter;
			InsertMenu(hSubmenu, submenuPosition++, MF_BYPOSITION, idCmdFirst + ID, tszDrive);
			lastCommand = max(lastCommand, ID);
		}
			
		MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
		mii.fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_ID;
		mii.wID = idCmdFirst + IDM_REMOUNTLTRBASE;
		mii.hSubMenu = hSubmenu;
		mii.dwTypeData = const_cast<LPWSTR>(txtMountToExistingDrive());
		InsertMenuItem(hmenu, indexMenu + 1, TRUE, &mii);
	}

	WINCDEMU_LOG_LINE(L"CVCDImgContextMenu::QueryContextMenu() returning...");
	return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(lastCommand + 1));
}

HRESULT STDMETHODCALLTYPE CVCDImgContextMenu::InvokeCommand(/* [in] */ __in CMINVOKECOMMANDINFO *pici)
{
	WINCDEMU_LOG_LINE(L"CVCDImgContextMenu::InvokeCommand()");

	if (!pici)
	{
		WINCDEMU_LOG_LINE(L"CVCDImgContextMenu::InvokeCommand() - no info");
		return E_NOTIMPL;
	}

	TCHAR tszPath[MAX_PATH] = { 0, };
	GetModuleFileName(g_hModule, tszPath, _countof(tszPath));
	BazisLib::String path = String::sFormat(L"%s\\%s", BazisLib::Path::GetDirectoryName(BazisLib::Path::GetDirectoryName(String(tszPath))).c_str(), VMNT_EXE_W);
	WINCDEMU_LOG_LINE(L"Mounter: %s", path.c_str());

	switch ((int)pici->lpVerb)
	{
	case IDM_SELECTANDMOUNT:
		WINCDEMU_LOG_LINE(L"ShellExecute() for %s", m_FileName.c_str());
		ShellExecute(pici->hwnd, _T("open"), path.c_str(), String::sFormat(L"\"%s\" /ltrselect", m_FileName.c_str()).c_str(), NULL, pici->nShow);
		WINCDEMU_LOG_LINE(L"ShellExecute() done");
		break;
	default:
		{
			int letter = (int) pici->lpVerb - IDM_REMOUNTLTRBASE;
			letter |= 0x40;
			if (letter >= 'A' && letter <= 'Z')
			{
				WINCDEMU_LOG_LINE(L"ShellExecute() for %s", m_FileName.c_str());
				HINSTANCE result = ShellExecute(pici->hwnd, _T("open"), path.c_str(), String::sFormat(L"\"%s\" /remount:%c", m_FileName.c_str(), (char)letter).c_str(), NULL, pici->nShow);
				WINCDEMU_LOG_LINE(L"ShellExecute() done: %d", result);
			}
			else
				return E_FAIL;
		}
	}
		
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCDImgContextMenu::GetCommandString(/* [in] */ __in UINT_PTR idCmd, /* [in] */ __in UINT uType, /* [in] */ __reserved UINT *pReserved, /* [out] */ __out_awcount(!(uType & GCS_UNICODE), cchMax) LPSTR pszName, /* [in] */ __in UINT cchMax)
{
	return E_NOTIMPL;
}
