#include "stdafx.h"
#include "VirtualCDDevice.h"
#include <bzscore/WinKernel/security.h>
#include <bzshlp/WinKernel/storage.h>

#define SKIP_WINCDEMU_GUID_DEFINITION
#include "DeviceControl.h"
#include "BazisVirtualCDBus.h"

using namespace BazisLib;

extern "C" 
{
	NTSTATUS NTAPI
	ZwCreateEvent(
				  OUT PHANDLE  EventHandle,
				  IN ACCESS_MASK  DesiredAccess,
				  IN POBJECT_ATTRIBUTES  ObjectAttributes OPTIONAL,
				  IN EVENT_TYPE  EventType,
				  IN BOOLEAN  InitialState
				  );

	NTSTATUS NTAPI
		ZwSetEvent(
		IN HANDLE  EventHandle,
		OUT PLONG  PreviousState OPTIONAL
		);


	NTSTATUS NTAPI
		ZwClearEvent(
		IN HANDLE  EventHandle);

	NTSTATUS
		NTAPI
		ZwDuplicateObject(
		__in HANDLE SourceProcessHandle,
		__in HANDLE SourceHandle,
		__in_opt HANDLE TargetProcessHandle,
		__out_opt PHANDLE TargetHandle,
		__in ACCESS_MASK DesiredAccess,
		__in ULONG HandleAttributes,
		__in ULONG Options
		);
}



VirtualCDDevice::VirtualCDDevice( const BazisLib::ConstManagedPointer<ImageFormats::AIParsedCDImage> &pImage, INT_PTR UniqueID, bool bDisableAutorun, const BazisLib::ConstManagedPointer<PersistentImageDatabase> &pDatabase, const BazisLib::TempString &PersistentImgDBKey, const BazisLib::TempString &ImagePath, unsigned MMCProfile, MMCDeviceType deviceType, class BazisVirtualCDBus *pOwningBus)
	: ZERO_REFERENCE(m_pUnsynchronizedImagePtr)
	, INIT_REFERENCE(m_pDatabase, pDatabase)
	, ZERO_REFERENCE(m_pPendingRemountRequest)
#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
	, m_pWorkerThread(NULL)
#endif
	, m_SectorCount(0)
	, m_LastReportedDriveLetter(0)
	, m_UniqueID(UniqueID)
	, m_hDeviceLinkCreatedEvent(0)
	, m_bDisableAutorun(bDisableAutorun)
	, m_PersistentImgDBKey(PersistentImgDBKey)
	, m_MMCProfile(MMCProfile)
	, m_DeviceType(deviceType)
	, m_TestUnitReadyCallsSinceUnmount(0)
	, m_TestUnitReadyRequestsBeforeCompletingUnmount(2)
	, m_SuppressUnusedDriveCleanup(false)
	, m_pOwningBus(pOwningBus)
{
	OBJECT_ATTRIBUTES attr;
	InitializeObjectAttributes(&attr, NULL, OBJ_KERNEL_HANDLE, 0, NULL);
	ZwCreateEvent(&m_hDeviceLinkCreatedEvent, EVENT_ALL_ACCESS, &attr, NotificationEvent, FALSE);
	AttachDiscImage(pImage, ImagePath, true);
}

VirtualCDDevice::~VirtualCDDevice()
{
	if (m_hDeviceLinkCreatedEvent)
	{
		ZwClearEvent(m_hDeviceLinkCreatedEvent);
		ZwClose(m_hDeviceLinkCreatedEvent);
	}

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
	if (m_pWorkerThread)
	{
		m_pWorkerThread->Shutdown();
		delete m_pWorkerThread;
	}
#endif
}

ULONGLONG VirtualCDDevice::GetSectorCount()
{
	return m_SectorCount;
}

unsigned VirtualCDDevice::Read( ULONGLONG ByteOffset, void *pBuffer, unsigned Length )
{
//Read/write requests are always serviced via the worker thread, so, no need for a mutex.
//	MutexLocker lck(m_DataAccessMutex);
	BazisLib::ManagedPointer<ImageFormats::AIParsedCDImage> pImage(m_pUnsynchronizedImagePtr);
	if (!pImage)
		return 0;
	unsigned firstSector = (unsigned)(ByteOffset / ImageFormats::CD_SECTOR_SIZE);
	if (firstSector > m_SectorCount)
		return 0;

	unsigned sectorCount = Length / ImageFormats::CD_SECTOR_SIZE;

	if (sectorCount > (m_SectorCount - firstSector))
		sectorCount = m_SectorCount - firstSector;

	return pImage->ReadSectorsBoundsChecked(firstSector, pBuffer, sectorCount);
}

