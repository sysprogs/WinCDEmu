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

extern HINSTANCE g_hModule;

HRESULT STDMETHODCALLTYPE CVCDImgContextMenu::Initialize(/* [unique][in] */ __in_opt PCIDLIST_ABSOLUTE pidlFolder, /* [unique][in] */ __in_opt IDataObject *pdtobj, /* [unique][in] */ __in_opt HKEY hkeyProgID)
{
	HRESULT hR = ContextMenuBase::Initialize(pidlFolder, pdtobj, hkeyProgID);
	if (!SUCCEEDED(hR))
		return hR;

	String ext = BazisLib::Path::GetExtensionExcludingDot(m_FileName);
	if (ext.length() > 0 && ext[0] == '.')
		ext = ext.substr(1);

	for each (LPCTSTR lpExt in lpSupportedExtensions)
		if (!ext.icompare(lpExt))
			return S_OK;

	return E_FAIL;
}

extern HINSTANCE g_hModule;
#include "TransparentMenuBitmap.h"

HRESULT STDMETHODCALLTYPE CVCDImgContextMenu::QueryContextMenu(/* [in] */ __in HMENU hmenu, /* [in] */ __in UINT indexMenu, /* [in] */ __in UINT idCmdFirst, /* [in] */ __in UINT idCmdLast, /* [in] */ __in UINT uFlags)
{
	if (uFlags & CMF_DEFAULTONLY)
		return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));

	DWORD lastCommand = IDM_SELECTANDMOUNT;
	InsertMenu(hmenu,
		indexMenu,
		MF_STRING | MF_BYPOSITION,
		idCmdFirst + IDM_SELECTANDMOUNT,
		txtSelectAndMount()
		);

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

	return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(lastCommand + 1));
}

HRESULT STDMETHODCALLTYPE CVCDImgContextMenu::InvokeCommand(/* [in] */ __in CMINVOKECOMMANDINFO *pici)
{
	if (!pici)
		return E_NOTIMPL;

	TCHAR tszPath[MAX_PATH];
	GetModuleFileName(g_hModule, tszPath, _countof(tszPath));
	BazisLib::String path = String::sFormat(L"%s\\%s", BazisLib::Path::GetDirectoryName(BazisLib::Path::GetDirectoryName(String(tszPath))).c_str(), VMNT_EXE_W);

	switch ((int)pici->lpVerb)
	{
	case IDM_SELECTANDMOUNT:
		ShellExecute(pici->hwnd, _T("open"), path.c_str(), String::sFormat(L"\"%s\" /ltrselect", m_FileName.c_str()).c_str(), NULL, pici->nShow);
		break;
	default:
		{
			int letter = (int) pici->lpVerb - IDM_REMOUNTLTRBASE;
			letter |= 0x40;
			if (letter >= 'A' && letter <= 'Z')
				ShellExecute(pici->hwnd, _T("open"), path.c_str(), String::sFormat(L"\"%s\" /remount:%c", m_FileName.c_str(), (char)letter).c_str(), NULL, pici->nShow);
		}
	}
		
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCDImgContextMenu::GetCommandString(/* [in] */ __in UINT_PTR idCmd, /* [in] */ __in UINT uType, /* [in] */ __reserved UINT *pReserved, /* [out] */ __out_awcount(!(uType & GCS_UNICODE), cchMax) LPSTR pszName, /* [in] */ __in UINT cchMax)
{
	return E_NOTIMPL;
}
