// VCDImgContextMenu.h : Declaration of the CVCDImgContextMenu

#pragma once
#include "resource.h"       // main symbols

#include "WinCDEmuContextMenu_i.h"
#include <bzscore/string.h>
#include "ContextMenuBase.h"


#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif



// CVCDImgContextMenu

class ATL_NO_VTABLE CVCDImgContextMenu :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CVCDImgContextMenu, &CLSID_VCDImgContextMenu>,
	public ContextMenuBase,

	public IContextMenu,
	public IShellExtInit
{

public:
	CVCDImgContextMenu()
	{
	}

DECLARE_REGISTRY_RESOURCEID(IDR_VCDIMGCONTEXTMENU)

DECLARE_NOT_AGGREGATABLE(CVCDImgContextMenu)

BEGIN_COM_MAP(CVCDImgContextMenu)
	COM_INTERFACE_ENTRY(IContextMenu)
	COM_INTERFACE_ENTRY(IShellExtInit)
END_COM_MAP()



	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

public:
	virtual HRESULT STDMETHODCALLTYPE Initialize( 
		/* [unique][in] */ 
		__in_opt  PCIDLIST_ABSOLUTE pidlFolder,
		/* [unique][in] */ 
		__in_opt  IDataObject *pdtobj,
		/* [unique][in] */ 
		__in_opt  HKEY hkeyProgID);

	virtual HRESULT STDMETHODCALLTYPE QueryContextMenu( 
		/* [in] */ 
		__in  HMENU hmenu,
		/* [in] */ 
		__in  UINT indexMenu,
		/* [in] */ 
		__in  UINT idCmdFirst,
		/* [in] */ 
		__in  UINT idCmdLast,
		/* [in] */ 
		__in  UINT uFlags);

	virtual HRESULT STDMETHODCALLTYPE InvokeCommand( 
		/* [in] */ 
		__in  CMINVOKECOMMANDINFO *pici);

	virtual HRESULT STDMETHODCALLTYPE GetCommandString( 
		/* [in] */ 
		__in  UINT_PTR idCmd,
		/* [in] */ 
		__in  UINT uType,
		/* [in] */ 
		__reserved  UINT *pReserved,
		/* [out] */ 
		__out_awcount(!(uType & GCS_UNICODE), cchMax)  LPSTR pszName,
		/* [in] */ 
		__in  UINT cchMax);

};

OBJECT_ENTRY_AUTO(__uuidof(VCDImgContextMenu), CVCDImgContextMenu)