unsigned VirtualCDDevice::GetTrackCount()
{
//	MutexLocker lck(m_DataAccessMutex);
	BazisLib::ManagedPointer<ImageFormats::AIParsedCDImage> pImage(m_pUnsynchronizedImagePtr);
	if (!pImage)
		return 0;
	return pImage->GetTrackCount();
}

unsigned VirtualCDDevice::GetTrackEndSector( unsigned TrackIndex )
{
//	MutexLocker lck(m_DataAccessMutex);
	BazisLib::ManagedPointer<ImageFormats::AIParsedCDImage> pImage(m_pUnsynchronizedImagePtr);
	if (!pImage)
		return 0;
	return pImage->GetTrackEndSector(TrackIndex);
}

bool VirtualCDDevice::DeviceControl( unsigned CtlCode, void *pBuffer, unsigned InSize, unsigned OutSize, unsigned *pBytesDone )
{
	if ((CtlCode & 0xFFFF3FFF) == (IOCTL_MOUNTDEV_LINK_CREATED & 0xFFFF3FFF))
	{
		PMOUNTDEV_NAME pName = (PMOUNTDEV_NAME)pBuffer;
		if (!pName || !pName->NameLength || (pName->NameLength > (InSize + 2)))
			return false;

		String link(pName->Name, pName->NameLength / sizeof(wchar_t));
		if (!link.substr(0, 12).icompare(L"\\DosDevices\\") && (link.length() >= 13) && (link[12] != '.'))
		{
			FastMutexLocker lck(m_LinkNameMutex);
			m_LastReportedLink = link;
			m_LastReportedDriveLetter = m_LastReportedLink[12];
		}
		ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
		ZwSetEvent(m_hDeviceLinkCreatedEvent, NULL);
		return true;
	}
	else if ((CtlCode & 0xFFFF3FFF) == (IOCTL_MOUNTDEV_LINK_DELETED & 0xFFFF3FFF))
	{
		ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
		ZwClearEvent(m_hDeviceLinkCreatedEvent);
		FastMutexLocker lck(m_LinkNameMutex);
		m_LastReportedLink.clear();
		m_LastReportedDriveLetter = 0;
		return true;
	}
	else if (CtlCode == IOCTL_VDEV_IS_AUTORUN_DISABLED)
	{
		if ((OutSize < 1) || !pBuffer)
			return false;
		((char *)pBuffer)[0] = m_bDisableAutorun;
		*pBytesDone = 1;
		return true;
	}
	else if (CtlCode == IOCTL_VDEV_GET_IMAGE_PATH)
	{
		unsigned byteCount = m_ImagePath.SizeInBytes(true);
		memcpy(pBuffer, &byteCount, min(sizeof(byteCount), OutSize));
		if (OutSize <= sizeof(byteCount))
		{
			*pBytesDone = OutSize;
			return true;
		}

		char *pOutStr = (char *)pBuffer + sizeof(byteCount);
		unsigned avail = OutSize - sizeof(byteCount);

		memcpy(pOutStr, m_ImagePath.GetConstBuffer(), min(avail, byteCount));
		*pBytesDone = sizeof(byteCount) + min(avail, byteCount);

		return true;
	}
	else if (CtlCode == IOCTL_VDEV_GET_MMC_PROFILE)
	{
		if ((OutSize < 4) || !pBuffer)
			return false;
		((unsigned *)pBuffer)[0] = m_MMCProfile;
		*pBytesDone = 4;
		return true;
	}
	else if (CtlCode == IOCTL_VDEV_SET_MMC_PROFILE)
	{
		if ((InSize  < 4) || !pBuffer)
			return false;
		m_MMCProfile = ((unsigned *)pBuffer)[0];
		*pBytesDone = 4;
		return true;
	}
	else if (CtlCode == IOCTL_VDEV_MOUNT_IMAGE)
	{
		const wchar_t *pFN = (wchar_t *) pBuffer;
		if (InSize > 0 && (InSize < 2 || pFN[(InSize / 2) - 1] != 0))
			return false;

		HANDLE waitHandle = NULL;		
		bool handleRequested = (OutSize >= sizeof(waitHandle));

		if (InSize == 0 || !pFN[0])
			AttachDiscImage(ManagedPointer<ImageFormats::AIParsedCDImage>(), BazisLib::String(), false, handleRequested ? &waitHandle : NULL);
		else
		{
			ImageFormats::ImageFormatDatabase formatDB;

			ActionStatus st = MAKE_STATUS(NoMemory);

			BazisLib::FileMode mode = FileModes::OpenReadOnly.ShareAll();
			mode.ObjectAttributes |= OBJ_FORCE_ACCESS_CHECK;
			ManagedPointer<AIFile> pFile = new ACFile(pFN, mode, &st);
			if (!pFile || !pFile->Valid())
				return false;

			ManagedPointer<ImageFormats::AIParsedCDImage> pImage = formatDB.OpenCDImage(pFile, pFN);
			if (!pImage)
				return false;

			AttachDiscImage(pImage, pFN, false, handleRequested ? &waitHandle : NULL);
		}

		if (handleRequested)
		{
			memcpy(pBuffer, &waitHandle, sizeof(HANDLE));
			*pBytesDone = sizeof(HANDLE);
		}
				
		return true;
	}

	return false;
}



