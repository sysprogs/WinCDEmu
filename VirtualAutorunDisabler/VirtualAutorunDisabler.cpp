// VirtualAutorunDisabler.cpp : Implementation of WinMain


#include "stdafx.h"
#include "resource.h"
#include "VirtualAutorunDisabler_i.h"


class CVirtualAutorunDisablerModule : public CAtlExeModuleT< CVirtualAutorunDisablerModule >
{
public :
	DECLARE_LIBID(LIBID_VirtualAutorunDisablerLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_VIRTUALAUTORUNDISABLER, "{6C50E507-74A2-4434-95A6-53563A797FF6}")
};

CVirtualAutorunDisablerModule _AtlModule;



//
extern "C" int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, 
                                LPTSTR /*lpCmdLine*/, int nShowCmd)
{
    return _AtlModule.WinMain(nShowCmd);
}

