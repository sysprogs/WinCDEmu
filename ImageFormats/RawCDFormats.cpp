#include "stdafx.h"
#include "RawCDFormats.h"
#include <bzshlp/textfile.h>

using namespace BazisLib;

namespace
{

	//! Checks first 16 sectors for format compliance
	/*!
		\return This function returns the score for the probed combination. The score is 1 if one of
				checked criteria was met, 2 if both and 0 if none. The following criteria are checked:
		<ul>
			<li>There should be no non-zero bytes in data area of first 16 sectors</li>
			<li>There should be at least one non-zero byte in metadata area of first 16 sectors</li>
		</ul>
	*/
	unsigned ProbeSectorFormat(const char *pBuffer, unsigned BaseOffset, unsigned BaseSectorSize)
	{
		bool fail1 = false;
		//! Check whether no non-zero bytes reside in sector data areas
		for (int i = 0; i < 16; i++)
		{
			unsigned base = BaseOffset + i * BaseSectorSize;
			for (int j = 0; j < 2048; j++)
				if (pBuffer[base + j])
				{
					fail1 = true;
					break;
				}
			if (fail1)
				break;
		}
		bool fail2 = false;
		if (BaseSectorSize <= 2048)
			return !fail1 + 1;
		//! Check that at least one non-zero byte resides in each sector's metadata section
		for (int i = 0; i < 16; i++)
		{
			bool fail = true;
			unsigned base = BaseOffset + i * BaseSectorSize;
			for (unsigned j = 2048; j < BaseSectorSize; j++)
				if (pBuffer[base + j])
				{
					fail = false;
					break;
				}
			if (fail)
			{
				fail2 = true;
				break;
			}
		}
		return !fail1 + !fail2;
	}

	unsigned FindFirstBigZeroArea(const char *pData, unsigned DataSize, unsigned MinZeroAreaSize)
	{
		unsigned firstZero = -1;
		bool found = false;
		for (unsigned i = 0; i < DataSize; i++)
		{
			if (!pData[i])
			{
				if (firstZero == -1)
					firstZero = i;
				else if ((i - firstZero) >= MinZeroAreaSize)
				{
					found = true;
					break;
				}
			}
			else
				firstZero = -1;
		}
		return found ? firstZero : -1;
	}
}

#define FIRST_VD_SECTOR             16

static unsigned PossibleSectorSizes[] = {2048, 2056, 2336, 2352};
static unsigned PossibleBaseOffsets[] = {0, 8, 16, 24};	//MUST go in ascending order

bool ScanVolumeHeaderForSignature(char *pData, size_t dataSize, RawCDFormatDescriptor *pDesc)
{
	char *Signatures[] = {"CD001", "CDROM", "BEA01"};	//Change 'len' below, if signatures with other sizes are added
	unsigned SignatureOffsets[] = {1, 9, 1};

	unsigned firstMatchingOff = 0;

	for (int s = 0; s < __countof(Signatures); s++)
	{
		size_t len = 5;
		char *pSign = Signatures[s];
		for (unsigned j = (2048 * FIRST_VD_SECTOR); j < (dataSize - len); j++)
		{
			if ((pData[j] == pSign[0]) && !memcmp(pData + j, pSign, len))
			{
				int firstSectorOffset = j - SignatureOffsets[s];
				if (!firstMatchingOff)
					firstMatchingOff = firstSectorOffset;
				for each (unsigned sectorSize in PossibleSectorSizes)
					for each (unsigned baseOff in PossibleBaseOffsets)
					{
						if (firstSectorOffset == ((sectorSize * FIRST_VD_SECTOR) + baseOff))
						{
							pDesc->InitialOffset = baseOff;
							pDesc->SectorSize = sectorSize;
							return true;
						}
					}
			}

		}
	}

	if (firstMatchingOff)
	{
		pDesc->SectorSize = (firstMatchingOff / FIRST_VD_SECTOR) & ~1;
		pDesc->InitialOffset = firstMatchingOff % pDesc->SectorSize;
		return true;
	}

	return false;
}

