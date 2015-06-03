#pragma once
#include <bzscore/string.h>

class ContextMenuBase
{
protected:
	BazisLib::String m_FileName;

public:
	HRESULT STDMETHODCALLTYPE Initialize(/* [unique][in] */ __in_opt PCIDLIST_ABSOLUTE pidlFolder, /* [unique][in] */ __in_opt IDataObject *pdtobj, /* [unique][in] */ __in_opt HKEY hkeyProgID);
};

