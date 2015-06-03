#pragma once

#include <bzshlp/WinKernel/scsi/VirtualSCSICdrom.h>
#include <bzscore/file.h>
#include "../ImageFormats/ImageFormats.h"
#include <bzscore/sync.h>
#include "PersistentImageDatabase.h"
#include <ntddmmc.h>
#include <bzshlp/WinKernel/WorkerThread.h>

#define SKIP_WINCDEMU_GUID_DEFINITION
#include "DeviceControl.h"

enum MMCDeviceType
{
	dtDefault = 0,
	dtNonMMC,
	dtCDROM,
	dtDVDROM,
	dtBDROM,
	dtMaximum
};

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
class LoggerWorkerThread : public BazisLib::DDK::WorkerThreadBase
{
private:
	BazisLib::File m_File;

	virtual void DispatchItem(void *pItem) override;

public:
	LoggerWorkerThread(BazisLib::FilePath &fp);
};
#endif

class VirtualCDDevice : public BazisLib::DDK::SCSI::VirtualSCSICdrom
{
private:
	DECLARE_REFERENCE(ImageFormats::AIParsedCDImage, m_pUnsynchronizedImagePtr);
	DECLARE_REFERENCE(PersistentImageDatabase, m_pDatabase);
	class BazisVirtualCDBus *m_pOwningBus;

	class RemountRequest : AUTO_OBJECT
	{
	public:
		DECLARE_REFERENCE(ImageFormats::AIParsedCDImage, m_pImage);
		HANDLE m_hDeletionEvent;
		BazisLib::String m_ImagePath;

	public:
		RemountRequest(const BazisLib::ConstManagedPointer<ImageFormats::AIParsedCDImage> &pImage, const BazisLib::String &imagePath);

		~RemountRequest()
		{
			ZwSetEvent(m_hDeletionEvent, NULL);
			ZwClose(m_hDeletionEvent);
		}

		HANDLE ExportEventHandleForCurrentProcess();

		BazisLib::ManagedPointer<ImageFormats::AIParsedCDImage> GetImage() {return m_pImage;}
		const BazisLib::String &GetImagePath() {return m_ImagePath;}
	};
	
	unsigned m_SectorCount;

	//BazisLib::InProcessMutex m_DataAccessMutex;

	BazisLib::DDK::FastMutex m_LinkNameMutex;
	BazisLib::String m_LastReportedLink, m_PersistentImgDBKey, m_ImagePath;
	char m_LastReportedDriveLetter;

	HANDLE m_hDeviceLinkCreatedEvent;
	bool m_bDisableAutorun;

	unsigned m_MMCProfile;
	MMCDeviceType m_DeviceType;
	INT_PTR m_UniqueID;
	bool m_SuppressUnusedDriveCleanup;

	unsigned m_TestUnitReadyCallsSinceUnmount;
	unsigned m_TestUnitReadyRequestsBeforeCompletingUnmount;
	BazisLib::DateTime m_DisposeTime;

	BazisLib::FastMutex m_UnmountStateMutex;
	DECLARE_REFERENCE(RemountRequest, m_pPendingRemountRequest);

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
	LoggerWorkerThread *m_pWorkerThread;
#endif

public:
	VirtualCDDevice(
		const BazisLib::ConstManagedPointer<ImageFormats::AIParsedCDImage> &pImage,
		INT_PTR UniqueID, 
		bool bDisableAutorun, 
		const BazisLib::ConstManagedPointer<PersistentImageDatabase> &pDatabase, 
		const BazisLib::TempString &PersistentImgDBKey,
		const BazisLib::TempString &ImagePath,
		unsigned MMCProfile,
		MMCDeviceType deviceType,
		class BazisVirtualCDBus *pOwningBus);
	~VirtualCDDevice();

#ifdef WINCDEMU_DEBUG_LOGGING_SUPPORT
	void AttachLogFile(BazisLib::FilePath fp)
	{
		if (m_pWorkerThread)
			return;

		m_pWorkerThread = new LoggerWorkerThread(fp);
		m_pWorkerThread->Start();
	}
#endif