ActionStatus VirtualCDDevice::OpenMountEventForCurrentProcess(HANDLE *pHandle)
{
	if (!pHandle)
		return MAKE_STATUS(InvalidPointer);

	NTSTATUS status = ZwDuplicateObject(NtCurrentProcess(), m_hDeviceLinkCreatedEvent, NtCurrentProcess(), pHandle, SYNCHRONIZE, 0, 0);
	return MAKE_STATUS(ActionStatus::FromNTStatus(status));
}

static inline size_t CopyStr(char *pBuf, const char *pSrcBuf, size_t maxSize)
{
	size_t todo = min(strlen(pSrcBuf), maxSize);
	memcpy(pBuf, pSrcBuf, todo);
	return todo;
}

size_t VirtualCDDevice::GetIDString(IDStringType IDType, char *pBuf, size_t MaxSize)
{
	switch(IDType)
	{
	case BasicSCSIBlockDevice::VendorId:
		return CopyStr(pBuf, "Bazis", MaxSize);
	case BasicSCSIBlockDevice::ProductId:
		return CopyStr(pBuf, "WinCDEmu", MaxSize);
	default:
		return 0;
	}

}

using namespace BazisLib::DDK::SCSI;

#pragma pack(push, 1)
struct LogEntryHeader
{
	unsigned Signature1;
	SCSI_REQUEST_TYPE RequestType;
	UCHAR Status;
	LARGE_INTEGER StartTime, EndTime;
	CDB cdb;
	unsigned Requested, Done, Saved;
	unsigned Signature2;
};
#pragma  pack(pop)



SCSIResult VirtualCDDevice::ExecuteSCSIRequest(GenericSRB &request)
{
#ifndef WINCDEMU_DEBUG_LOGGING_SUPPORT
	return DoExecuteSCSIRequest(request);
#else
	if (!m_pWorkerThread)
		return DoExecuteSCSIRequest(RequestType, pDataBuffer, DataTransferSize, pCDB, pDataTransferred);

	CBuffer *pLogBuf = new CBuffer();

	bool skipData = false;
	if ((RequestType == SCSIOP_READ) || (RequestType == SCSIOP_WRITE))
		skipData = true;

	if (skipData)
		pLogBuf->EnsureSize(sizeof(LogEntryHeader));
	else if (DataTransferSize <= 131072)
		pLogBuf->EnsureSize(sizeof(LogEntryHeader) + 2 * DataTransferSize);
	else
		pLogBuf->EnsureSize(sizeof(LogEntryHeader) + DataTransferSize);

	LogEntryHeader *pHdr = (LogEntryHeader *)pLogBuf->GetData();
	pLogBuf->SetSize(sizeof(LogEntryHeader));

	if (!skipData && DataTransferSize)
		memcpy(pLogBuf->GetDataAfterEndOfUsedSpace(), pDataBuffer, DataTransferSize);

	pHdr->Signature1 = 0xCDEB5555;
	pHdr->RequestType = RequestType;
	pHdr->cdb = *pCDB;
	KeQuerySystemTime(&pHdr->StartTime);
	SCSI_STATUS status = DoExecuteSCSIRequest(RequestType, pDataBuffer, DataTransferSize, pCDB, pDataTransferred);
	KeQuerySystemTime(&pHdr->EndTime);
	pHdr->Requested = DataTransferSize;
	pHdr->Done = pDataTransferred ? *pDataTransferred : -1;
	pHdr->Saved = pHdr->Done;
	pHdr->Status = status;
	if (skipData)
		pHdr->Saved = 0;
	pHdr->Signature2 = 0xCDEBAAAA;

	if (!skipData && DataTransferSize)
	{
		pLogBuf->SetSize(sizeof(LogEntryHeader) + pHdr->Saved);
		pLogBuf->EnsureSize(sizeof(LogEntryHeader) + pHdr->Saved * 2);
		memcpy(pLogBuf->GetDataAfterEndOfUsedSpace(), pDataBuffer, DataTransferSize);
		pLogBuf->SetSize(sizeof(LogEntryHeader) + pHdr->Saved * 2);
	}

	m_pWorkerThread->EnqueueItem((CBuffer *)pLogBuf);
	return status;
#endif
}

