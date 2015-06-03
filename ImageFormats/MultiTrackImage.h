#pragma once
#include <map>
#include <vector>
#include "ImageFormats.h"
#include <bzscore/buffer.h>

namespace ImageFormats
{
	using namespace BazisLib;

	class MultiTrackCDImageBase : public AIParsedCDImage
	{
	private:
		struct TrackInfo
		{
			CDTrackMode Mode;	//Not currently used
			
			ULONGLONG OffsetInFile;
			unsigned ImageSectorSize;	//First 2048 bytes of each sector will be read
			unsigned SectorCount;

			unsigned StartSector;
			unsigned EndSector;
		};

	protected:
		DECLARE_REFERENCE(AIFile, m_pFile);
		unsigned m_TotalSectorCount;

		std::vector<TrackInfo> m_TrackRecords;
		std::map<unsigned, TrackInfo> m_FirstSectorToTrackMap;	//Maps track beginnings to track info structures

		CBuffer m_RawSectorBuffer;

	protected:
		MultiTrackCDImageBase(const ConstManagedPointer<AIFile> &pFile)
			: INIT_REFERENCE(m_pFile, pFile)
			, m_TotalSectorCount(0)
		{
		}

		bool AddDataTrack(const CDTrackMode &Mode, ULONGLONG StartFileOffset, ULONGLONG EndFileOffset)
		{
			unsigned imageSectorSize = Mode.MainSectorSize + Mode.SubSectorSize;
			if (!imageSectorSize)
				return false;
			if ((unsigned)((EndFileOffset - StartFileOffset) % imageSectorSize))
				return false;

			StartFileOffset += Mode.DataOffsetInSector;
			EndFileOffset += Mode.DataOffsetInSector;

			ULONGLONG fileSize = m_pFile->GetSize();
			
			if (EndFileOffset > fileSize)
				EndFileOffset = fileSize;

			TrackInfo info = {Mode, StartFileOffset, imageSectorSize, (unsigned)((EndFileOffset - StartFileOffset) / imageSectorSize)};

			info.ImageSectorSize = imageSectorSize;
			info.StartSector = m_TotalSectorCount;
			info.EndSector = info.StartSector + info.SectorCount;

			m_TrackRecords.push_back(info);
			m_FirstSectorToTrackMap[m_TotalSectorCount] = info;
			m_TotalSectorCount += info.SectorCount;

			if (m_RawSectorBuffer.GetAllocated() < info.ImageSectorSize)
				m_RawSectorBuffer.EnsureSize(info.ImageSectorSize);
			return true;
		}

	public:
		virtual unsigned GetSectorCount() override {return m_TotalSectorCount;}
		virtual unsigned GetTrackCount() override {return m_TrackRecords.size();}
		
		virtual unsigned GetTrackEndSector(unsigned TrackNumber) override
		{
			if (TrackNumber >= m_TrackRecords.size())
				return 0;
			return m_TrackRecords[TrackNumber].EndSector;
		}

		virtual size_t ReadSectorsBoundsChecked(unsigned FirstSector, void *pBuffer, unsigned SectorCount)
		{
			if (!m_TrackRecords.size())
				return 0;

			for (unsigned done = 0, todo = 0; done < SectorCount; done += todo)
			{
				unsigned sector = FirstSector + done;

				std::map<unsigned, TrackInfo>::iterator it = m_FirstSectorToTrackMap.upper_bound(sector);
				if (it != m_FirstSectorToTrackMap.begin())
					it--;
				
				TrackInfo *pTrack = &it->second;

				unsigned inTrackSectorIndex = sector - pTrack->StartSector;
				unsigned maxSectorsFromThisTrack = pTrack->EndSector - inTrackSectorIndex;

				todo = min(maxSectorsFromThisTrack, (SectorCount - done));

				if (!todo)
					return done * CD_SECTOR_SIZE;

				ULONGLONG fileOffset = pTrack->OffsetInFile + (pTrack->ImageSectorSize * (ULONGLONG)inTrackSectorIndex);
				if (m_pFile->Seek(fileOffset, FileFlags::FileBegin) != fileOffset)
					return done * CD_SECTOR_SIZE;

				if (m_RawSectorBuffer.GetAllocated() < pTrack->ImageSectorSize)
				{
					ASSERT(FALSE);
					return done * CD_SECTOR_SIZE;
				}

				if (pTrack->ImageSectorSize == CD_SECTOR_SIZE)
				{
					if (m_pFile->Read(((char *)pBuffer) + (done * CD_SECTOR_SIZE), todo * CD_SECTOR_SIZE) != (todo * CD_SECTOR_SIZE))
						return (done + todo) * CD_SECTOR_SIZE;
				}
				else
				{
					for (unsigned i = 0; i < todo; i++)
					{
						if (m_pFile->Read(m_RawSectorBuffer.GetData(), pTrack->ImageSectorSize) != pTrack->ImageSectorSize)
							return done * CD_SECTOR_SIZE;
						memcpy(((char *)pBuffer) + (done + i) * CD_SECTOR_SIZE, m_RawSectorBuffer.GetData(), CD_SECTOR_SIZE);
					}
				}
			}
			return SectorCount * CD_SECTOR_SIZE;
		}
	};

}