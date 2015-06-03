#include "stdafx.h"
#include <bzshlp/WinKernel/SCSI/VirtualSCSIPDO.h>
#include <bzscore/string.h>
#include <bzshlp/WinKernel/devapi.h>
#include <string>
#include <vector>
#include <map>
#include "BazisVirtualCDBus.h"
#include <bzshlp/WinKernel/driver.h>

using namespace BazisLib;
using namespace BazisLib::SCSI;
using namespace DDK;
using namespace Security;


class WinCDEmuPDO : public VirtualSCSIPDO
{
public:
	WinCDEmuPDO(const ManagedPointer<AIBasicSCSIDevice> &pScsiDevice, const wchar_t *pDeviceName = L"Virtual SCSI Device", const wchar_t *pUniqueID = NULL, bool UnplugPDOOnSCSIEject = false)
		: VirtualSCSIPDO(pScsiDevice, pDeviceName, pUniqueID, UnplugPDOOnSCSIEject)
	{
	}

	void QueryReferenceCounts(unsigned short *pDevRefCount, unsigned short *pHandleCount, unsigned short *pRefCount)
	{
		PDEVICE_OBJECT pObj = GetDeviceObject();
		PDEVICE_OBJECT pHighest = IoGetAttachedDeviceReference(pObj);
		*pDevRefCount = pHighest->ReferenceCount;
		*pHandleCount = GetObjectHandleCount(pHighest);
		*pRefCount = GetObjectReferenceCount(pHighest);
		ObDereferenceObject(pHighest);
	}
};


extern "C" 
{
	NTSTATUS NTAPI
  ZwAllocateVirtualMemory(
    IN HANDLE  ProcessHandle,
    IN OUT PVOID  *BaseAddress,
    IN ULONG  ZeroBits,
    IN OUT PSIZE_T  RegionSize,
    IN ULONG  AllocationType,
    IN ULONG  Protect
    ); 

	NTSTATUS NTAPI
		ZwFreeVirtualMemory(
		__in HANDLE  ProcessHandle,
		__inout PVOID  *BaseAddress,
		__inout PSIZE_T  RegionSize,
		__in ULONG  FreeType
		); 

}


#ifndef WINCDEMU_PORTABLE
DDK::Driver *_stdcall CreateMainDriverInstance()
{
	return new _GenericPnpDriver<BazisVirtualCDBus, &GUID_BazisVirtualCDBus>();
}
#endif

void BazisVirtualCDBus::RegisterNewDeviceLink(const String &deviceId, char SuggestedDriveLetter, bool cleanupOldLinks)
{
	static const wchar_t wszVCDPrefix[] = L"\\??\\BazisVirtualCDBus#StandardDevice#";
	RegistryKey key(L"\\Registry\\Machine\\System\\MountedDevices");
	TypedBuffer<KEY_VALUE_FULL_INFORMATION> info;

	if (cleanupOldLinks)
	{
		for (unsigned i = 0; ; i++)
		{
			if (!key.EnumValue(i, &info))
				break;
			if ((info->DataLength / sizeof(wchar_t)) < __countof(wszVCDPrefix))
				continue;
			PWCHAR pwszData = (PWCHAR)(((char *)info.GetData()) + info->DataOffset);
			if (memcmp(pwszData, wszVCDPrefix, sizeof(wszVCDPrefix) - sizeof(wszVCDPrefix[0])))
				continue;
			if (key.DeleteValue(info->Name, info->NameLength / sizeof(wchar_t)))
				i--;	//Indices have been shifted after last deletion
		}
	}

	if (deviceId.empty() || !SuggestedDriveLetter)
		return;

	String strHardwareId;
	const wchar_t wszGUID_DEVINTERFACE_VOLUME[] = L"{53f5630d-b6bf-11d0-94f2-00a0c91efb8b}";
	strHardwareId.Format(L"%s%s#%s", wszVCDPrefix, deviceId.c_str(), wszGUID_DEVINTERFACE_VOLUME);

	wchar_t wszKeyName[] = L"\\DosDevices\\X:";
	wszKeyName[12] = SuggestedDriveLetter & ~0x20;	//upcase()

	key[wszKeyName].SetValueEx(strHardwareId.c_str(), strHardwareId.length() * sizeof(wchar_t), REG_BINARY);
}