SCSIResult VirtualCDDevice::DoExecuteSCSIRequest(GenericSRB &request)
{
	BazisLib::ManagedPointer<ImageFormats::AIParsedCDImage> pImage(m_pUnsynchronizedImagePtr);

	switch(request.RequestType)
	{
	case SCSIOP_TEST_UNIT_READY:
		if (!pImage)
		{
			BazisLib::FastMutexLocker lck(m_UnmountStateMutex);
			if (++m_TestUnitReadyCallsSinceUnmount >= m_TestUnitReadyRequestsBeforeCompletingUnmount)
			{
				if (m_pPendingRemountRequest)
				{
					AttachDiscImage(m_pPendingRemountRequest->GetImage(), m_pPendingRemountRequest->GetImagePath(), true);
					m_pPendingRemountRequest = NULL;
				}
			}
		}
		//no break
	case SCSIOP_READ:
	case SCSIOP_READ_TOC:
	case SCSIOP_READ_DISC_INFORMATION:
		if (!pImage)
			return scsiMediumNotPresent;
		break;
	case SCSIOP_LOAD_UNLOAD:
		Dispose();
		return scsiGood;
	}

	if (m_MMCProfile && (m_DeviceType != dtNonMMC))
	{
		if (request.RequestType == SCSIOP_GET_CONFIGURATION)
			return OnGetConfiguration((_CDB::_GET_CONFIGURATION *)request.pCDB, request.DataBuffer, request.DataTransferSize, &request.DataTransferSize);
		else if (request.RequestType == SCSIOP_READ_DISC_INFORMATION)
			return OnReadDiscInformation((_CDB::_READ_DISK_INFORMATION *)request.pCDB, request.DataBuffer, request.DataTransferSize, &request.DataTransferSize);
		else if (request.RequestType == SCSIOP_MODE_SENSE10)
			return OnModeSense10((_CDB::_MODE_SENSE10 *)request.pCDB, request.DataBuffer, request.DataTransferSize, &request.DataTransferSize);
	}

	return __super::ExecuteSCSIRequest(request);
}

FEATURE_PROFILE_TYPE VirtualCDDevice::GetCurrentProfile()
{
	return (FEATURE_PROFILE_TYPE)m_MMCProfile;
}

