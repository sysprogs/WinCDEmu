/*
 * Simplified UDF 2.50 filesystem parser. 
 * Allows checking for VIDEO_TS / BDMV directory to detect video discs
 * Copyright (C) 2011  Ivan Shcherbakov
 * Licensed under LGPL
 * In simple words, if you modify this file to use it in your project, publish the modified sources!
*/

#include "stdafx.h"
#include "miniudf.h"
#include "UDFHelpers.h"
#include <bzscore/buffer.h>

using namespace UDFStructures;

static bool VerifyTagChecksum(Tag *pTag)
{
	UDFStructures::byte sum = 0;
	UDFStructures::byte *p = (UDFStructures::byte *)pTag;
	for (int i = 0; i < sizeof(Tag); i++)
	{
		if (i != FIELD_OFFSET(Tag, TagChecksum))
			sum += p[i];
	}
	return (sum == pTag->TagChecksum);
}


UDFDisc::UDFDisc(IDisc *pDisc)
	: m_pDisc(pDisc)
	, m_bHeadersParsed(false)
	, m_NormalPartitionCount(0)
	, m_pInvalidPartitionObject(new InvalidPartition())
{

}


bool UDFDisc::ReadAndCheckDescriptor(SectorIndex SectorNumber, void *pData, size_t DataSize)
{
	if (SectorNumber == kInvalidSector)
		return false;
	if (DataSize > sizeof(m_SectorBuffer))
		return false;
	if (!m_pDisc->ReadSector(SectorNumber, m_SectorBuffer, sizeof(m_SectorBuffer)))
		return false;
	if (!ValidateUDFDescriptor(SectorNumber, (UDFStructures::Tag *)m_SectorBuffer, sizeof(m_SectorBuffer)))
		return false;
	if (pData && DataSize)
		memcpy(pData, m_SectorBuffer, DataSize);
	return true;
}


bool UDFDisc::ValidateUDFDescriptor(SectorIndex VirtualSectorNumber, UDFStructures::Tag *pTag, size_t descriptorSize)
{
/*	if (pTag->TagLocation != VirtualSectorNumber)
		return false;*/
	if (!VerifyTagChecksum(pTag))
		return false;
	//Verify CRC
	if (pTag->DescriptorCRCLength == 0)
		return true;
	if (pTag->DescriptorCRCLength > (descriptorSize - sizeof(Tag)))
		return true;

	unsigned short CRC = UDFHelpers::CRC16(pTag + 1, pTag->DescriptorCRCLength);
	if (CRC != pTag->DescriptorCRC)
		return false;

	return true;
}



bool UDFDisc::ParsePartitionDescriptors(SectorIndex firstSector, unsigned sectorCount)
{
	//Scan the volume descriptor sequence. Interpret every descriptor based on its type.
	for (SectorIndex i = 0; i < sectorCount; i++)
	{
		if (!ReadAndCheckDescriptor(firstSector + i))
			continue;
		Tag *pTag = (Tag *)m_SectorBuffer;
		if (pTag->TagIdentifier != dtPartitionDescriptor)
			continue;

		PartitionDescriptor *pPD = (PartitionDescriptor *)m_SectorBuffer;
		unsigned partitionNumber = pPD->PartitionNumber;
		if (partitionNumber >= kMaxVolumesAndPartitions)
			continue;

		if (m_AllPartitions.size() <= partitionNumber)
			m_AllPartitions.resize(partitionNumber + 1);

		if (m_AllPartitions[partitionNumber])
			delete m_AllPartitions[partitionNumber];

		m_AllPartitions[partitionNumber] = new NormalPartition(pPD->PartitionStartingLocation, pPD->PartitionLength);
	}

	m_NormalPartitionCount = m_AllPartitions.size();
	return true;
}


