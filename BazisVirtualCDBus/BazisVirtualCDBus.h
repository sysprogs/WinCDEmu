#pragma once
#include <bzshlp/WinKernel/scsi/VirtualSCSIPDO.h>
#include <bzshlp/uidpool.h>
#include <bzscore/WinKernel/security.h>
#include <bzscore/thread.h>
#include "../ImageFormats/ImageFormats.h"
#include "DeviceControl.h"
#include "VirtualCDDevice.h"
#include "PersistentImageDatabase.h"


using namespace BazisLib;
using namespace DDK;
using namespace BazisLib::DDK::Security;

class BazisVirtualCDBus : public _GenericIndexedBus<VirtualSCSIPDO, String, _CaseInsensitiveLess<String>>
{
private:
	UniqueIdPool m_Pool;
	ManagedPointer<PersistentImageDatabase> m_pDatabase;
	String m_DebugLogDir;
	MemberThread m_VirtualDriveCleanupThread;
	KEvent m_bTerminating;

	unsigned m_SufficientTestUnitReadyRequestsToCleanup;
	unsigned m_CleanupTimeout;

private:
	void RegisterNewDeviceLink(const String &deviceId, char SuggestedDriveLetter, bool cleanupOldLinks = true);
	NTSTATUS ReportDeviceList(VirtualCDRecordHeader *pBuffer, const DynamicString &LetterOrImage, ULONG OutputLength, PULONG_PTR pBytesDone);

	int VirtualDriveCleanupThread();

private:
	ManagedPointer<VirtualCDDevice> FindDeviceByImageFilePath(const wchar_t *pImageFile);
	NTSTATUS DoMountImage(const wchar_t *pwszImage, char SuggestedDriveLetter, bool DisableAutorun, bool Persistent, unsigned MMCProfile, OUT ManagedPointer<VirtualCDDevice> *ppDev = NULL, const String &existingDBKey = L"", bool cleanupExistingLinks = true);

public:
	ImageFormats::ImageFormatDatabase m_ImageFormatDatabase;

	BazisVirtualCDBus()
		: _GenericIndexedBus(L"BazisVirtualCDBus", false, FILE_DEVICE_SECURE_OPEN, FALSE, DO_BUFFERED_IO)
		, m_Pool(32)
		, m_VirtualDriveCleanupThread(this, &BazisVirtualCDBus::VirtualDriveCleanupThread)
		, m_bTerminating(NotificationEvent, false)
		, m_SufficientTestUnitReadyRequestsToCleanup(2)
		, m_CleanupTimeout(3000)

	{ 
		m_pDatabase = new PersistentImageDatabase();
		CreateDeviceRequestQueue();
		m_VirtualDriveCleanupThread.Start();
	}

	~BazisVirtualCDBus()
	{
		m_bTerminating.Set();
		m_VirtualDriveCleanupThread.Join();
	}

	virtual NTSTATUS OnDeviceControl(ULONG ControlCode, bool IsInternal, void *pInOutBuffer, ULONG InputLength, ULONG OutputLength, PULONG_PTR pBytesDone) override;
	void OnImagePathUpdated( VirtualCDDevice* pDevice, BazisLib::String &oldPath, const BazisLib::String & newPath );

protected:
	virtual void ShutdownPDO(VirtualSCSIPDO *pPDO)
	{
		if (pPDO)
		{
			ManagedPointer<VirtualCDDevice> pDev = pPDO->GetRelatedSCSIDevice();
			if (pDev)
				m_Pool.ReleaseID(pDev->GetUniqueID());
			pPDO->ShutdownUnderlyingDevice();
		}
	}

private:

	//Interface link access cannot be set until the device startup is complete.
	//As a workaround, we start a separate thread to apply security.
	class AccessSetter
	{
	private:
		String m_DeviceInterfaceName;
		MemberThread m_Thread;
		bool m_bStop;
		
		int ThreadBody()
		{
			TranslatedAcl ACL;
			ACL.AddAllowingAce(FILE_ALL_ACCESS, Sid::Everyone());

			while (!m_bStop)
			{
				if (NT_SUCCESS(Device::sApplyDACL(&ACL, m_DeviceInterfaceName.c_str())))
					return 0;
				BazisLib::Thread::Sleep(100);
			}
			return 1;
		}

	public:
		AccessSetter()
			: m_Thread(this, &AccessSetter::ThreadBody)
			, m_bStop(false)
		{
		}

		~AccessSetter()
		{
			m_bStop = true;
			m_Thread.Join();
		}

		void Run(PCUNICODE_STRING pLinkName)
		{
			if (m_Thread.GetThreadID())
			{
				m_bStop = true;
				m_Thread.Join();
				m_Thread.Reset();
			}
			m_DeviceInterfaceName = pLinkName;
			m_Thread.Start();
		}

	};

	AccessSetter m_AccessSetter;

	virtual NTSTATUS OnStartDevice(IN PCM_RESOURCE_LIST AllocatedResources, IN PCM_RESOURCE_LIST AllocatedResourcesTranslated) override;
	virtual void OnDeviceStarted() override;

protected:
	virtual NTSTATUS DispatchRoutine(IN IncomingIrp *Irp, IO_STACK_LOCATION *IrpSp) override
	{
		switch (IrpSp->MajorFunction)
		{
		case IRP_MJ_PNP:
			if (IrpSp->MinorFunction != IRP_MN_START_DEVICE)
				break;
			if (!Irp->IsFromQueue())
				return EnqueuePacket(Irp);
			break;
		}
		return __super::DispatchRoutine(Irp, IrpSp);
	}
};