NTSTATUS BazisVirtualCDBus::ReportDeviceList(VirtualCDRecordHeader *pBuffer, const DynamicString &LetterOrImage, ULONG OutputLength, PULONG_PTR pBytesDone)
{
	if (!pBuffer || (OutputLength < sizeof(VirtualCDRecordHeader)))
		return STATUS_BUFFER_TOO_SMALL;
	SynchronizedMapPointer pMap(*this);

	pBuffer->Version = VirtualCDRecordHeader::CurrentVersion;
	pBuffer->RecordCount = 0;

	size_t elCount = pMap->size();
	if (!elCount)
	{
		pBuffer->TotalSize = sizeof(VirtualCDRecordHeader);
		*pBytesDone = pBuffer->TotalSize;
		return STATUS_SUCCESS;
	}

	if (OutputLength < (sizeof(VirtualCDRecordHeader) + elCount * sizeof(VirtualCDRecord)))
	{
		pBuffer->TotalSize = sizeof(VirtualCDRecordHeader) + elCount * sizeof(VirtualCDRecord);
		*pBytesDone = sizeof(VirtualCDRecordHeader);
		return STATUS_SUCCESS;
	}

	size_t off = sizeof(VirtualCDRecordHeader) + elCount * sizeof(VirtualCDRecord);
	unsigned idx = 0;

	for (_MappingType::iterator it = pMap->begin(); it != pMap->end(); it++)
	{
		if (idx >= elCount)
			break;

		char driveLetter = 0;

		WinCDEmuPDO *pSCSIPDO = static_cast<WinCDEmuPDO *>(FindPDOByUniqueID(it->second));
		if (pSCSIPDO)
		{
			ManagedPointer<VirtualCDDevice> pDev = pSCSIPDO->GetRelatedSCSIDevice();
			if (pDev)
				driveLetter = pDev->GetLastReportedDriveLetter();

			pSCSIPDO->QueryReferenceCounts(&pBuffer->Records[idx].DeviceRefCount, &pBuffer->Records[idx].HandleCount, &pBuffer->Records[idx].RefCount);
		}

		const DynamicString *pImageFile = &it->first;
		size_t stringSize = (pImageFile->length() + 1) * sizeof(wchar_t);

		if (!LetterOrImage.empty())
		{
			if (LetterOrImage.length() <= 2)
			{
				if ((LetterOrImage[0] | 0x20) != (driveLetter | 0x20))
					continue;
			}
			else
			{
				if (pImageFile->icompare(LetterOrImage))
					continue;
			}
		}

		if ((off + stringSize) <= OutputLength)
		{
			memcpy(((char *)pBuffer) + off, pImageFile->GetConstBuffer(), stringSize);
			pBuffer->Records[idx].ImageNameOffset = off;
			*pBytesDone = off + stringSize;
		}
		else
			pBuffer->Records[idx].ImageNameOffset = 0;

		off += stringSize;
		pBuffer->Records[idx].DriveLetter = driveLetter;
		idx++;
	}

	pBuffer->RecordCount = idx;
	pBuffer->TotalSize = off;
	*pBytesDone = off;
	return STATUS_SUCCESS;
}

ManagedPointer<VirtualCDDevice> BazisVirtualCDBus::FindDeviceByImageFilePath(const wchar_t *pImageFile)
{
	SynchronizedMapPointer pMap(*this);
	_MappingType::iterator it = pMap->find(pImageFile);
	if (it == pMap->end())
		return NULL;
	VirtualSCSIPDO *pPDO = static_cast<VirtualSCSIPDO *>(FindPDOByUniqueID(it->second));
	if (!pPDO)
		return NULL;
	return pPDO->GetRelatedSCSIDevice();
}