bool DetectRawCDFormat(const BazisLib::ConstManagedPointer<BazisLib::AIFile> &pFile, RawCDFormatDescriptor *pDesc)
{
	const unsigned SCANNED_FILE_AREA = FIRST_VD_SECTOR * 3000;//2352;
	//Area of consequent zeroes treated as 'first big zero area'
	const unsigned MIN_ZERO_AREA = (2352 - 2048) + 10;

	if (!pDesc)
		return false;
	if (!pFile || !pFile->Valid() || (pFile->GetSize() < SCANNED_FILE_AREA))
		return false;

	ActionStatus status;
	pFile->Seek(0, FileFlags::FileBegin, &status);
	if (!status.Successful())
		return false;

	char *pData = new char[SCANNED_FILE_AREA];
	if (!pData)
		return false;
	pFile->Seek(0, FileFlags::FileBegin);
	if (pFile->Read(pData, SCANNED_FILE_AREA) != SCANNED_FILE_AREA)
		return false;

	if (ScanVolumeHeaderForSignature(pData, SCANNED_FILE_AREA, pDesc))
	{
		delete pData;
		return true;
	}

	unsigned firstBigZero = FindFirstBigZeroArea(pData, SCANNED_FILE_AREA, MIN_ZERO_AREA);
	
	unsigned BaseOffset = -1;
	/* Base offset is basically the offset of first sector's first byte. As first 16 sectors of an
	   image are usually zeros, we can try detecting this offset by finding first 'big zero area'
	   (area of consecutive zeros).
	*/
	for (unsigned i = 0; i < sizeof(PossibleBaseOffsets) / sizeof(PossibleBaseOffsets[0]); i++)
		if (firstBigZero <= PossibleBaseOffsets[i])
		{
			BaseOffset = PossibleBaseOffsets[i];
			break;
		}

	unsigned TotalScore1Variants = 0, TotalScore2Variants = 0;
	unsigned MaxScore = 0;

	for (unsigned pass = 0; pass < 1; pass++)
	{
		for (unsigned i = 0; i < sizeof(PossibleSectorSizes) / sizeof(PossibleSectorSizes[0]); i++)
		{
			/* If we did not found the base offset (or found no non-zero bytes in the beginning),
			   we perform a slightly more complex detection sequence that involves probing all
			   starting offsets as well as sector sizes.
			*/

			if ((BaseOffset != -1) && BaseOffset)
			{
				//See ProbeSectorFormat description for details
				unsigned Score = ProbeSectorFormat(pData, BaseOffset, PossibleSectorSizes[i]);
				if (Score == 1)
					TotalScore1Variants++;
				else if (Score == 2)
					TotalScore2Variants++;

				if (Score > MaxScore)
				{
					MaxScore = Score;
					pDesc->InitialOffset = BaseOffset;
					pDesc->SectorSize = PossibleSectorSizes[i];
				}
			}
			else 
				for (unsigned j = 0; j < sizeof(PossibleBaseOffsets) / sizeof(PossibleBaseOffsets[0]); j++)
				{
					//See ProbeSectorFormat description for details
					unsigned Score = ProbeSectorFormat(pData, PossibleBaseOffsets[j], PossibleSectorSizes[i]);
					if (Score == 1)
						TotalScore1Variants++;
					else if (Score == 2)
						TotalScore2Variants++;

					if (Score > MaxScore)
					{
						MaxScore = Score;
						pDesc->InitialOffset = BaseOffset;
						pDesc->SectorSize = PossibleSectorSizes[i];
					}
				}
		}

		//If we've found the only best variant, end detection now
		if (!pass)
		{
			if (TotalScore2Variants == 1)
				break;
			MaxScore = TotalScore1Variants = TotalScore2Variants = 0;
		}
	}
	delete pData;

	if (TotalScore2Variants == 1)
		return true;
	if ((TotalScore1Variants == 1) && !TotalScore2Variants)
		return true;

	return false;
}

unsigned DetectDataOffsetInSector(const ConstManagedPointer<AIFile> &pFile, unsigned SectorSize, unsigned DefaultOffset, ULONGLONG FirstSectorOffset)
{
	if (!pFile || !pFile->Valid())
		return DefaultOffset;
	if (SectorSize == 2048)
		return DefaultOffset;
	char SectorHeader[16];
	pFile->Seek(FirstSectorOffset, FileFlags::FileBegin);
	if (pFile->Read(SectorHeader, sizeof(SectorHeader)) != sizeof(SectorHeader))
		return DefaultOffset;
	static const unsigned char s_RawSync[12] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
	if (memcmp(SectorHeader, s_RawSync, sizeof(s_RawSync)))
	{
		for (int i = 0; i < sizeof(SectorHeader); i++)
			if (SectorHeader[i])
				return DefaultOffset;
		return 0;	//Does not seem to be raw sector format
	}
	switch (SectorHeader[15])
	{
	case 1:
		return 16;	//Raw MODE1 sector
	case 2:
		return 24;	//Raw MODE2 sector
	default:
		return DefaultOffset;	//Unknown sector
	}
}