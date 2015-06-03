#pragma once
#include "../../../svn/win32/installer/ssibase/ssibase.h"
#include <bzscore/thread.h>

class CustomInstaller : public CInstallationPacketInstaller
{
private:
	BazisLib::FunctionThread m_InstallThread;
	HWND m_hWnd;
	bool m_bExtOptionsVisible;
	bool m_bUnattended;

public:
	virtual bool PerformInstallation(const char *ComponentOverrideString = 0, bool Unattended = false);

	CustomInstaller(HINSTANCE hInst, char *pszSourceFile, ULONGLONG InitialOffset)
		: CInstallationPacketInstaller(hInst, pszSourceFile, InitialOffset)
		, m_InstallThread(&InstallationThread, this)
		, m_bExtOptionsVisible(true)
		, m_bUnattended(false)
	{
	}

	enum {WMX_DONE = WM_USER + 67};	//wParam = bSuccessful

protected:
	virtual bool OnPartialCompletion(int FileIndex,
		ULONGLONG FileSize,
		ULONGLONG BytesDone,
		ULONGLONG TotalSize,
		ULONGLONG TotalDone);

	virtual bool OnNextFile(char *pszFileName);

private:
	static BOOL CALLBACK sInstallerDialogProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);
	BOOL InstallerDialogProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam);

	static int InstallationThread(LPVOID lpParam);

	void UpdateInstallationState(bool running);

	virtual void OnPostInstallCommand(PCOMMAND pCommand);
	void ShowExtendedOptions(bool bShow);

private:
	void ApplyCustomSettings();
};