NTSTATUS BazisVirtualCDBus::OnDeviceControl(ULONG ControlCode, bool IsInternal, void *pInOutBuffer, ULONG InputLength, ULONG OutputLength, PULONG_PTR pBytesDone)
{
	if (ControlCode == IOCTL_QUERY_DRIVER_VERSION)
	{
		if (OutputLength < sizeof(unsigned))
			return STATUS_INVALID_BUFFER_SIZE;
		*((unsigned *)pInOutBuffer) = WINCDEMU_DRIVER_VERSION_CURRENT;
		*pBytesDone = sizeof(unsigned);
		return STATUS_SUCCESS;
	}

	wchar_t *pwszName = (wchar_t *)pInOutBuffer;
	bool bFound = false;
	//If NULL char is not found, our parameter is not a NULL-terminated string!
	for (unsigned i = 0; i < (InputLength / sizeof(wchar_t)); i++)
	{
		if (!pwszName[i])
		{
			bFound = true;
			break;
		}
	}
	if (!bFound && InputLength)
		return STATUS_INVALID_PARAMETER;

	switch (ControlCode)
	{
	case IOCTL_PLUGIN_DEVICE:
	case IOCTL_PLUGIN_DEVICE_RETURN_WAITHANDLE:
		{
			VirtualCDMountParams *pParams = (VirtualCDMountParams *)pInOutBuffer;
			if ((InputLength < sizeof(VirtualCDMountParams)) || !CheckStringLengthArray(pParams->wszFilePath))
				return STATUS_INVALID_PARAMETER;
			if (PDOExists(pParams->wszFilePath))
				return STATUS_OBJECT_NAME_COLLISION;
			if ((ControlCode == IOCTL_PLUGIN_DEVICE_RETURN_WAITHANDLE) && (OutputLength < sizeof(ULONGLONG)))
				return STATUS_INVALID_BUFFER_SIZE;

			ManagedPointer<VirtualCDDevice> pDev;
			NTSTATUS status = DoMountImage(pParams->wszFilePath, pParams->SuggestedDriveLetter, pParams->DisableAutorun, pParams->KeepAfterRestart, pParams->MMCProfile, &pDev);
			if (!NT_SUCCESS(status))
				return status;

			if (ControlCode == IOCTL_PLUGIN_DEVICE_RETURN_WAITHANDLE)
			{
				if (!pDev)
					return STATUS_INTERNAL_ERROR;
				HANDLE hEvent = (HANDLE)-1LL;
				ActionStatus st = pDev->OpenMountEventForCurrentProcess(&hEvent);
				//If handle opening failed, we return STATUS_SUCCESS, as the device was created, and set the handle to -1

				*((ULONGLONG *)pInOutBuffer) = (ULONGLONG)hEvent;
				*pBytesDone = sizeof(ULONGLONG);

				if (ControlCode != IOCTL_PLUGIN_DEVICE_RETURN_WAITHANDLE)
					if (hEvent != (HANDLE)-1LL)
						ZwClose(hEvent);
			}
			return STATUS_SUCCESS;
		}
	case IOCTL_PLUGOUT_DEVICE:
	case IOCTL_EJECT_DEVICE:
		if (!InputLength || !pwszName)
			return STATUS_INVALID_BUFFER_SIZE;
		return (ControlCode != IOCTL_EJECT_DEVICE) ? PlugoutPDO(pwszName) : EjectPDO(pwszName);
	case IOCTL_SOFT_PLUGOUT_DEVICE:
		if (!InputLength || !pwszName)
			return STATUS_INVALID_BUFFER_SIZE;
		{
			VirtualSCSIPDO *pPDO = static_cast<VirtualSCSIPDO *>(FindPDO(pwszName));
			if (!pPDO || !pPDO->ShouldUnplugPDOOnEject())
				return STATUS_INVALID_PARAMETER;
			return PlugoutPDO(pwszName);
		}
		return PlugoutPDO(pwszName);
	case IOCTL_IS_DEVICE_PRESENT:
		if (OutputLength < 1)
			return STATUS_INVALID_BUFFER_SIZE;
		*((bool *)pInOutBuffer) = PDOExists(pwszName);
		*pBytesDone = 1;
		return STATUS_SUCCESS;
	case IOCTL_LIST_DEVICES:
		{
			DynamicString letterOrImageFilter = InputLength ? pwszName : _T("");
			if (OutputLength < sizeof(VirtualCDRecordHeader))
				return STATUS_INVALID_BUFFER_SIZE;
			return ReportDeviceList((VirtualCDRecordHeader *)pInOutBuffer, letterOrImageFilter, OutputLength, pBytesDone);
		}
#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
	case IOCTL_SET_DEBUG_LOG_DIR:
		if (InputLength == 0)
			m_DebugLogDir = L"";
		else
			m_DebugLogDir = pwszName;
		return STATUS_SUCCESS;
#endif
	default:
		return STATUS_INVALID_DEVICE_REQUEST;
	}
}