#pragma region GET CONFIGURATION implementation
SCSIResult VirtualCDDevice::OnGetConfiguration(const _CDB::_GET_CONFIGURATION *pRequest, PVOID pData, size_t Size, size_t *pDone)
{
	bool skipInactiveDescriptors = (pRequest->RequestType == 1);
	int startingFeature = (pRequest->StartingFeature[0] << 16) | pRequest->StartingFeature[1], endingFeature = kMaxFeatureNumber;

	if (pRequest->RequestType == 2)
		endingFeature = startingFeature;
	CBuffer featureBuffer;
	if (!featureBuffer.EnsureSize(sizeof(GET_CONFIGURATION_HEADER)))
		return scsiCommandTerminated;
	featureBuffer.SetSize(sizeof(GET_CONFIGURATION_HEADER));
	memset(featureBuffer.GetData(), 0, sizeof(GET_CONFIGURATION_HEADER));

	for (int feature = startingFeature; feature <= endingFeature; feature++)
	{
		if (!featureBuffer.EnsureSize(featureBuffer.GetSize() + kMaxFeatureDataSize + sizeof(FEATURE_HEADER)))
			return scsiCommandTerminated;

		PFEATURE_HEADER pHeader = (PFEATURE_HEADER)featureBuffer.GetDataAfterEndOfUsedSpace();
		PVOID pFeatureDataBuffer = pHeader + 1;

		memset(pHeader, 0, sizeof(FEATURE_HEADER));
		pHeader->FeatureCode[0] = (UCHAR)(feature >> 8);
		pHeader->FeatureCode[1] = (UCHAR)feature;
		
		size_t dataSize = GetFeatureDescriptor((FEATURE_NUMBER)feature, pHeader, pFeatureDataBuffer, featureBuffer.GetAllocated() - featureBuffer.GetSize() - sizeof(FEATURE_HEADER));
		if (dataSize == -1)
			continue;
		ASSERT(dataSize <= 0xFF);
		if (dataSize > 0xFF)
			dataSize = 0xFF;

		pHeader->AdditionalLength = (UCHAR)dataSize;
		featureBuffer.SetSize(featureBuffer.GetSize() + sizeof(FEATURE_HEADER) + dataSize);
	}

	GET_CONFIGURATION_HEADER *pHdr = (PGET_CONFIGURATION_HEADER)featureBuffer.GetData();
	FEATURE_PROFILE_TYPE curProfile = GetCurrentProfile();
	size_t dataLength = featureBuffer.GetSize() - sizeof(pHdr->DataLength);
	
	pHdr->DataLength[0] = (UCHAR)(dataLength >> 24);
	pHdr->DataLength[1] = (UCHAR)(dataLength >> 16);
	pHdr->DataLength[2] = (UCHAR)(dataLength >> 8);
	pHdr->DataLength[3] = (UCHAR)(dataLength >> 0);
	
	pHdr->CurrentProfile[0] = (UCHAR)(curProfile >> 8);
	pHdr->CurrentProfile[1] = (UCHAR)(curProfile >> 0);

	ULONG copySize = min(Size, featureBuffer.GetSize());
	*pDone = copySize;

	memcpy(pData, featureBuffer.GetData(), copySize);
	return scsiGood;
}

template <class _FeatureStruct> static inline size_t CopyFeatureData(bool current, bool persistent, const _FeatureStruct *pSource, PFEATURE_HEADER pHeader, PVOID pData, size_t maxSize, size_t version = 0)
{
	pHeader->Current = true;
	pHeader->Persistent = true;
	pHeader->Version = version;
	size_t todo = min(maxSize, sizeof(_FeatureStruct) - sizeof(FEATURE_HEADER));
	memcpy(pData, ((char *)pSource) + sizeof(FEATURE_HEADER), todo);
	return todo;
}

static inline bool IsCDProfile(FEATURE_PROFILE_TYPE profile)
{
	switch (profile)
	{
	case ProfileCdrom:
	case ProfileCdRecordable:
	case ProfileCdRewritable:
		return true;
	default:
		return false;
	}
}

static inline bool IsDVDProfile(FEATURE_PROFILE_TYPE profile)
{
	switch (profile)
	{
	case ProfileDvdRom:            
	case ProfileDvdRecordable:     
	case ProfileDvdRam:            
	case ProfileDvdRewritable:     
	case ProfileDvdRWSequential:   
	case ProfileDvdDashRDualLayer: 
	case ProfileDvdDashRLayerJump: 
	case ProfileDvdPlusRW:         
	case ProfileDvdPlusR:
		return true;
	default:
		return false;
	}
}

static inline bool IsBDProfile(FEATURE_PROFILE_TYPE profile)
{
	switch (profile)
	{
	case ProfileBDRom:
	case ProfileBDRSequentialWritable:
	case ProfileBDRRandomWritable:
	case ProfileBDRewritable:
		return true;
	default:
		return false;
	}
}

