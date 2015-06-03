#pragma once
#include "ImageFormats.h"
#include "RawCDFormats.h"

namespace ImageFormats
{
	class IMGImage : public ParsedCDImageBase
	{
	private:
		RawCDFormatDescriptor m_FormatDescriptor;

	public:
		IMGImage(const ConstManagedPointer<AIFile> &pFile, RawCDFormatDescriptor *pDesc)
			: ParsedCDImageBase(pFile, (unsigned)((pFile->GetSize() - pDesc->InitialOffset) / pDesc->SectorSize) + 150)
			, m_FormatDescriptor(*pDesc)
		{
		}

		virtual size_t ReadSectorsBoundsChecked(unsigned FirstSector, void *pBuffer, unsigned SectorCount)
		{
			if (m_FormatDescriptor.SectorSize == CD_SECTOR_SIZE)
			{
				ULONGLONG offset = m_FormatDescriptor.InitialOffset + ((ULONGLONG)FirstSector) * CD_SECTOR_SIZE;
				if (m_pFile->Seek(offset, FileFlags::FileBegin) != offset)
					return 0;
				return m_pFile->Read(pBuffer, SectorCount * CD_SECTOR_SIZE);
			}

			for (unsigned i = 0; i < SectorCount; i++)
				if (!ReadSectorFromFile(m_FormatDescriptor.InitialOffset + ((ULONGLONG)(FirstSector + i)) * m_FormatDescriptor.SectorSize, 
					pBuffer,
					i * CD_SECTOR_SIZE))
					return i * CD_SECTOR_SIZE;
			return SectorCount * CD_SECTOR_SIZE;
		}

	};

	class IMGParser : public AICDImageFormatParser
	{
	public:
		virtual ProbeResult Probe(const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath, bool SecondPass);
		virtual ManagedPointer<AIParsedCDImage> OpenImage(const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath);

	};
}