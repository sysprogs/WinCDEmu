#include "stdafx.h"
#include "UDFAnalyzer.h"
#include "Filesystems/miniudf.h"
#include <algorithm>

using namespace BazisLib;
using namespace ImageFormats;

#ifndef _DDK_

namespace
{
	class ParsedImageWrapper : public IDisc
	{
	private:
		ManagedPointer<AIParsedCDImage> m_pImage;
		unsigned m_FirstSector, m_TrackEnd;

	public:
		ParsedImageWrapper(ManagedPointer<AIParsedCDImage> pImg, unsigned track)
			: m_pImage(pImg)
			, m_FirstSector(0)
			, m_TrackEnd(0)
		{
			if (track > 0)
				m_FirstSector = pImg->GetTrackEndSector(track - 1);
			m_TrackEnd = pImg->GetTrackEndSector(track);
		}

		virtual bool ReadSector(unsigned sectorIndex, void *pBuffer, size_t sectorSize) override
		{
			if (sectorIndex >= (m_TrackEnd - m_FirstSector))
				return false;
			if (sectorSize != 2048)
				return false;
			return (m_pImage->ReadSectorsBoundsChecked(m_FirstSector + sectorIndex, pBuffer, 1) == 2048);
		}

	};
}

UDFAnalysisResult AnalyzeUDFImage(const BazisLib::ManagedPointer<ImageFormats::AIParsedCDImage> &pImg)
{
	std::auto_ptr<ParsedImageWrapper> pWrapper(new ParsedImageWrapper(pImg, 0));
	UDFDisc disc(pWrapper.get());

	UDFAnalysisResult result = {false,};

	if (!disc.ParseUDFHeaders())
		return result;

	UDFDisc::LogicalVolume *pVol = disc.FindLastVolume();
	if (!pVol)
		return result;
	
	result.isUDF = true;
	result.foundBDMV = disc.FindFileInDirectory(pVol, pVol->RootDirectory, L"BDMV", false).Valid();
	result.foundVIDEO_TS = disc.FindFileInDirectory(pVol, pVol->RootDirectory, L"VIDEO_TS", false).Valid();
	
	return result;
}

#endif