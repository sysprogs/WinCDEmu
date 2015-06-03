// dllmain.h : Declaration of module class.

class CWinCDEmuContextMenuModule : public CAtlDllModuleT< CWinCDEmuContextMenuModule >
{
public :
	DECLARE_LIBID(LIBID_WinCDEmuContextMenuLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_WINCDEMUCONTEXTMENU, "{901EB7D4-307F-41A5-BB63-3070FCD11914}")
};

extern class CWinCDEmuContextMenuModule _AtlModule;
