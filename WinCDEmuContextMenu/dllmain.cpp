// dllmain.cpp : Implementation of DllMain.

#include "stdafx.h"
#include "resource.h"
#include "WinCDEmuContextMenu_i.h"
#include "dllmain.h"
#include "dlldatax.h"

#include <bzshlp/Win32/i18n.h>
#include "../vmnt/RegistryParams.h"

CWinCDEmuContextMenuModule _AtlModule;
HINSTANCE g_hModule;

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	switch(dwReason)
	{
	case DLL_PROCESS_ATTACH:
		g_hModule = hInstance;
#ifdef BAZISLIB_LOCALIZATION_ENABLED
		{
			RegistryParams params;	//Registry params structure used by VMNT
			params.LoadFromRegistry();
			BazisLib::InitializeLNGBasedTranslationEngine(params.Language, _T("..\\langfiles"), hInstance);
		}
#endif
		break;
	}

#ifdef _MERGE_PROXYSTUB
	if (!PrxDllMain(hInstance, dwReason, lpReserved))
		return FALSE;
#endif
	hInstance;
	return _AtlModule.DllMain(dwReason, lpReserved); 
}
