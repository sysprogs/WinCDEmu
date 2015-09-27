// dllmain.cpp : Implementation of DllMain.

#include "stdafx.h"
#include "resource.h"
#include "WinCDEmuContextMenu_i.h"
#include "dllmain.h"
#include "dlldatax.h"

#include <bzshlp/Win32/i18n.h>
#include "../vmnt/RegistryParams.h"
#include "DebugLog.h"

CWinCDEmuContextMenuModule _AtlModule;
HINSTANCE g_hModule;

#ifdef ENABLE_DEBUG_LOGGING
#include <ShlObj.h>

static BazisLib::Mutex s_LogMutex;
static BazisLib::File *s_pLogFile;


void LogDebugLine(BazisLib::DynamicString line)
{
	BazisLib::MutexLocker lck(s_LogMutex);
	if (s_pLogFile)
	{
		BazisLib::DateTime now = BazisLib::DateTime::Now();
		line = BazisLib::String::sFormat(L"[%02d:%02d:%02d] ", now.GetHour(), now.GetMinute(), now.GetSecond()) + line;
		s_pLogFile->WriteLine(line);
	}
}

#endif


// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	switch(dwReason)
	{
	case DLL_PROCESS_ATTACH:
		g_hModule = hInstance;
#ifdef ENABLE_DEBUG_LOGGING
		{
			BazisLib::MutexLocker lck(s_LogMutex);
			wchar_t wszLogFile[MAX_PATH] = { 0, };
			SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, wszLogFile);
			wcscat_s(wszLogFile, L"\\WinCDEmuLog.txt");
			s_pLogFile = new BazisLib::File(wszLogFile, BazisLib::CreateOrTruncateRW);
		}
#endif
#ifdef BAZISLIB_LOCALIZATION_ENABLED
		{
			WINCDEMU_LOG_LINE(L"Loading settings...");
			RegistryParams params;	//Registry params structure used by VMNT
			params.LoadFromRegistry();
			WINCDEMU_LOG_LINE(L"Initializing translation engine...");
			BazisLib::InitializeLNGBasedTranslationEngine(params.Language, _T("..\\langfiles"), hInstance);
		}
#endif
		break;
	case DLL_PROCESS_DETACH:
#ifdef ENABLE_DEBUG_LOGGING
		{
			WINCDEMU_LOG_LINE(L"Unloading...");
			BazisLib::MutexLocker lck(s_LogMutex);
			delete s_pLogFile;
			s_pLogFile = NULL;
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