bool UDFDisc::ParseLVDInSectorBuffer(LogicalVolume *pVolume)
{
	//We might need to read another sectors to fetch all required information.
	//As the m_SectorBuffer will be overwritten in these cases, we copy it to a local buffer.
	BazisLib::TypedBuffer<LogicalVolumeDescriptor> pLVD;
	pLVD.EnsureSizeAndSetUsed(sizeof(m_SectorBuffer));
	memcpy(pLVD.GetData(), m_SectorBuffer, sizeof(m_SectorBuffer));

	if (pLVD->LogicalBlockSize != kSectorSize)
		return false;

	Uint8 *pPartitionMap = (Uint8 *)pLVD.GetDataAfterStructure();
	Uint8 *pPartitionMapEnd = pPartitionMap + pLVD->MapTableLength;
	Uint8 *pSectorEnd = ((Uint8 *)pLVD.GetData()) + kSectorSize;

	if (pPartitionMapEnd > pSectorEnd)
		pPartitionMapEnd = pSectorEnd;

	long_ad fdesc = pLVD->FileSetDescriptorSequence;

	bool sequenceNumberDefined = false;

	unsigned entryIndex = 0;
	for (Uint8 *pEntry = pPartitionMap, *pNext = pSectorEnd; (pEntry < pPartitionMapEnd) && (entryIndex < pLVD->NumberofPartitionMaps); pEntry = pNext, entryIndex++)
	{
		pNext = pEntry + pEntry[1];
		if (pNext > pPartitionMapEnd)
			break;
		unsigned sequenceNumber = 0;
		bool sequenceNumberKnown = true;
		IPartition *pPartition = NULL;

		if (pEntry[0] == 1)
		{
			PartitionMapEntries::Type1 *pE = (PartitionMapEntries::Type1 *)pEntry;
			if (pE->PartitionNumber < m_NormalPartitionCount)
				pPartition = m_AllPartitions[pE->PartitionNumber];
			sequenceNumber = pE->VolumeSequenceNumber;
		}
		else if (pEntry[0] == 2)
		{
			PartitionMapEntries::Type2 *pE = (PartitionMapEntries::Type2 *)pEntry;
			sequenceNumber = pE->VolumeSequenceNumber;

			IPartition *pUnderlyingPartition = m_pInvalidPartitionObject;
			if (pE->PartitionNumber < m_NormalPartitionCount)
				pUnderlyingPartition = m_AllPartitions[pE->PartitionNumber];

			std::string partitionTypeID(pE->PartitionTypeIdentifier.Identifier, sizeof(pE->PartitionTypeIdentifier.Identifier));

			if (partitionTypeID == "*UDF Metadata Partition")
				pPartition = CreateFileFromICB(VirtualDataBlock(pUnderlyingPartition, pE->MetadataPartition.MetadataFileLocation, kSectorSize));

			if (pPartition)
				m_AllPartitions.push_back(pPartition);

		}
		else
			sequenceNumberKnown = false;

		if (sequenceNumberKnown)
		{
			if (!sequenceNumberDefined)
				pVolume->VolumeSequenceNumber = sequenceNumber;
			else
				if (pVolume->VolumeSequenceNumber != sequenceNumber)
					return false;
		}

		if (!pPartition)
			pPartition = m_pInvalidPartitionObject;

		pVolume->Partitions.push_back(pPartition);
	}

	pVolume->FileSet = pVolume->LongADToDataBlock(pLVD->FileSetDescriptorSequence);

	if (!ReadAndCheckDescriptor(pVolume->FileSet.GetFirstPhysicalSector()))
		return false;
	FileSetDescriptor *pFSD = (FileSetDescriptor *)m_SectorBuffer;
	if (pFSD->DescriptorTag.TagIdentifier != dtFileSet)
		return false;

	pVolume->RootDirectory = pVolume->LongADToDataBlock(pFSD->RootDirectoryICB);

	if (!pVolume->RootDirectory.GetICB().Valid())
		return false;

	return true;
}

