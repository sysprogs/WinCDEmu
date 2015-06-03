#pragma once
#include <bzscore/file.h>

class VirtualDriveClient
{
private:
	BazisLib::File m_Drive;

public:
	VirtualDriveClient(const BazisLib::String &drivePath, BazisLib::ActionStatus *pStatus);
	bool IsAutorunSuppressionFlagged();
	BazisLib::String GetNativeMountedImagePath();
	BazisLib::ActionStatus MountNewImage(const wchar_t *pwszImage, unsigned timeout);
};