size_t VirtualCDDevice::GetFeatureDescriptor(FEATURE_NUMBER Feature, PFEATURE_HEADER pHeader, PVOID pData, size_t maxSize)
{
	static const FEATURE_DATA_CORE fdCore = {
		{0,},	//Header
		{0, 0, 0, 2}, //PhysicalInterface = 2 (ATAPI)
		TRUE,	//DeviceBusyEvent
	};

	static const FEATURE_DATA_MORPHING fdMorphing = {
		{0,},	//Header
		FALSE,	//Asynchronous
		TRUE,	//OCEvent
	};

	static const FEATURE_DATA_REMOVABLE_MEDIUM fdRemovable = {
		{0,},	//Header
		FALSE,	//Lockable
		FALSE,
		FALSE,	//Default to prevent
		TRUE,	//Eject
		FALSE,
		1,		//Tray type loading mechanism
	};

	static const FEATURE_DATA_CD_READ fdCDRead = {
		{0,},	//Header
		TRUE,	//CDText
		TRUE,	//C2ErrorData
		0,
		TRUE,	//Digital Audio Play
	};

	static const FEATURE_DATA_DVD_READ fdDVDRead = {
		{0,},	//Header
		TRUE,	//Multi110
		0,
		0,
		TRUE,	//Dual-R
	};

	static const FEATURE_BD_READ fdBDRead = {
		{0,},	//Header
	};

	switch (Feature)
	{
	case FeatureProfileList:
		return GetProfileList(pHeader, pData, maxSize);
	case FeatureCore:
		return CopyFeatureData(true, true, &fdCore, pHeader, pData, maxSize, 2);
	case FeatureMorphing:
		return CopyFeatureData(true, true, &fdMorphing, pHeader, pData, maxSize, 1);
	case FeatureRemovableMedium:
		return CopyFeatureData(true, true, &fdRemovable, pHeader, pData, maxSize, 1);
	case FeatureMultiRead:
		return 0;
	case FeatureCdRead:
		return CopyFeatureData(IsCDProfile(GetCurrentProfile()), false, &fdCDRead, pHeader, pData, maxSize, 2);
	case FeatureDvdRead:
		if (m_DeviceType < dtDVDROM)
			return -1;
		return CopyFeatureData(IsDVDProfile(GetCurrentProfile()), false, &fdDVDRead, pHeader, pData, maxSize, 2);
	case FeatureBDRead:
		if (m_DeviceType < dtBDROM)
			return -1;
		return CopyFeatureData(IsBDProfile(GetCurrentProfile()), false, &fdBDRead, pHeader, pData, maxSize, 0);
	}

	return -1;
}

size_t VirtualCDDevice::GetProfileList(PFEATURE_HEADER pHeader, PVOID pData, size_t maxSize)
{
	//static const int profilesReportedByLaptopDrive[] = {0x12, 0x11, 0x15, 0x14, 0x13, 0x1A, 0x1B, 0x2B, 0x10, 0x09, 0x0A, 0x08, 0x02};

	static const FEATURE_PROFILE_TYPE supportedProfiles[] = {
		ProfileBDRom,
		ProfileBDRSequentialWritable,
		ProfileBDRRandomWritable,
		ProfileBDRewritable,
		ProfileDvdPlusRW,
		ProfileDvdPlusR,
		ProfileDvdPlusRWDualLayer,
		ProfileDvdPlusRDualLayer,
		ProfileDvdRom,
		ProfileDvdRecordable,
		ProfileDvdRam,
		ProfileDvdRewritable,
		ProfileDvdRWSequential,
		ProfileHDDVDRom,
		ProfileHDDVDRecordable,
		ProfileHDDVDRam,
		ProfileHDDVDRewritable,
		ProfileHDDVDRDualLayer,
		ProfileHDDVDRWDualLayer,
		ProfileCdrom,
		ProfileCdRecordable,
		ProfileCdRewritable,
		ProfileRemovableDisk,
	};

	pHeader->Current = TRUE;
	pHeader->Persistent = TRUE;
	FEATURE_PROFILE_TYPE curProfile = GetCurrentProfile();
	
	C_ASSERT(kMaxFeatureDataSize >= (__countof(supportedProfiles) * sizeof(FEATURE_DATA_PROFILE_LIST_EX)));
	size_t requiredSize = __countof(supportedProfiles) * sizeof(FEATURE_DATA_PROFILE_LIST_EX);
	if (maxSize < requiredSize)
	{
		ASSERT(false);
		return -1;
	}

	memset(pData, 0, requiredSize);
	PFEATURE_DATA_PROFILE_LIST_EX pDescriptors = (PFEATURE_DATA_PROFILE_LIST_EX)pData;

	for (int i = 0; i < __countof(supportedProfiles); i++)
	{
		pDescriptors[i].ProfileNumber[0] = (UCHAR)(supportedProfiles[i] >> 8);
		pDescriptors[i].ProfileNumber[1] = (UCHAR)(supportedProfiles[i] >> 0);
		pDescriptors[i].Current = (curProfile == supportedProfiles[i]);
	}
	
	return requiredSize;
}

#pragma endregion

struct DISC_INFORMATION_BLOCK
{
	UCHAR Length[2];
	UCHAR DiscStatus : 2, StatusOfLastSession : 2, Erasable : 1, DiscInformationDataType : 3;
	UCHAR FirstTrackNumber;
	UCHAR NumberOfSessionsLSB;
	UCHAR FirstTrackInLastSessionLSB;
	UCHAR LastTrackInLastSessionLSB;
	UCHAR ExtStatusByte;
	UCHAR DiscType;
	UCHAR NumberOfSessionsMSB;
	UCHAR FirstTrackInLastSessionMSB;
	UCHAR LastTrackInLastSessionMSB;
	UCHAR DiscID[4];
	UCHAR LeadInStart[4];
	UCHAR LastPossibleLeadOutStart[4];
	UCHAR BarCode[8];
	UCHAR ApplicationCode;
	UCHAR Obsolete;
};

