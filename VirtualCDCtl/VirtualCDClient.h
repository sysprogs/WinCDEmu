#pragma once
#include <bzscore/autofile.h>
#include <vector>

#define SKIP_WINCDEMU_GUID_DEFINITION
#include "../BazisVirtualCDBus/DeviceControl.h"

enum MMCProfile
{
	mpInvalid				   = 0x0000,
	mpCdrom                    = 0x0008,
	mpCdRecordable             = 0x0009,
	mpCdRewritable             = 0x000a,
	mpDvdRom                   = 0x0010,
	mpDvdRecordable            = 0x0011,
	mpDvdRam                   = 0x0012,
	mpDvdRewritable            = 0x0013,
	mpDvdRWSequential          = 0x0014,
	mpDvdPlusRW                = 0x001A,
	mpDvdPlusR                 = 0x001B,
	mpDvdPlusRWDualLayer       = 0x002A,
	mpDvdPlusRDualLayer        = 0x002B,
	mpBDRom                    = 0x0040,
	mpBDRSequentialWritable    = 0x0041,
	mpBDRRandomWritable        = 0x0042,
	mpBDRewritable             = 0x0043,
	mpHDDVDRom                 = 0x0050,
	mpHDDVDRecordable          = 0x0051,
	mpHDDVDRam                 = 0x0052,
	mpHDDVDRewritable          = 0x0053,
	mpHDDVDRDualLayer          = 0x0058,
	mpHDDVDRWDualLayer         = 0x005A,
};

class ACFileWithIOCTL : public BazisLib::ACFile
{
public:
	ACFileWithIOCTL(const TCHAR *ptszFileName, const BazisLib::FileMode &mode, BazisLib::ActionStatus *pStatus = NULL)
		: BazisLib::ACFile(ptszFileName, mode, pStatus)
	{
	}

	ACFileWithIOCTL(const BazisLib::String &FileName, const BazisLib::FileMode &mode, BazisLib::ActionStatus *pStatus = NULL)
		: BazisLib::ACFile(FileName, mode, pStatus)
	{
	}


	DWORD DeviceIoControl(DWORD dwIoControlCode,
		LPCVOID lpInBuffer = NULL,
		DWORD nInBufferSize = 0,
		LPVOID lpOutBuffer = NULL,
		DWORD nOutBufferSize = 0,
		BazisLib::ActionStatus *pStatus = NULL)
	{
		return _File.DeviceIoControl(dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, pStatus);
	}

};

class VirtualCDClient
{
private:
	BazisLib::ManagedPointer<ACFileWithIOCTL> m_pDevice;
	
	BazisLib::ActionStatus TryDisconnectingUsingEject(LPCWSTR lpKernelFilePath);

public:
	static bool Win32FileNameToKernelFileName(LPCWSTR lpWin32Name, LPWSTR lpKernelName, size_t MaxKernelNameSize);

public:
	typedef bool (*PAUTORUNERRORHANDLER)(BazisLib::ActionStatus st);

	VirtualCDClient(BazisLib::ActionStatus *pStatus = NULL, bool usePortableDriver = false, bool tryBothDrivers = false);
	BazisLib::ActionStatus ConnectDisk(LPCWSTR lpKernelFilePath, char SuggestedLetter, unsigned MaxWaitTillMount, bool DisableAutorun = false, bool Persistent = false, PAUTORUNERRORHANDLER pAutorunErrorHandler = NULL, MMCProfile profile = mpInvalid);
	BazisLib::ActionStatus DisconnectDisk(LPCWSTR lpKernelFilePath);
	bool IsDiskConnected(LPCWSTR lpKernelFilePath);
	unsigned QueryVersion();
	static BazisLib::ActionStatus MountImageOnExistingDrive(const wchar_t *pImage, char driveLetter, unsigned timeout = 30000);

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
	BazisLib::ActionStatus SetDebugLogDirectory(LPCWSTR lpKernelDirPath);
#endif

	bool Valid()
	{
		return m_pDevice && m_pDevice->Valid();
	}

	struct VirtualCDEntry
	{
		BazisLib::DynamicStringW ImageFile;
		char DriveLetter;

		unsigned HandleCount, RefCount, DeviceRefCount;

		VirtualCDEntry(const wchar_t *pImageFile, char driveLtr)
			: ImageFile(pImageFile)
			, DriveLetter(driveLtr)
		{
		}

		bool operator==(const VirtualCDEntry &_Right) const
		{
			return (ImageFile == _Right.ImageFile) && (DriveLetter == _Right.DriveLetter);
		}

		bool operator<(const VirtualCDEntry &_Right) const
		{
			return DriveLetter < _Right.DriveLetter;
		}
	};

	static void RestartDriver(bool unmountAllDrives = true);

	typedef std::vector<VirtualCDEntry> VirtualCDList;

	VirtualCDList GetVirtualDiskList(const wchar_t *pFilter = NULL);

	
};