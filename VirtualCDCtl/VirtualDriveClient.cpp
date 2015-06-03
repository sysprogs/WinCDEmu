#include "stdafx.h"
#include "VirtualDriveClient.h"

#define SKIP_WINCDEMU_GUID_DEFINITION
#include "../BazisVirtualCDBus/DeviceControl.h"

using namespace BazisLib;

VirtualDriveClient::VirtualDriveClient(const BazisLib::String &drivePath, ActionStatus *pStatus) 
	: m_Drive(drivePath, BazisLib::FileModes::OpenReadOnly.ShareAll(), pStatus)
{
	if (!m_Drive.Valid())
		return;
	char sz[1024];
	PSTORAGE_DEVICE_DESCRIPTOR pDesc = (PSTORAGE_DEVICE_DESCRIPTOR)sz;
	STORAGE_PROPERTY_QUERY query;
	query.QueryType = PropertyStandardQuery;
	query.PropertyId = StorageDeviceProperty;
	size_t done = m_Drive.DeviceIoControl(IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), sz, sizeof(sz), pStatus);
	if ((done < sizeof(STORAGE_DEVICE_DESCRIPTOR)) || (pDesc->BusType != 14))
	{
		ASSIGN_STATUS(pStatus, NotSupported);
		m_Drive.Close();
		return;
	}

	char *pProduct = sz + pDesc->ProductIdOffset;
	if ((pDesc->ProductIdOffset >= sizeof(sz)) || memcmp(pProduct, "WinCDEmu", 9))
	{
		ASSIGN_STATUS(pStatus, NotSupported);
		m_Drive.Close();
		return;
	}

	GUIDAndVersion *pGV = (GUIDAndVersion *)(pDesc + 1);
	if ((pDesc->RawPropertiesLength < sizeof(GUIDAndVersion)) || (pGV->Guid != GUID_WinCDEmuDrive))
	{
		ASSIGN_STATUS(pStatus, NotSupported);
		m_Drive.Close();
		return;
	}

	ASSIGN_STATUS(pStatus, Success);
}

bool VirtualDriveClient::IsAutorunSuppressionFlagged()
{
	if (!m_Drive.Valid())
		return false;
	char bDisabled = 0;
	size_t done = m_Drive.DeviceIoControl(IOCTL_VDEV_IS_AUTORUN_DISABLED, NULL, 0, &bDisabled, 1);
	return (done == 1) && (bDisabled == 1);
}

BazisLib::String VirtualDriveClient::GetNativeMountedImagePath()
{
	unsigned size = 0;
	if (m_Drive.DeviceIoControl(IOCTL_VDEV_GET_IMAGE_PATH, 0, NULL, &size, sizeof(size), NULL) != sizeof(size))
		return BazisLib::String();
	size_t todo = size + sizeof(size);
	char *pBuf = new char[todo];
	if (m_Drive.DeviceIoControl(IOCTL_VDEV_GET_IMAGE_PATH, 0, NULL, pBuf, (DWORD)todo, NULL) != todo)
	{
		delete pBuf;
		return BazisLib::String();
	}
	wchar_t *pStr = (wchar_t *)(pBuf + sizeof(size));
	if (pStr[(size / sizeof(wchar_t)) - 1])
	{
		delete pBuf;
		return BazisLib::String();
	}
	BazisLib::String fp = pStr;
	delete pBuf;
	return fp;
}

BazisLib::ActionStatus VirtualDriveClient::MountNewImage(const wchar_t *pwszImage, unsigned timeout)
{
	ActionStatus status;
	ULONGLONG hWait = 0;
	m_Drive.DeviceIoControl(IOCTL_VDEV_MOUNT_IMAGE, pwszImage, pwszImage ? ((wcslen(pwszImage) + 1) * sizeof(wchar_t)) : 0, &hWait, timeout ? sizeof(hWait) : 0, &status);
	if (!status.Successful())
		return status;

	if (timeout && hWait)
	{
		if (WaitForSingleObject(reinterpret_cast<HANDLE>(hWait), timeout) != WAIT_OBJECT_0)
			return MAKE_STATUS(TimeoutExpired);
	}
	return status;
}