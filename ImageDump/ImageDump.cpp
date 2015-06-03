// ImageDump.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../ImageFormats/ImageFormats.h"
#include "cdstruct.h"
#include <vector>

using namespace BazisLib;
using namespace ImageFormats;

template <size_t arrSize> DynamicStringA ISOField(UCHAR (&arr)[arrSize])
{
	ConstStringA str((char *)arr, arrSize);
	off_t start = str.find_first_not_of(' ');
	off_t end = str.find_last_not_of(' ');
	if (end == -1)
		end = str.length();
	else
		end++;
	return str.substr(start, end - start);
}

const char CDUdfId[] = "BEA01";

static inline unsigned SwapByteOrder(unsigned long &val)
{
	return ((val & 0xFF000000) >> 24) | ((val & 0x00FF0000) >> 8) | ((val & 0x0000FF00) << 8) | ((val & 0x000000FF) << 24);
}

static inline short SwapByteOrder(unsigned short &val)
{
	return ((val & 0xFF00) >> 8) | ((val & 0x00FF) << 8);
}

//ISO9660 structures store several fields twice: in MSB and LSB formats.
//DUAL_ENDIAN_VALID() is a shortcut to check whether two such values match each other
//It can be used to verify volume descriptor integrity.
#define DUAL_ENDIAN_VALID(ptr,base) (ptr->base ## I == SwapByteOrder(ptr->base ## M))

bool CheckCDHeaders(const ConstManagedPointer<AIParsedCDImage> &pImage, bool dump, bool *pWarnings = NULL)
{
	if (!pImage)
		return false;
	char VolumeDescriptor[CD_SECTOR_SIZE];
	if (pImage->ReadSectorsBoundsChecked(FIRST_VD_SECTOR, VolumeDescriptor, 1) != CD_SECTOR_SIZE)
		return false;

	if (!memcmp(((PRAW_ISO_VD)VolumeDescriptor)->StandardId, CdIsoId, sizeof(CdIsoId) - 1))
	{
		PRAW_ISO_VD pVD = (PRAW_ISO_VD)VolumeDescriptor;
		if (!DUAL_ENDIAN_VALID(pVD,VolSpace) ||
			!DUAL_ENDIAN_VALID(pVD,VolSetSize) ||
			//!DUAL_ENDIAN_VALID(pVD,VolSeqNum)  ||
			!DUAL_ENDIAN_VALID(pVD,LogicalBlkSz) ||
			!DUAL_ENDIAN_VALID(pVD,PathTableSz))
		{
			if (pWarnings)
				*pWarnings = true;
		}

		unsigned lastSector = pImage->GetTrackEndSector(0);
		if (pVD->VolSpaceI > (lastSector + 1))
			if (pWarnings)
				*pWarnings = true;

		if (dump)
		{
			printf("Image type: ISO\r\n");
			printf("Volume ID: %s\r\nSystem ID: %s\r\n", ISOField(pVD->VolumeId).c_str(), ISOField(pVD->SystemId).c_str());
			printf("Creator: %s\r\nDate: %s\r\n", ISOField(pVD->AppId).c_str(), ISOField(pVD->CreateDate).c_str());
		}
		return true;
	}
	else if (!memcmp(((PRAW_HSG_VD)VolumeDescriptor)->StandardId, CdHsgId, sizeof(CdHsgId) - 1))
	{
		if (dump)
		{
			PRAW_HSG_VD pVD = (PRAW_HSG_VD)VolumeDescriptor;
			if (!DUAL_ENDIAN_VALID(pVD,VolSpace) ||
				!DUAL_ENDIAN_VALID(pVD,VolSetSize) ||
				!DUAL_ENDIAN_VALID(pVD,VolSeqNum)  ||
				!DUAL_ENDIAN_VALID(pVD,LogicalBlkSz) ||
				!DUAL_ENDIAN_VALID(pVD,PathTableSz))
				return false;
			printf("Image type: HSG\r\n");
		}
		return true;
	}
	else if (!memcmp(&VolumeDescriptor[1], CDUdfId, sizeof(CDUdfId) - 1))
	{
		if (dump)
		{
			printf("Image type: UDF\r\n");
		}
		return true;
	}
	else
		return false;
}

