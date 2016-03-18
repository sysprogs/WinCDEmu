#pragma once
#include <vector>
#include "UDFStructures.h"
#include <string>
#include <map>

namespace UDFStructures
{
	struct long_ad;
}

class IDisc
{
public:
	enum {kSectorSize = 2048};
	virtual bool ReadSector(unsigned sectorIndex, void *pBuffer, size_t sectorSize)=0;
};

//Non-thread-safe
class UDFDisc
{
public:
	typedef unsigned SectorIndex;
	enum {kInvalidSector = -1};
	enum {kSectorSize = 2048, kMaxVolumesAndPartitions = 100};

private:
#pragma region Partition classes
	struct IPartition
	{
		virtual SectorIndex MapSector(SectorIndex relativeOffset)=0;
		virtual ~IPartition(){}
	};

	class NormalPartition : public IPartition
	{
	private:
		SectorIndex m_FirstSector;
		SectorIndex m_SectorCount;

	public:
		NormalPartition(SectorIndex first, SectorIndex count)
			: m_FirstSector(first)
			, m_SectorCount(count)
		{
		}

		virtual SectorIndex MapSector(SectorIndex relativeOffset) override
		{
			if (relativeOffset >= m_SectorCount)
				return kInvalidSector;
			return m_FirstSector + relativeOffset;
		}
	};

	class InvalidPartition : public IPartition
	{
	public:
		virtual SectorIndex MapSector(SectorIndex relativeOffset) override
		{
			return kInvalidSector;
		}
	};

#pragma endregion

public:
	class VirtualDataBlock
	{
		SectorIndex m_FirstSector;
		ULONGLONG m_ByteCount;
		IPartition *m_pPartition;

	public:
		VirtualDataBlock(IPartition *pPartition = NULL, SectorIndex firstSector = 0, ULONGLONG byteCount  = 0)
			: m_pPartition(pPartition)
			, m_FirstSector(firstSector)
			, m_ByteCount(byteCount)
		{
		}

		ULONGLONG Size() const
		{
			return m_ByteCount;
		}

		SectorIndex GetFirstVirtualSector() const
		{
			return m_FirstSector;
		}

		SectorIndex GetFirstPhysicalSector() const
		{
			if (!m_pPartition)
				return kInvalidSector;
			return m_pPartition->MapSector(m_FirstSector);
		}

		IPartition *GetPartition() const
		{
			return m_pPartition;
		}

		bool Valid() const
		{
			return m_pPartition != NULL;
		}
	};

	class FileDesc
	{
	private:
		VirtualDataBlock m_ICBRange;

	public:
		FileDesc()
		{
		}

		FileDesc(VirtualDataBlock range)
			: m_ICBRange(range)
		{
		}

		IPartition *GetPartition() const
		{
			return m_ICBRange.GetPartition();
		}

		const VirtualDataBlock &GetICB() const
		{
			return m_ICBRange;
		}

	};

	struct LogicalVolume
	{
		unsigned VolumeSequenceNumber;
		std::vector<IPartition *> Partitions;
		VirtualDataBlock FileSet;
		FileDesc RootDirectory;
		
		LogicalVolume()
			: VolumeSequenceNumber(0)
		{
		}

		VirtualDataBlock LongADToDataBlock(const UDFStructures::long_ad &AD);
	};

private:
	class FragmentedFile : public IPartition
	{
	private:
		struct Extent
		{
			SectorIndex Sector;
			SectorIndex Count;

			Extent(SectorIndex sector = 0, SectorIndex count = 0)
				: Sector(sector)
				, Count(count)
			{
			}
		};

		IPartition *m_pUnderlyingPartition;

		std::map<SectorIndex, Extent> m_Extents;	//Key - virtual sector number
		SectorIndex m_TotalSectors;
		ULONGLONG m_SizeInBytes;

		static SectorIndex TransformKey(SectorIndex idx)
		{
			//To efficiently support highly fragmented partitions, this function should perform a one-to-one non-monotonic transformation of idx.
			//I.e. when we are adding increasing indices (e.g. 0, 100, 200, 500), the transformation result should not monotonically grow (e.g. 1,2,7,123)
			//or drop (e.g. 123,100,20,3). Instead, the result should change pseudorandomly (e.g. 100, 2, 500, 144). This will ensure maximum performance
			//with the trees used by m_Extents
			return idx;
		}

	public:
		FragmentedFile(IPartition *pUnderlyingPartition, ULONGLONG totalSize)
			: m_TotalSectors(0)
			, m_pUnderlyingPartition(pUnderlyingPartition)
			, m_SizeInBytes(totalSize)
		{
		}

		void AppendExtent(SectorIndex sector, SectorIndex count)
		{
			m_Extents[m_TotalSectors] = Extent(sector, count);
			m_TotalSectors += count;
		}

		ULONGLONG Size()
		{
			return m_SizeInBytes;
		}

		virtual SectorIndex MapSector(SectorIndex relativeOffset) override
		{
			if(relativeOffset >= m_TotalSectors)
				return kInvalidSector;
			std::map<SectorIndex, Extent>::iterator it = m_Extents.lower_bound(relativeOffset);
			if (it == m_Extents.end())
				it = --m_Extents.end();
			return m_pUnderlyingPartition->MapSector(it->second.Sector + (relativeOffset - it->first));
		}

		VirtualDataBlock EntireFile()
		{
			return VirtualDataBlock(this, 0, m_SizeInBytes);
		}

	};
private:
	IDisc *m_pDisc;
	
	std::vector<IPartition *> m_AllPartitions;
	InvalidPartition *m_pInvalidPartitionObject;
	size_t m_NormalPartitionCount;

	std::vector<LogicalVolume> m_LogicalVolumes;
	bool m_bHeadersParsed;
	std::wstring m_VolumeID;

	unsigned char m_SectorBuffer[kSectorSize];

private:
	bool ValidateUDFDescriptor(SectorIndex VirtualSectorNumber, UDFStructures::Tag *pTag, size_t DescriptorSize);
	bool ReadAndCheckDescriptor(SectorIndex PhysicalSectorNumber, void *pData = NULL, size_t DataSize = 0);

	template <class _Descriptor> bool ReadAndCheckDescriptor(SectorIndex SectorNumber, _Descriptor *pDesc)
	{
		return ReadAndCheckDescriptor(SectorNumber, pDesc, sizeof(_Descriptor));
	}

	bool ParsePartitionDescriptors(SectorIndex firstSector, unsigned sectorCount);
	bool ParseLVDInSectorBuffer(LogicalVolume *pVolume);
	FragmentedFile *CreateFileFromICB(const VirtualDataBlock &ICB);

	class SmallCacheBuffer
	{
	public:
		SectorIndex m_SectorNumber;
		unsigned char *m_pBuffer;

	public:
		SmallCacheBuffer()
		{
			m_SectorNumber = kInvalidSector;
			m_pBuffer = new unsigned char[kSectorSize];
		}

		~SmallCacheBuffer()
		{
			delete m_pBuffer;
		}
	};

	size_t ReadStructureCached(VirtualDataBlock &data, SmallCacheBuffer &cacheBuf, ULONGLONG offset, void *pBuffer, size_t size);	

public:
	UDFDisc(IDisc *pDisc);
	~UDFDisc();
	bool ParseUDFHeaders();

	LogicalVolume *FindLastVolume();

	VirtualDataBlock FindFileInDirectory(LogicalVolume *pVolume, const FileDesc &directory, const wchar_t *pwszFileName, bool caseSensitive = false);

};
