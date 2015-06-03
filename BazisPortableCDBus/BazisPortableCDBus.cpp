#include "stdafx.h"
#include <bzshlp/WinKernel/driver.h>
#include <bzshlp/WinKernel/DeviceEnumerator.h>
#define SKIP_WINCDEMU_GUID_DEFINITION
#include "../BazisVirtualCDBus/BazisVirtualCDBus.h"

extern "C" PDEVICE_OBJECT __stdcall IoGetLowerDeviceObject(IN PDEVICE_OBJECT  DeviceObject); 

using namespace BazisLib;
using namespace BazisLib::DDK;

typedef struct _OBJECT_HEADER
{
	LONG PointerCount;
	union
	{
		LONG HandleCount;
		PVOID NextToFree;
	};
	POBJECT_TYPE Type;
	UCHAR NameInfoOffset;
	UCHAR HandleInfoOffset;
	UCHAR QuotaInfoOffset;
	UCHAR Flags;
	PVOID Unused;
	PVOID SecurityDescriptor;
	QUAD Body;
} OBJECT_HEADER, *POBJECT_HEADER;

class PciFilterDevice : BazisVirtualCDBus
{
public:
	PciFilterDevice()
	{ 
	}

	virtual NTSTATUS DispatchRoutine(IN IncomingIrp *Irp, IO_STACK_LOCATION *IrpSp) override
	{
		if (IrpSp->MajorFunction == IRP_MJ_PNP)
			return __super::DispatchRoutine(Irp, IrpSp);
		else
			return CallNextDriverSynchronously(Irp);
	}

	NTSTATUS Attach(Driver *pDriver, PDEVICE_OBJECT PhysicalDeviceObject)
	{
		NTSTATUS st = AddDevice(pDriver, PhysicalDeviceObject);
		if (!NT_SUCCESS(st))
			return st;

		SetNewPNPState(Started);
		return STATUS_SUCCESS;
	}

	void Detach()
	{
		DetachDevice();
		SetNewPNPState(Stopped);
	}

	virtual NTSTATUS OnDeviceControl(ULONG ControlCode, bool IsInternal, void *pInOutBuffer, ULONG InputLength, ULONG OutputLength, PULONG_PTR pBytesDone) override
	{
		return __super::OnDeviceControl(ControlCode, IsInternal, pInOutBuffer, InputLength, OutputLength, pBytesDone);
	}


};

class BazisPortableCDBus : public Device
{
private:
	PciFilterDevice *m_pFilterDevice;

public:
	BazisPortableCDBus()
		: Device(FILE_DEVICE_BUS_EXTENDER, L"BazisPortableCDBus")
		, m_pFilterDevice(NULL)
	{
	}

	~BazisPortableCDBus()
	{
	}

	NTSTATUS OnDeviceControl(ULONG ControlCode, bool IsInternal, void *pInOutBuffer, ULONG InputLength, ULONG OutputLength, PULONG_PTR pBytesDone) 
	{
		if (m_pFilterDevice)
			return m_pFilterDevice->OnDeviceControl(ControlCode, IsInternal, pInOutBuffer, InputLength, OutputLength, pBytesDone);
		else
			return STATUS_INTERNAL_ERROR;
	}

	void AttachPCIFilterDevice(PciFilterDevice *pDevice)
	{
		m_pFilterDevice = pDevice;
	}

	virtual NTSTATUS DispatchRoutine(IN IncomingIrp *Irp, IO_STACK_LOCATION *IrpSp)
	{
		NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

		if (IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
		{
			switch (METHOD_FROM_CTL_CODE(IrpSp->Parameters.DeviceIoControl.IoControlCode))
			{
			case METHOD_BUFFERED:
				status = OnDeviceControl(IrpSp->Parameters.DeviceIoControl.IoControlCode,
					(IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL),
					Irp->GetSystemBuffer(),
					IrpSp->Parameters.DeviceIoControl.InputBufferLength,
					IrpSp->Parameters.DeviceIoControl.OutputBufferLength,
					&Irp->GetIoStatus()->Information);
				break;
			default:
				return __super::DispatchRoutine(Irp, IrpSp);
			}
		}
		else
			return __super::DispatchRoutine(Irp, IrpSp);

		Irp->SetIoStatus(status);
		Irp->CompleteRequest();
		return status;
	}

};

extern "C" POBJECT_TYPE ObGetObjectType(PVOID Object);

PDEVICE_OBJECT GetLowestDeviceObject(PDEVICE_OBJECT pDevObj)
{
	PDEVICE_OBJECT pTmp = pDevObj;
	while (pTmp)
	{
		PDEVICE_OBJECT pLower = IoGetLowerDeviceObject(pTmp);
		if (!pLower)
			return pTmp;
		if (pTmp != pDevObj)
			ObDereferenceObject(pTmp);
		pTmp = pLower;
	}
	return NULL;
}

PVOID g_pDriverType;

class BazisPortableCDBusDriver : public Driver
{
private:
	BazisPortableCDBus *pDevice;
	PciFilterDevice *pFilter;

public:
	BazisPortableCDBusDriver() : DDK::Driver(false)
	{
		pDevice = new BazisPortableCDBus();
		pFilter = NULL;
	}