C_ASSERT(sizeof(DISC_INFORMATION_BLOCK) == 34);

SCSIResult VirtualCDDevice::OnReadDiscInformation(const _CDB::_READ_DISK_INFORMATION *pRequest, PVOID pData, size_t Size, size_t *pDone)
{
	DISC_INFORMATION_BLOCK blk = {0, };
	blk.Length[1] = sizeof(DISC_INFORMATION_BLOCK) - sizeof(blk.Length);
	blk.DiscStatus = 2;				//Complete disc
	blk.StatusOfLastSession = 3;	//Complete session
	blk.DiscInformationDataType = 0;
	blk.FirstTrackNumber = 1;
	blk.NumberOfSessionsLSB = 1;
	blk.LastTrackInLastSessionMSB = GetTrackCount();

	memset(blk.LeadInStart, 0xFF, sizeof(blk.LeadInStart));
	memset(blk.LastPossibleLeadOutStart, 0xFF, sizeof(blk.LastPossibleLeadOutStart));

	ULONG todo = min(Size, sizeof(blk));
	memcpy(pData, &blk, todo);
	*pDone = todo;

	return scsiGood;
}

SCSIResult VirtualCDDevice::OnModeSense10(const _CDB::_MODE_SENSE10 *pRequest, PVOID pData, size_t Size, size_t *pDone)
{
	unsigned first = pRequest->PageCode, last = pRequest->PageCode;
	if (pRequest->PageCode == MODE_SENSE_RETURN_ALL)
		first = 0, last = kMaxModePage;

	CBuffer modePageBuffer;
	if (!modePageBuffer.EnsureSize(sizeof(MODE_PARAMETER_HEADER10)))
		return scsiCommandTerminated;
	modePageBuffer.SetSize(sizeof(MODE_PARAMETER_HEADER10));
	memset(modePageBuffer.GetData(), 0, sizeof(MODE_PARAMETER_HEADER10));

	for (unsigned page = first; page <= last; page++)
	{
		if (!modePageBuffer.EnsureSize(modePageBuffer.GetSize() + kMaxFeatureDataSize + sizeof(MODE_PAGE_HEADER)))
			return scsiCommandTerminated;

		MODE_PAGE_HEADER *pHeader = (MODE_PAGE_HEADER *)modePageBuffer.GetDataAfterEndOfUsedSpace();
		PVOID pPageDataBuffer = pHeader + 1;

		memset(pHeader, 0, sizeof(FEATURE_HEADER));
		pHeader->PageCode = page;

		size_t dataSize = GetModePage(page, pHeader, pPageDataBuffer, modePageBuffer.GetAllocated() - modePageBuffer.GetSize() - sizeof(MODE_PAGE_HEADER));
		if (dataSize == -1)
			continue;

		ASSERT(dataSize <= 0xFF);
		if (dataSize > 0xFF)
			dataSize = 0xFF;

		pHeader->PageLength = (UCHAR)dataSize;
		modePageBuffer.SetSize(modePageBuffer.GetSize() + sizeof(MODE_PAGE_HEADER) + dataSize);
	}

	MODE_PARAMETER_HEADER10 *pHdr = (PMODE_PARAMETER_HEADER10)modePageBuffer.GetData();
	size_t dataLength = modePageBuffer.GetSize() - sizeof(pHdr->ModeDataLength);

	pHdr->ModeDataLength[0] = (UCHAR)(dataLength >> 8);
	pHdr->ModeDataLength[1] = (UCHAR)(dataLength >> 0);

	ULONG copySize = min(Size, modePageBuffer.GetSize());
	*pDone = copySize;

	memcpy(pData, modePageBuffer.GetData(), copySize);
	return scsiGood;
}

size_t VirtualCDDevice::GetModePage(int ModePage, MODE_PAGE_HEADER *pHeader, PVOID pData, size_t maxSize)
{
	static const unsigned char mpCDVD[] = {
		0x3B, 0x00, 0x71, 0xFF, 0x2D, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
	};

	switch (ModePage)
	{
	case MODE_PAGE_CAPABILITIES:
		pHeader->PageSavable = 0;
		maxSize = min(maxSize, sizeof(mpCDVD));
		memcpy(pData, mpCDVD, maxSize);
		return maxSize;
	}
	return -1;
}

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT

