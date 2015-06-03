#pragma once
#include "../ImageHelpers.h"

namespace ImageFormats
{
#pragma pack(push, 1)

	struct MDSHeader
	{
		UCHAR Signature[16]; /* "MEDIA DESCRIPTOR" */
		UCHAR Version[2]; /* Version ? */
		USHORT MediumType; /* Medium type */
		USHORT SessionCount; /* Number of sessions */
		USHORT unused1[2];
		USHORT BCALength; /* Length of BCA data (DVD-ROM) */
		unsigned unused2[2];
		unsigned BCAOffset; /* Offset to BCA data (DVD-ROM) */
		unsigned unused3[6]; /* Probably more offsets */
		unsigned DiskStructuresOffset; /* Offset to disc structures */
		unsigned unused4[3]; /* Probably more offsets */
		unsigned SessionsBlocksOffset; /* Offset to session blocks */
		unsigned DpmBlocksOffset; /* offset to DPM data blocks */
		unsigned EncryptionKeyOffset;
	}; /* length: 92 bytes */

	C_ASSERT(sizeof(MDSHeader) == 92);

	struct MDSSessionBlock
	{
		int SessionStart; /* Session's start address */
		int SessionEnd; /* Session's end address */
		USHORT SessionNumber; /* (Unknown) */
		UCHAR TotalBlockCount; /* Number of all data blocks. */
		UCHAR LeadInBlockCount; /* Number of lead-in data blocks */
		USHORT FirstTrack; /* Total number of sessions in image? */
		USHORT LastTrack; /* Number of regular track data blocks. */
		unsigned unused; /* (unknown) */
		unsigned TrackBlocksOffset; /* Offset of lead-in+regular track data blocks. */
	}; /* length: 24 bytes */

	C_ASSERT(sizeof(MDSSessionBlock) == 24);

	struct MDSTrackBlock 
	{
		UCHAR Mode; /* Track mode */
		UCHAR SubchannelMode; /* Subchannel mode */
		UCHAR AdrCtk; /* Adr/Ctl */
		UCHAR unused1; /* Track flags? */
		UCHAR TrackNumber; /* Track number. (>0x99 is lead-in track) */

		unsigned unused2;
		MSFAddress MSF;
		unsigned ExtraOffset; /* Start offset of this track's extra block. */
		USHORT SectorSize; /* Sector size. */

		UCHAR unused3[18];
		unsigned StartSector; /* Track start sector (PLBA). */
		ULONGLONG StartOffset; /* Track start offset. */
		UCHAR Session; /* Session or index? */
		UCHAR unused4[3];
		unsigned FooterOffset; /* Start offset of footer. */
		UCHAR unused5[24];
	}; /* length: 80 bytes */

	C_ASSERT(sizeof(MDSTrackBlock) == 80);

	struct MDSTrackExtraBlock
	{
		unsigned Pregap; /* Number of sectors in pregap. */
		unsigned Length; /* Number of sectors in track. */
	}; /* length: 8 bytes */

	C_ASSERT(sizeof(MDSTrackExtraBlock) == 8);

	struct MDSFooter
	{
		unsigned FilenameOffset; /* Start offset of image filename. */
		unsigned WidecharFilename; /* Seems to be set to 1 if widechar filename is used */
		unsigned unused1;
		unsigned unused2;
	}; /* length: 16 bytes */

	C_ASSERT(sizeof(MDSFooter) == 16);

#pragma pack(pop)
}