	virtual size_t GetIDString(IDStringType IDType, char *pBuf, size_t MaxSize) override;

	BazisLib::String GetLastReportedLink()
	{
		BazisLib::FastMutexLocker lck(m_LinkNameMutex);
		return m_LastReportedLink;
	}

	BazisLib::ActionStatus OpenMountEventForCurrentProcess(HANDLE *pHandle);

	char GetLastReportedDriveLetter()
	{
		return m_LastReportedDriveLetter;
	}

	INT_PTR GetUniqueID()
	{
		return m_UniqueID;
	}

public:
	virtual BazisLib::ActionStatus GetInquiryData(BazisLib::TypedBuffer<BazisLib::SCSI::InquiryData> &pInquiryData) override;


	virtual ULONGLONG GetSectorCount() override;
	virtual unsigned Read(ULONGLONG ByteOffset, void *pBuffer, unsigned Length) override;

	virtual unsigned GetTrackCount() override;
	virtual unsigned GetTrackEndSector(unsigned TrackIndex) override;

	virtual bool DeviceControl(unsigned CtlCode, void *pBuffer, unsigned InSize, unsigned OutSize, unsigned *pBytesDone) override;

	virtual void Dispose() override
	{
		m_DisposeTime = BazisLib::DateTime::Now();
		if (!m_PersistentImgDBKey.empty() && m_pDatabase)
			m_pDatabase->RemoveImageByKey(m_PersistentImgDBKey);

		//BazisLib::MutexLocker lck(m_DataAccessMutex);
		m_pUnsynchronizedImagePtr = NULL;
	}

	virtual BazisLib::SCSI::SCSIResult OnGetConfiguration(const _CDB::_GET_CONFIGURATION *pRequest, PVOID pData, size_t Size, size_t *pDone);
	virtual BazisLib::SCSI::SCSIResult OnReadDiscInformation(const _CDB::_READ_DISK_INFORMATION *pRequest, PVOID pData, size_t Size, size_t *pDone);
	virtual BazisLib::SCSI::SCSIResult OnModeSense10(const _CDB::_MODE_SENSE10 *pRequest, PVOID pData, size_t Size, size_t *pDone);

	virtual BazisLib::SCSI::SCSIResult ExecuteSCSIRequest(BazisLib::SCSI::GenericSRB &request) override;

	BazisLib::SCSI::SCSIResult DoExecuteSCSIRequest(BazisLib::SCSI::GenericSRB &request);

	bool IsUnplugPending(unsigned sufficientTestUnitReadyIterations, unsigned sufficientTimeout);

	bool CanExecuteAsynchronously(const BazisLib::SCSI::GenericSRB &request);


private:
	enum {kMaxFeatureDataSize = 256, kMaxFeatureNumber = FeatureVCPS, kMaxModePage = MODE_PAGE_CAPABILITIES};
	
	//Returns -1 if the feature is not available, returns 0 if no additional information is returned except the header
	size_t GetFeatureDescriptor(FEATURE_NUMBER Feature, PFEATURE_HEADER pHeader, PVOID pData, size_t maxSize);
	
	//Return value similar to GetFeatureDescriptor()
	size_t GetProfileList(PFEATURE_HEADER pHeader, PVOID pData, size_t maxSize);

	FEATURE_PROFILE_TYPE GetCurrentProfile();

	#pragma pack(push, 1)
	struct MODE_PAGE_HEADER {
		UCHAR PageCode : 6;
		UCHAR Reserved : 1;
		UCHAR PageSavable : 1;
		UCHAR PageLength;
	};
	#pragma pack(pop)

	//Returns -1 if the feature is not available, returns 0 if no additional information is returned except the header
	size_t GetModePage(int ModePage, MODE_PAGE_HEADER *pHeader, PVOID pData, size_t maxSize);
	void AttachDiscImage(const BazisLib::ConstManagedPointer<ImageFormats::AIParsedCDImage> & pImage, const BazisLib::String &imagePath, bool attachImmediately, OUT HANDLE *pAttachCompleteEvent = NULL);
};