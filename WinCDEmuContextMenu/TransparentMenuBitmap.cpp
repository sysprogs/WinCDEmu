#include "stdafx.h"

//Based on the sample code from http://msdn.microsoft.com/en-us/library/bb757020.aspx

#include <windowsx.h>           // For WM_COMMAND handling macros
#include <shlobj.h>             // For shell
#include <shlwapi.h>            // QISearch, easy way to implement QI
#include <commctrl.h>
#include <wincodec.h>           // WIC
#include "resource.h"

#pragma comment(lib, "shlwapi") // Default link libs do not include this.
#pragma comment(lib, "comctl32")
#pragma comment(lib, "WindowsCodecs")    // WIC

// AddIconToMenuItem and its supporting functions.
// Note: BufferedPaintInit/BufferedPaintUnInit should be called to
// improve performance.
// In this sample they are called in _OnInitDlg/_OnDestroyDlg.
// In a full application you would call these during WM_NCCREATE/WM_NCDESTROY.

typedef DWORD ARGB;

void InitBitmapInfo(__out_bcount(cbInfo) BITMAPINFO *pbmi, ULONG cbInfo, LONG cx, LONG cy, WORD bpp)
{
	ZeroMemory(pbmi, cbInfo);
	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biPlanes = 1;
	pbmi->bmiHeader.biCompression = BI_RGB;

	pbmi->bmiHeader.biWidth = cx;
	pbmi->bmiHeader.biHeight = cy;
	pbmi->bmiHeader.biBitCount = bpp;
}

HRESULT Create32BitHBITMAP(HDC hdc, const SIZE *psize, __deref_opt_out void **ppvBits, __out HBITMAP* phBmp)
{
	*phBmp = NULL;

	BITMAPINFO bmi;
	InitBitmapInfo(&bmi, sizeof(bmi), psize->cx, psize->cy, 32);

	HDC hdcUsed = hdc ? hdc : GetDC(NULL);
	if (hdcUsed)
	{
		*phBmp = CreateDIBSection(hdcUsed, &bmi, DIB_RGB_COLORS, ppvBits, NULL, 0);
		if (hdc != hdcUsed)
		{
			ReleaseDC(NULL, hdcUsed);
		}
	}
	return (NULL == *phBmp) ? E_OUTOFMEMORY : S_OK;
}

HRESULT AddBitmapToMenuItem(HMENU hmenu, int iItem, BOOL fByPosition, HBITMAP hbmp)
{
	HRESULT hr = E_FAIL;

	MENUITEMINFO mii = { sizeof(mii) };
	mii.fMask = MIIM_BITMAP;
	mii.hbmpItem = hbmp;
	if (SetMenuItemInfo(hmenu, iItem, fByPosition, &mii))
	{
		hr = S_OK;
	}

	return hr;
}

HRESULT AddIconToMenuItem(__in IWICImagingFactory *pFactory,
	HMENU hmenu, int iMenuItem, BOOL fByPosition, HICON hicon, BOOL fAutoDestroy, __out_opt HBITMAP *phbmp)
{
	HBITMAP hbmp = NULL;

	IWICBitmap *pBitmap;
	HRESULT hr = pFactory->CreateBitmapFromHICON(hicon, &pBitmap);
	if (SUCCEEDED(hr))
	{
		IWICFormatConverter *pConverter;
		hr = pFactory->CreateFormatConverter(&pConverter);
		if (SUCCEEDED(hr))
		{
			hr = pConverter->Initialize(pBitmap, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
			if (SUCCEEDED(hr))
			{
				UINT cx, cy;
				hr = pConverter->GetSize(&cx, &cy);
				if (SUCCEEDED(hr))
				{
					const SIZE sizIcon = { (int) cx, -(int) cy };
					BYTE *pbBuffer;
					hr = Create32BitHBITMAP(NULL, &sizIcon, reinterpret_cast<void **>(&pbBuffer), &hbmp);
					if (SUCCEEDED(hr))
					{
						const UINT cbStride = cx * sizeof(ARGB);
						const UINT cbBuffer = cy * cbStride;
						hr = pConverter->CopyPixels(NULL, cbStride, cbBuffer, pbBuffer);
					}
				}
			}

			pConverter->Release();
		}

		pBitmap->Release();
	}

	if (SUCCEEDED(hr))
	{
		hr = AddBitmapToMenuItem(hmenu, iMenuItem, fByPosition, hbmp);
	}

	if (FAILED(hr))
	{
		DeleteObject(hbmp);
		hbmp = NULL;
	}

	if (fAutoDestroy)
	{
		DestroyIcon(hicon);
	}

	if (phbmp)
	{
		*phbmp = hbmp;
	}

	return hr;
}

//-------------------------------------------------------------------
//-- TrackPopupIconMenuEx -------------------------------------------
// Wrapper to TrackPopupMenuEx that shows how to use AddIconToMenuItem // and how to manage the lifetime of the created bitmaps
struct ICONMENUENTRY
{
	UINT        idMenuItem;
	HINSTANCE   hinst;
	LPCWSTR     pIconId;
};

BOOL TrackPopupIconMenuEx(__in IWICImagingFactory *pFactory,
	HMENU hmenu, UINT fuFlags, int x, int y, HWND hwnd, __in_opt LPTPMPARAMS lptpm,
	UINT nIcons, __in_ecount_opt(nIcons) const ICONMENUENTRY *pIcons)
{
	HRESULT hr = S_OK;
	BOOL fRet = FALSE;

	MENUINFO menuInfo = { sizeof(menuInfo) };
	menuInfo.fMask = MIM_STYLE;

	if (nIcons)
	{
		for (UINT n = 0; SUCCEEDED(hr) && n < nIcons; ++n)
		{
			HICON hicon;
			hr = LoadIconMetric(pIcons[n].hinst, pIcons[n].pIconId, LIM_SMALL, &hicon);
			if (SUCCEEDED(hr))
			{
				/* hr = */ AddIconToMenuItem(pFactory, hmenu, pIcons[n].idMenuItem, FALSE, hicon, TRUE, NULL);
			}
		}

		GetMenuInfo(hmenu, &menuInfo);

		MENUINFO menuInfoNew = menuInfo;
		menuInfoNew.dwStyle = (menuInfo.dwStyle & ~MNS_NOCHECK) | MNS_CHECKORBMP;
		SetMenuInfo(hmenu, &menuInfoNew);
	}

	if (SUCCEEDED(hr))
	{
		fRet = TrackPopupMenuEx(hmenu, fuFlags, x, y, hwnd, lptpm) ? S_OK : E_FAIL;
	}

	if (nIcons)
	{
		for (UINT n = 0; n < nIcons; ++n)
		{
			MENUITEMINFO mii = { sizeof(mii) };
			mii.fMask = MIIM_BITMAP;

			if (GetMenuItemInfo(hmenu, pIcons[n].idMenuItem, FALSE, &mii))
			{
				DeleteObject(mii.hbmpItem);
				mii.hbmpItem = NULL;
				SetMenuItemInfo(hmenu, pIcons[n].idMenuItem, FALSE, &mii);
			}
		}

		SetMenuInfo(hmenu, &menuInfo);
	}

	return fRet;
}