bool UDFDisc::ParseUDFHeaders()
{
	enum {kAnchorVolumeDescriptorPointerLocation = 256};
	AnchorVolumeDescriptorPointer anchor;
	if (!ReadAndCheckDescriptor(kAnchorVolumeDescriptorPointerLocation, &anchor))
		return false;

	if (!ParsePartitionDescriptors(anchor.MainVolumeDescriptorSequenceExtent.Location, (anchor.MainVolumeDescriptorSequenceExtent.Length + kSectorSize - 1) / kSectorSize))
		return false;

	//Scan the volume descriptor sequence. Interpret every descriptor based on its type.
	for (SectorIndex i = 0; i < (anchor.MainVolumeDescriptorSequenceExtent.Length + kSectorSize - 1) / kSectorSize; i++)
	{
		if (!ReadAndCheckDescriptor(anchor.MainVolumeDescriptorSequenceExtent.Location + i))
			continue;
		Tag *pTag = (Tag *)m_SectorBuffer;
		if (pTag->TagIdentifier == dtPrimaryVolumeDescriptor)
		{
			PrimaryVolumeDescriptor *pPVD = (PrimaryVolumeDescriptor *)m_SectorBuffer;
			m_VolumeID = UDFHelpers::UncompressUnicode(pPVD->VolumeIdentifier);
		}
		else if (pTag->TagIdentifier == dtLogicalVolumeDescriptor)
		{
			LogicalVolumeDescriptor *pLVD = (LogicalVolumeDescriptor *)m_SectorBuffer;
			m_LogicalVolumes.push_back(LogicalVolume());

			if (!ParseLVDInSectorBuffer(&m_LogicalVolumes.back()))
				m_LogicalVolumes.pop_back();
		}
	}

	return true;
}

UDFDisc::FragmentedFile * UDFDisc::CreateFileFromICB(const VirtualDataBlock &ICB)
{
	if (!ReadAndCheckDescriptor(ICB.GetFirstPhysicalSector()))
		return false;

	Tag *pTag = (Tag *)m_SectorBuffer;
	short_ad *pFirstAD = NULL, *pADEnd = NULL;
	ULONGLONG fileSize = 0;

	if (pTag->TagIdentifier == dtFileEntry)
	{
		FileEntry *pFE = (FileEntry *)m_SectorBuffer;
		fileSize = pFE->InformationLength;
		pFirstAD = (short_ad *)(((Uint8 *)pFE) + sizeof(*pFE) + pFE->LengthofExtendedAttributes);
		pADEnd = (short_ad *)(((Uint8 *)pFirstAD) + pFE->LengthofAllocationDescriptors);
	}
	else if (pTag->TagIdentifier == dtExtendedFileEntry)
	{
		ExtendedFileEntry *pEFE = (ExtendedFileEntry *)m_SectorBuffer;
		fileSize = pEFE->InformationLength;
		pFirstAD = (short_ad *)(((Uint8 *)pEFE) + sizeof(*pEFE) + pEFE->LengthofExtendedAttributes);
		pADEnd = (short_ad *)(((Uint8 *)pFirstAD) + pEFE->LengthofAllocationDescriptors);
	}
	else
		return NULL;

	if ((Uint8 *)pADEnd > (m_SectorBuffer + sizeof(m_SectorBuffer)))
		pADEnd = ((short_ad *)(m_SectorBuffer + sizeof(m_SectorBuffer)));

	FragmentedFile *pPartition = new FragmentedFile(ICB.GetPartition(), fileSize);

	for (short_ad *pAD = pFirstAD; pAD < pADEnd; pAD++)
		pPartition->AppendExtent(pAD->Position, pAD->Length);

	return pPartition;
}

UDFDisc::~UDFDisc()
{
	for each(IPartition *pPartition in m_AllPartitions)
		delete pPartition;
	delete m_pInvalidPartitionObject;
}