void DoFindImagesWithValidNames(ImageFormatDatabase *pDatabase, const FilePath &dirName, std::vector<FilePath> *pResult)
{
	Directory dir(dirName);
	for (ManagedPointer<AIDirectoryIterator> pIter = dir.FindFirstFile(); pIter && pIter->Valid(); pIter->FindNextFile())
	{
		if (pIter->IsAPseudoEntry())
			continue;
		if (pIter->IsDirectory())
			DoFindImagesWithValidNames(pDatabase, pIter->GetFullPath(), pResult);
		else
		{
			FilePath fp = pIter->GetFullPath();
			if (pDatabase->IsAValidCDImageName(fp))
				pResult->push_back(fp);
		}
	}
}

std::vector<FilePath> FindImagesWithValidNames(ImageFormatDatabase *pDatabase, const FilePath &dir)
{
	std::vector<FilePath> result;
	DoFindImagesWithValidNames(pDatabase, dir, &result);
	return result;
}

bool VerifyCDImage(const FilePath &fp, ImageFormatDatabase &formats, bool dump, bool *pWarnings = NULL)
{
	ManagedPointer<AIFile> pFile = new OSFile(fp, FastFileFlags::OpenReadOnlyShareAll);
	if (!pFile->Valid())
	{
		if (dump)
			printf("Cannot open image file!\n");
		return false;
	}

	ManagedPointer<AIParsedCDImage> pImage = formats.OpenCDImage(pFile, fp);
	if (!pImage)
	{
		if (dump)
			printf("Cannot parse image file format!\n");
		return false;
	}

	if (!CheckCDHeaders(pImage, dump, pWarnings))
	{
		if (dump)
			printf("Image seems inconsistent. Maybe, a parser bug.");
		return false;
	}
	return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2)
	{
		printf("Usage: ImageDump <image file>\n");
		printf("       ImageDump <directory>\\* [<log file>]\n");
		return 1;
	}
	ImageFormatDatabase formats;

	TCHAR *p = _tcschr(argv[1], '*');

	ManagedPointer<AIFile> pLog = (argc > 2) ? new OSFile(argv[2], FastFileFlags::CreateOrTruncateRW) : NULL;
	DynamicStringA logLine;
	if (pLog)
	{
		logLine = "Unmountable image list:\r\n";
		pLog->Write(logLine.c_str(), logLine.length());
	}

	if (p)
	{
		DynamicString dirName(argv[1], p - argv[1]);
		printf("Scanning %S ...\n", dirName.c_str());
		std::vector<FilePath> images = FindImagesWithValidNames(&formats, dirName);
		printf("Found %d CD/DVD images. Running checks...\n", images.size());

		std::vector<FilePath> badImages;
		int done = 0;

		for each(const FilePath &fp in images)
		{
			bool warnings = false;
			if (!VerifyCDImage(fp, formats, false, &warnings))
			{
				badImages.push_back(fp);
				if (pLog)
				{
					logLine.Format("%S\r\n", fp.c_str());
					pLog->Write(logLine.c_str(), logLine.length());
				}
			}
			if (warnings && pLog)
			{
				logLine.Format("Warning: %S format detected with some minor inconsistencies\r\n", fp.c_str());
				pLog->Write(logLine.c_str(), logLine.length());
			}
			printf("Checked %d/%d images. Bad images: %d.               \r", done++, images.size(), badImages.size());
		}

		if (pLog)
		{
			logLine.Format("------------------------------------\r\nTotal: %d unmountable images\r\n", badImages.size());
			pLog->Write(logLine.c_str(), logLine.length());
		}

	}
	else
	{
		FilePath fp = argv[1];
		VerifyCDImage(fp, formats, true);
	}

	return 0;
}
