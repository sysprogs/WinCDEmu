#include "stdafx.h"
#include "IMGParser.h"
#include "RawCDFormats.h"

using namespace ImageFormats;
using namespace BazisLib;

ProbeResult IMGParser::Probe( const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath, bool SecondPass )
{
	if (!SecondPass)
	{
		if (!FileExtension.icompare(_T("bin")) || !FileExtension.icompare(_T("img")) || !FileExtension.icompare(_T("iso")))
			return NameOnlyMatch;
		else
			return ProbableMatch;
	}
	else
	{
		ASSERT(pFile);
		RawCDFormatDescriptor desc = {0,};
		if (DetectRawCDFormat(pFile, &desc))
			return ProbableMatch;
		else
			return NoMatch;
	}
}

ManagedPointer<AIParsedCDImage> ImageFormats::IMGParser::OpenImage( const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath )
{
	RawCDFormatDescriptor desc = {0,};
	if (!DetectRawCDFormat(pFile, &desc))
		return NULL;
	if (!desc.SectorSize)
		return NULL;
	return new IMGImage(pFile, &desc);
}