#pragma once

#include <wincodec.h>           // WIC
HRESULT AddIconToMenuItem(__in IWICImagingFactory *pFactory, HMENU hmenu, int iMenuItem, BOOL fByPosition, HICON hicon, BOOL fAutoDestroy, __out_opt HBITMAP *phbmp);
