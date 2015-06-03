#pragma once

#include <bzscore/autofile.h>

struct RawCDFormatDescriptor
{
	unsigned SectorSize;
	unsigned InitialOffset;
};

bool DetectRawCDFormat(const BazisLib::ConstManagedPointer<BazisLib::AIFile> &pFile, RawCDFormatDescriptor *pDesc);
BazisLib::ActionStatus ParseCUEFile(const BazisLib::String &OriginalPath, BazisLib::String *pBinaryPath, const BazisLib::ManagedPointer<BazisLib::AIFile> &pFile, RawCDFormatDescriptor *pDesc);

unsigned DetectDataOffsetInSector(const BazisLib::ConstManagedPointer<BazisLib::AIFile> &pFile, unsigned SectorSize, unsigned DefaultOffset, ULONGLONG FirstSectorOffset = 0);
