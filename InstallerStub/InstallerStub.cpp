// InstallerStub.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "InstallerStub.h"

#include "CustomInstaller.h"

#include "../../../svn/win32/installer/ssibase/intl.h"
#include "../../../svn/win32/installer/ssibase/resource.h"
static ResourceStringPool g_StringPool(GetModuleHandle(NULL));

#pragma comment(lib, "comctl32")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


CErrorManager *g_pErrorMgr;
CPathSubstitutionManager *g_pPathSub;
CTempFileManager *g_pTmpMgr;

int DetectStubSize()
{
	char szThisFile[512];
	int ExeFileOffset = 0;
	GetModuleFileName(GetModuleHandle(0), szThisFile, sizeof(szThisFile));
	HANDLE h = CreateFile(szThisFile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (h != INVALID_HANDLE_VALUE)
	{
		int L = GetFileSize(h, 0);
		char *p = (char *)malloc(L);
		DWORD dwOk;
		ReadFile(h, p, L, &dwOk, 0);
		if (dwOk == L)
		{
			for (int i = 0; i < (L - sizeof(szSSISIGN)); i++)
			{
				if (!memcmp(p + i, szSSISIGN, sizeof(szSSISIGN)))
				{
					ExeFileOffset = i;
					break;
				}
			}
		}
		free(p);
		CloseHandle(h);
	}
	return ExeFileOffset;
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR    lpCmdLine,
                     int       nCmdShow)
{
char szThisFile[512];
GetModuleFileName(GetModuleHandle(0), szThisFile, sizeof(szThisFile));
int StubSize = DetectStubSize();
if (!StubSize)
{
	MessageBox(0,g_StringPool.LoadStringForLanguage(IDS_NOPACKET),g_StringPool.LoadStringForLanguage(IDS_ERROR),MB_ICONERROR);
	return 2;
}

int ver = QueryPacketFormatVersion(szThisFile,StubSize);
if (ver == -1)
	{
	MessageBox(0,g_StringPool.LoadStringForLanguage(IDS_NOVERSION),g_StringPool.LoadStringForLanguage(IDS_ERROR),MB_ICONERROR);
	return 2;
	}
InitializeVersionInformation();
g_pErrorMgr = new CErrorManager;
g_pPathSub = new CPathSubstitutionManager;
g_pTmpMgr = new CTempFileManager;
POVERRIDEMODULE pOver = CheckForCoreOverride(ver);
if (pOver)
	{
	if (PerformOverrideRequest(pOver, szThisFile, StubSize) == OVERRIDE_ERROR)
		{
		if (MessageBox(0,g_StringPool.LoadStringForLanguage(IDS_BADOVERRIDE),g_StringPool.LoadStringForLanguage(IDS_QUESTION),MB_ICONQUESTION|MB_YESNO) == IDNO)
			{
			delete g_pTmpMgr;
			delete g_pPathSub;
			delete g_pErrorMgr;
			return 3;
			}
		}
	else
		{
		delete g_pTmpMgr;
		delete g_pPathSub;
		delete g_pErrorMgr;
		return 0x10;
		}
	xxfree(pOver);
	}

InitCommonControls();
InitializePathSub(g_pPathSub);

bool Unattended = false;
char szComponentOverrideString[512];
memset(szComponentOverrideString, 0, sizeof(szComponentOverrideString));
if (lpCmdLine)
{
	char *p = strstr(lpCmdLine, "/COMPOVER:");
	if (p)
	{
		p += 9;
		unsigned copyLen = 0;
		char *p2 = strchr(p, ' ');
		if (p2)
			copyLen = p2 - p;
		else
			copyLen = strlen(p);
		if (copyLen >= (sizeof(szComponentOverrideString) - 1))
			copyLen = 0;
		if (copyLen)
			memcpy(szComponentOverrideString, p, copyLen);
	}
	if (strstr(lpCmdLine, "/UNATTENDED"))
		Unattended = true;

}

unsigned StatusCode = 0;
CInstallationPacketInstaller *pInst = new CustomInstaller(hInstance,szThisFile,StubSize);
if (!pInst->PerformInstallation(szComponentOverrideString[0] ? szComponentOverrideString : 0, Unattended))
	{
		StatusCode = 100;
		MessageBox(0,g_StringPool.LoadStringForLanguage(IDS_INSTFAILED),g_StringPool.LoadStringForLanguage(IDS_ERROR),MB_ICONERROR);
	}
else
	{
/*	if (pInst->NeedSuccessReport())
		MessageBox(0,g_StringPool.LoadStringForLanguage(IDS_INSTDONE),g_StringPool.LoadStringForLanguage(IDS_INFORMATION),MB_ICONINFORMATION);
		*/
	}
delete pInst;
delete g_pTmpMgr;
delete g_pPathSub;
delete g_pErrorMgr;
return StatusCode;
}