UDFDisc::LogicalVolume *UDFDisc::FindLastVolume()
{
	unsigned maxIdx = 0;

	if (m_LogicalVolumes.empty())
		return NULL;

	for (size_t i = 1; i < m_LogicalVolumes.size(); i++)
		if (m_LogicalVolumes[i].VolumeSequenceNumber > m_LogicalVolumes[maxIdx].VolumeSequenceNumber)		
			maxIdx = i;

	return &m_LogicalVolumes[maxIdx];
}

UDFDisc::VirtualDataBlock UDFDisc::FindFileInDirectory(LogicalVolume *pVolume, const FileDesc &directory, const wchar_t *pwszFileName, bool caseSensitive /*= false*/)
{
	FragmentedFile *pDirectoryData = CreateFileFromICB(directory.GetICB());
	if (!pDirectoryData)
		return UDFDisc::VirtualDataBlock();

	VirtualDataBlock dirData = pDirectoryData->EntireFile();
	BazisLib::TypedBuffer<FileIdentifierDescriptor> pDesc;

	SmallCacheBuffer cacheBuf;
	for (ULONGLONG i = 0; i < directory.GetICB().Size();)
	{
		FileIdentifierDescriptor desc;
		if (ReadStructureCached(dirData, cacheBuf, i, &desc, sizeof(desc)) != sizeof(desc))
			break;
		size_t totalSize = sizeof(FileIdentifierDescriptor) + desc.LengthofFileIdentifier + desc.LengthOfImplementationUse;
		totalSize = (totalSize + 3) & ~3;	//Align to DWORD boundary
		pDesc.EnsureSizeAndSetUsed(totalSize);
		if (ReadStructureCached(dirData, cacheBuf, i, pDesc, totalSize) != totalSize)
			break;

		if (!ValidateUDFDescriptor(kInvalidSector, &pDesc->DescriptorTag, totalSize))
			break;

		std::wstring fileName = UDFHelpers::UncompressUnicode(pDesc->LengthofFileIdentifier, ((unsigned char *)pDesc.GetDataAfterStructure()) + pDesc->LengthOfImplementationUse);
		bool match;
		if (caseSensitive)
			match = !wcscmp(fileName.c_str(), pwszFileName);
		else
			match = !_wcsicmp(fileName.c_str(), pwszFileName);

		if (match)
			return pVolume->LongADToDataBlock(pDesc->ICB);

		i += totalSize;
	}
	
	delete pDirectoryData;
	return UDFDisc::VirtualDataBlock();
}

size_t UDFDisc::ReadStructureCached(VirtualDataBlock &data, SmallCacheBuffer &cacheBuf, ULONGLONG offset, void *pBuffer, size_t size)
{
	size_t done = 0;
	while (done < size)
	{
		SectorIndex sectorNumber = (SectorIndex)((offset + done) / kSectorSize);
		unsigned sectorOffset = (SectorIndex)((offset + done) % kSectorSize);

		SectorIndex physicalSector = data.GetPartition()->MapSector(data.GetFirstVirtualSector() + sectorNumber);

		size_t bytesInSector = kSectorSize - sectorOffset;
		size_t todo = min(size - done, bytesInSector);

		if (cacheBuf.m_SectorNumber != physicalSector)
		{
			if (!m_pDisc->ReadSector(physicalSector, cacheBuf.m_pBuffer, kSectorSize))
				return 0;
			cacheBuf.m_SectorNumber = physicalSector;
		}
		
		memcpy((char *)pBuffer + done, cacheBuf.m_pBuffer + sectorOffset, todo);
		done += todo;
	}
	return size;
}

UDFDisc::VirtualDataBlock UDFDisc::LogicalVolume::LongADToDataBlock(const struct long_ad &AD)
{
	if (AD.PartitionNumber >= Partitions.size())
		return VirtualDataBlock();
	return VirtualDataBlock(Partitions[AD.PartitionNumber], AD.LBA, AD.Length);
}