void LoggerWorkerThread::DispatchItem(void *pItem)
{
	CBuffer *pBuf = (CBuffer *)pItem;
	if (!pBuf)
		return;

	m_File.Write(pBuf->GetData(), pBuf->GetSize());
	delete pBuf;	
}

LoggerWorkerThread::LoggerWorkerThread(BazisLib::FilePath &fp)
	: m_File(fp, FastFileFlags::CreateOrTruncateRW)
{
}

#endif

bool VirtualCDDevice::IsUnplugPending(unsigned sufficientTestUnitReadyIterations, unsigned sufficientTimeout)
{
	if (m_pUnsynchronizedImagePtr || m_pPendingRemountRequest)
		return false;
	if (m_SuppressUnusedDriveCleanup)
		return false;
	if (m_TestUnitReadyCallsSinceUnmount >= sufficientTestUnitReadyIterations)
		return true;
	unsigned msec = (DateTime::Now() - m_DisposeTime).GetTotalMilliseconds();
	if (msec >= sufficientTimeout)
		return true;
	return false;
}

BazisLib::ActionStatus VirtualCDDevice::GetInquiryData( TypedBuffer<InquiryData> &pData )
{
	ActionStatus st = __super::GetInquiryData(pData);
	if (!st.Successful())
		return st;

	C_ASSERT(sizeof(pData->VendorSpecific) >= sizeof(GUIDAndVersion));
	GUIDAndVersion *pInf = (GUIDAndVersion *)pData->VendorSpecific;
	pInf->Guid = GUID_WinCDEmuDrive;
	pInf->Version = WINCDEMU_VER_INT;
	return MAKE_STATUS(Success);
}

bool VirtualCDDevice::CanExecuteAsynchronously( const GenericSRB &request )
{
	return __super::CanExecuteAsynchronously(request);
}

void VirtualCDDevice::AttachDiscImage(const BazisLib::ConstManagedPointer<ImageFormats::AIParsedCDImage> &pImage, const BazisLib::String &imagePath, bool attachImmediately, HANDLE *pAttachCompleteEvent)
{
	//The 'attach complete' event will get signalled once the pending attach request is deleted (that normally happens after it is serviced).
	if (pAttachCompleteEvent)
		*pAttachCompleteEvent = NULL;

	if (!attachImmediately)
	{
		BazisLib::ManagedPointer<ImageFormats::AIParsedCDImage> pOldImage(m_pUnsynchronizedImagePtr);
		 
		if (!pImage)
		{
			m_SuppressUnusedDriveCleanup = true;
		}

		{
			BazisLib::FastMutexLocker lck(m_UnmountStateMutex);
			m_TestUnitReadyCallsSinceUnmount = 0;

			m_pPendingRemountRequest = new RemountRequest(pImage, imagePath);
			if (pAttachCompleteEvent)
				*pAttachCompleteEvent = m_pPendingRemountRequest->ExportEventHandleForCurrentProcess();
		}

		m_pUnsynchronizedImagePtr = NULL;
	}
	else
	{
		m_SectorCount = pImage->GetSectorCount();
		m_pUnsynchronizedImagePtr = pImage;
		if (m_pOwningBus)
			m_pOwningBus->OnImagePathUpdated(this, m_ImagePath, imagePath);
		m_ImagePath = imagePath;
	}
}

HANDLE VirtualCDDevice::RemountRequest::ExportEventHandleForCurrentProcess()
{
	HANDLE hCopy = NULL;
	ZwDuplicateObject(NtCurrentProcess(), m_hDeletionEvent, NtCurrentProcess(), &hCopy, SYNCHRONIZE, 0, 0);
	return hCopy;
}

VirtualCDDevice::RemountRequest::RemountRequest( const BazisLib::ConstManagedPointer<ImageFormats::AIParsedCDImage> &pImage, const BazisLib::String &imagePath ) 
	: INIT_REFERENCE(m_pImage, pImage)
	, m_ImagePath(imagePath)
{
	OBJECT_ATTRIBUTES attr;
	InitializeObjectAttributes(&attr, NULL, OBJ_KERNEL_HANDLE, 0, NULL);
	ZwCreateEvent(&m_hDeletionEvent, EVENT_ALL_ACCESS, &attr, NotificationEvent, FALSE);
}