	virtual NTSTATUS DriverLoad(IN PUNICODE_STRING RegistryPath) override
	{
		DbgPrint("BazisPortableCDBus: Starting BazisPortableCDBus...\r\n");
		DbgPrint("BazisPortableCDBus: Locating ACPI and PCI driver objects...\r\n");
		DriverObjectFinder drvACPI(L"acpi");
		DeviceEnumerator drvPCI(L"pci");

		if (!drvACPI.Valid() || !drvPCI.Valid())
			return STATUS_DEVICE_NOT_READY;

		PDEVICE_OBJECT pPciBusPDO = NULL;

		DbgPrint("BazisPortableCDBus: Searching for PCI bus PDO...\r\n");

		for (size_t i = 0; i < drvPCI.DeviceCount(); i++)
		{
			PDEVICE_OBJECT pDevObj = drvPCI.GetDeviceByIndex(i);
			PDEVICE_OBJECT pLower = IoGetLowerDeviceObject(pDevObj);
			if (!pLower)
				continue;
			if (pLower->DriverObject == drvACPI.GetDriverObject())
			{
				PDEVICE_OBJECT pPreLower = IoGetLowerDeviceObject(pLower);
				if (!pPreLower)
					pPciBusPDO = pLower;
				else
					ObDereferenceObject(pPreLower);
			}
			ObDereferenceObject(pLower);
		}

		if (!pPciBusPDO)
		{
			size_t FDOCount = 0, potentialFDOCount = 0;
			DbgPrint("BazisPortableCDBus: Standard lookup found nothing.\r\nPerforming advanced PDO lookup...\r\nChecking all %d device objects owned by PCI driver...\r\n", drvPCI.DeviceCount());
			for (size_t i = 0; i < drvPCI.DeviceCount(); i++)
			{
				PDEVICE_OBJECT pDevObj = drvPCI.GetDeviceByIndex(i);
				PDEVICE_OBJECT pLower = IoGetLowerDeviceObject(pDevObj);
				if (!pLower)
					continue;
				potentialFDOCount++;
				PDEVICE_OBJECT pPreLower = GetLowestDeviceObject(pLower);
				if (pPreLower)
				{
					if (pPreLower->DriverObject == drvACPI.GetDriverObject())
					{
						FDOCount++;
						pPciBusPDO = pPreLower;
					}
					ObDereferenceObject(pPreLower);
				}

				ObDereferenceObject(pLower);
			}
			DbgPrint("BazisPortableCDBus: Found %d PCI FDO(s); %d potential FDOs.\r\n", FDOCount, potentialFDOCount);
			if (!pPciBusPDO)
				return STATUS_NO_SUCH_DEVICE;
		}

		DbgPrint("BazisPortableCDBus: Found the PCI PDO (%X)...\r\n", pPciBusPDO);

		DbgPrint("BazisPortableCDBus: Creating \\Device\\BazisPortableCDBus...\r\n");
		NTSTATUS status = __super::DriverLoad(RegistryPath);
		status = pDevice->RegisterDevice(this, true, L"\\DosDevices\\BazisPortableCDBus");
		if (!NT_SUCCESS(status))
			return status;

		pFilter = NULL;
		pFilter = new PciFilterDevice();
		status = pFilter->Attach(this, pPciBusPDO);
		if (!NT_SUCCESS(status))
			return status;

		DbgPrint("BazisPortableCDBus: Hooking PCI bus driver...\r\n");
		pDevice->AttachPCIFilterDevice(pFilter);

		return STATUS_SUCCESS;
	}

	virtual ~BazisPortableCDBusDriver()
	{
		DbgPrint("BazisPortableCDBus: Unloading BazisPortableCDBus...\r\n");
		pDevice->AttachPCIFilterDevice(NULL);

		if (pFilter)
		{
			pFilter->Detach();
			delete pFilter;
		}
		delete pDevice;
	}
};

DDK::Driver *_stdcall CreateMainDriverInstance()
{
	return new BazisPortableCDBusDriver();
}