NTSTATUS BazisVirtualCDBus::OnStartDevice(IN PCM_RESOURCE_LIST AllocatedResources, IN PCM_RESOURCE_LIST AllocatedResourcesTranslated)
{
	NTSTATUS st = __super::OnStartDevice(AllocatedResources, AllocatedResourcesTranslated);
	if (!NT_SUCCESS(st))
		return st;

	String regPath = GetDriver()->GetRegistryPath() + L"\\Parameters";
	RegistryKey key(regPath.c_str());
	if (key[L"GrantAccessToEveryone"])
		m_AccessSetter.Run(GetInterfaceName());

	int tmp = key[L"SufficientTestUnitReadyRequestsToCleanup"];
	if (tmp)
		m_SufficientTestUnitReadyRequestsToCleanup = tmp;
	tmp = key[L"CleanupTimeout"];
	if (tmp)
		m_CleanupTimeout = tmp;

	regPath += L"\\PersistentImages";
	m_pDatabase->SetRegistryPath(regPath);

	return STATUS_SUCCESS;
}

NTSTATUS BazisVirtualCDBus::DoMountImage(const wchar_t *pwszImage, char SuggestedDriveLetter, bool DisableAutorun, bool Persistent, unsigned MMCProfile, OUT ManagedPointer<VirtualCDDevice> *ppDev, const String &existingDBKey, bool cleanupExistingLinks)
{
	ActionStatus st = MAKE_STATUS(NoMemory);

	BazisLib::FileMode mode = FileModes::OpenReadOnly.ShareAll();
	mode.ObjectAttributes |= OBJ_FORCE_ACCESS_CHECK;
	ManagedPointer<AIFile> pFile = new ACFile(pwszImage, mode, &st);
	if (!pFile || !pFile->Valid())
		return st.ConvertToNTStatus();

	ManagedPointer<ImageFormats::AIParsedCDImage> pImage = m_ImageFormatDatabase.OpenCDImage(pFile, pwszImage);
	if (!pImage)
		return STATUS_BAD_FILE_TYPE;

	INT_PTR uniqueID = m_Pool.AllocateID();

	String dbKey;
	if (!existingDBKey.empty())
		dbKey = existingDBKey;
	else if (Persistent)
		dbKey = m_pDatabase->AddImage(pwszImage, SuggestedDriveLetter, MMCProfile);

	String regPath = GetDriver()->GetRegistryPath() + L"\\Parameters";
	RegistryKey key(regPath.c_str());
	MMCDeviceType devType = (MMCDeviceType)(int)key[L"MMCDeviceType"];
	if (devType == dtDefault)
		devType = dtMaximum;

	ManagedPointer<VirtualCDDevice> pDev = new VirtualCDDevice(pImage, uniqueID, DisableAutorun, m_pDatabase, dbKey, TempStrPointerWrapper(pwszImage), MMCProfile, devType, this);
	if (!pDev)
	{
		m_Pool.ReleaseID(uniqueID);
		return STATUS_NO_MEMORY;
	}

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
	ManagedPointer<AIFile> pLogFile;
	if (!m_DebugLogDir.empty())
	{
		for (int i = 0; i < 0x10000; i++)
		{
			FilePath fp = String::sFormat(L"%s\\vcd%05x.log", m_DebugLogDir.c_str(), i);
			if (File::Exists(fp.c_str()))
				continue;
			pDev->AttachLogFile(fp);
			break;
		}
	}
#endif

	const wchar_t *pDevName = wcsrchr(pwszImage, '\\');
	if (!pDevName)
		pDevName = pwszImage;
	else
		pDevName++;

#ifdef WINCDEMU_USE_LONG_DEVICEIDS
#error This option may cause incompatibility with several versions of Windows
	String devId = pDevName;
	for (int i = 0; i < devId.size(); i++)
		if (!wcschr(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0124567890_", devId[i]))
			devId[i] = '_';
#else
	String devId;
#ifdef WINCDEMU_PORTABLE
	devId.Format(L"PortableVirtualCD_%04X", (unsigned)uniqueID);
	pDevName = L"Portable WinCDEmu drive";
#else
	devId.Format(L"VirtualCD_%04X", (unsigned)uniqueID);
	pDevName = L"WinCDEmu drive";
#endif
#endif

	RegisterNewDeviceLink(devId, SuggestedDriveLetter, cleanupExistingLinks);

	if (ppDev)
		*ppDev = pDev;
	bool allowImmediateUnplug = (MMCProfile == 0);
	return InsertPDO(pwszImage, new WinCDEmuPDO(pDev, pDevName, devId.c_str(), allowImmediateUnplug));
}

void BazisVirtualCDBus::OnDeviceStarted()
{
	std::list<PersistentImageDatabase::ImageRecord> images = m_pDatabase->LoadImageList();
	bool cleanupOldLinks = true;
	for each(const PersistentImageDatabase::ImageRecord &img in images)
	{
		NTSTATUS status = DoMountImage(img.ImagePath.c_str(), img.DriveLetter, true, true, img.MMCProfile, NULL, img.Key, cleanupOldLinks);
		cleanupOldLinks = false;
		m_pDatabase->ReportMountStatus(img.Key, status);
	}
}

int BazisVirtualCDBus::VirtualDriveCleanupThread()
{
	std::list<String> devicesToDelete;
	while (m_bTerminating.WaitWithTimeoutEx(1000000) != STATUS_WAIT_0)
	{
		{
			SynchronizedMapPointer pMap(*this);

			for (_MappingType::iterator it = pMap->begin(); it != pMap->end(); it++)
			{
				VirtualSCSIPDO *pPDO = static_cast<VirtualSCSIPDO *>(FindPDOByUniqueID(it->second));
				if (!pPDO)
					continue;
				ManagedPointer<VirtualCDDevice> pDev = pPDO->GetRelatedSCSIDevice();
				if (pDev->IsUnplugPending(m_SufficientTestUnitReadyRequestsToCleanup, m_CleanupTimeout))
					devicesToDelete.push_back(it->first);
			}
		}

		if (!devicesToDelete.empty())
		{
			for each(String str in devicesToDelete)
				PlugoutPDO(str);
			devicesToDelete.clear();
		}
	}
	return 0;
}

void BazisVirtualCDBus::OnImagePathUpdated( VirtualCDDevice* pDevice, BazisLib::String &oldPath, const BazisLib::String & newPath )
{
	if (!oldPath.empty() && newPath.empty())
		return;

	bool found = ReplacePDOKey(oldPath, newPath);
	ASSERT(found);
}
