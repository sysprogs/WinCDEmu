#pragma once
#include "../ImageHelpers.h"

namespace ImageFormats
{
#pragma pack(push, 1)
	struct NRGCueBlock
	{
		UCHAR AdrCtl;
		UCHAR Track;
		UCHAR Index;
		UCHAR unused;
		union
		{
			unsigned StartSector;
			MSFAddress MSF;
		};
	};

	C_ASSERT(sizeof(NRGCueBlock) == 8);

	struct NRGDAOHeader
	{
		unsigned unused1;
		UCHAR mcn[13];
		UCHAR unused2;
		UCHAR SessionType; /* ? */
		UCHAR NumSessions; /* ? */
		UCHAR FirstTrack;
		UCHAR LastTrack;
	};

	C_ASSERT(sizeof(NRGDAOHeader) == 22);

	template<typename _OffsetType> struct _NRGDAOEntry
	{
		UCHAR ISRC[12];
		USHORT SectorSize;
		UCHAR ModeCode;
		UCHAR unused[3];
		/* The following fields are 32-bit in old format and 64-bit in new format */
		_OffsetType PregapOffset; /* Pregap offset in file */
		_OffsetType StartOffset; /* Track start offset in file */
		_OffsetType EndOffset; /* Track end offset */
	};

	C_ASSERT(sizeof(_NRGDAOEntry<ULONGLONG>) == 42);
	C_ASSERT(sizeof(_NRGDAOEntry<unsigned>) == 30);

	template <typename _OffsetType> struct _NRGETNEntry
	{
		_OffsetType Offset;
		_OffsetType Size;
		UCHAR unused1[3];
		UCHAR Mode;
		unsigned Sector;
		_OffsetType unused2;
	};

	C_ASSERT(sizeof(_NRGETNEntry<ULONGLONG>) == 32);
	C_ASSERT(sizeof(_NRGETNEntry<unsigned>) == 20);

#pragma pack(pop)
}