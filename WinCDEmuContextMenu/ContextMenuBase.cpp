#include "StdAfx.h"
#include "ContextMenuBase.h"
#include "DebugLog.h"

HRESULT STDMETHODCALLTYPE ContextMenuBase::Initialize( /* [unique][in] */ __in_opt PCIDLIST_ABSOLUTE pidlFolder, /* [unique][in] */ __in_opt IDataObject *pdtobj, /* [unique][in] */ __in_opt HKEY hkeyProgID)
{
	WINCDEMU_LOG_LINE(L"ContextMenuBase::Initialize(): %x", pdtobj);

	if (!pdtobj)
		return E_INVALIDARG;

	FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stm;
	HRESULT hR = pdtobj->GetData(&fe, &stm);
	WINCDEMU_LOG_LINE(L"pdtobj->GetData(): %x", hR);
	if (SUCCEEDED(hR))
	{
		HDROP hDrop = static_cast<HDROP>(GlobalLock(stm.hGlobal));
		WINCDEMU_LOG_LINE(L"GlobalLock(): %x", hDrop);
		if (hDrop == NULL)
			hR = E_FAIL;
		else
		{
			UINT nFiles = DragQueryFile(hDrop, -1, NULL, 0);
			WINCDEMU_LOG_LINE(L"DragQueryFile(-1): %d", nFiles);
			if (nFiles != 1)
				hR = E_INVALIDARG;
			else
			{
				size_t done = DragQueryFile(hDrop, 0, m_FileName.PreAllocate(MAX_PATH, false), MAX_PATH);
				WINCDEMU_LOG_LINE(L"DragQueryFile(0): %d, allocated=%d", done, m_FileName.GetAllocated());

				if (done && done <= m_FileName.GetAllocated())
				{
					m_FileName.SetLength(done);
					WINCDEMU_LOG_LINE(L"File name: %s", m_FileName.c_str());
					hR = S_OK;
				}
				else
					hR = E_FAIL;
			}

			WINCDEMU_LOG_LINE(L"Unlocking %x...", stm.hGlobal);
			GlobalUnlock(stm.hGlobal);
		}

		WINCDEMU_LOG_LINE(L"Releasing medium");
		ReleaseStgMedium(&stm);
	}

	return hR;
}
