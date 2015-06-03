#include "StdAfx.h"
#include "ContextMenuBase.h"


HRESULT STDMETHODCALLTYPE ContextMenuBase::Initialize( /* [unique][in] */ __in_opt PCIDLIST_ABSOLUTE pidlFolder, /* [unique][in] */ __in_opt IDataObject *pdtobj, /* [unique][in] */ __in_opt HKEY hkeyProgID)
{
	if (!pdtobj)
		return E_INVALIDARG;

	FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stm;
	HRESULT hR = pdtobj->GetData(&fe, &stm);
	if (SUCCEEDED(hR))
	{
		HDROP hDrop = static_cast<HDROP>(GlobalLock(stm.hGlobal));
		if (hDrop == NULL)
			hR = E_FAIL;
		else
		{
			UINT nFiles = DragQueryFile(hDrop, -1, NULL, 0);
			if (nFiles != 1)
				hR = E_INVALIDARG;
			else
			{
				size_t done = DragQueryFile(hDrop, 0, m_FileName.PreAllocate(MAX_PATH, false), MAX_PATH);
				if (done)
				{
					m_FileName.SetLength(done);
					hR = S_OK;
				}
				else
					hR = E_FAIL;
			}

			GlobalUnlock(stm.hGlobal);
		}

		ReleaseStgMedium(&stm);
	}

	return hR;
}
