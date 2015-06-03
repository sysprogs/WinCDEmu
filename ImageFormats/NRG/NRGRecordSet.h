#pragma once
#include <map>
#include <vector>
#include <bzscore/buffer.h>
#include <bzshlp/endian.h>

namespace ImageFormats
{
	using namespace BazisLib;

	#define _R(val) ((((val) & 0xFF000000) >> 24) | (((val) & 0x00FF0000) >> 8) | (((val) & 0x0000FF00) << 8) | (((val) & 0x000000FF) << 24))

	enum NRGRecordType	//Just to see things better in the debugger
	{
		NRG_CUEX = _R('CUEX'),
		NRG_CUES = _R('CUES'),
		NRG_ETN2 = _R('ETN2'),
		NRG_ETNF = _R('ETNF'),
		NRG_DAOX = _R('DAOX'),
		NRG_DAOI = _R('DAOI'),
		NRG_CDTX = _R('CDTX'),
		NRG_SINF = _R('SINF'),
		NRG_MTYP = _R('MTYP'),
	};

	class NRGRecordSet
	{
	private:
		CBuffer m_RawRecords;

	public:
		struct RawRecordHeader
		{
			NRGRecordType ID;
			unsigned SizeMSB;
		};

		struct RecordData
		{
			void *pRecord;
			size_t Size;
		};

		typedef std::map<NRGRecordType, std::vector<RecordData>> RecordMap;
		RecordMap Records;
		bool OldFormat;

	public:
		NRGRecordSet(const ConstManagedPointer<AIFile> &pFile)
			: OldFormat(false)
		{
			if (!pFile)
				return;
			ULONGLONG baseOffset = 0;

			unsigned signature = 0;
			pFile->Seek(-12LL, FileFlags::FileEnd);
			pFile->Read(&signature, sizeof(signature));
			if (signature == _R('NER5'))
			{
				pFile->Read(&baseOffset, sizeof(baseOffset));
				baseOffset = SwapByteOrder64(baseOffset);
			}
			else
			{
				pFile->Read(&signature, sizeof(signature));
				if (signature != _R('NERO'))
					return;
				OldFormat = true;
				unsigned off32 = 0;
				pFile->Read(&off32, sizeof(off32));
				baseOffset = SwapByteOrder32(off32);			
			}
			if (!baseOffset)
				return;

			unsigned recBlockSize = (unsigned)(pFile->GetSize() - baseOffset);
			if (recBlockSize > (1024 * 1024))
				return;
			if (!m_RawRecords.EnsureSize(recBlockSize))
				return;
			if (pFile->Seek(baseOffset, FileFlags::FileBegin) != baseOffset)
				return;
			if (pFile->Read(m_RawRecords.GetData(), m_RawRecords.GetAllocated()) != m_RawRecords.GetAllocated())
				return;
			m_RawRecords.SetSize(recBlockSize);

			void *pRawEnd = ((char *)m_RawRecords.GetData()) + recBlockSize;

			for (RawRecordHeader *pRec = (RawRecordHeader *)m_RawRecords.GetData();
				(pRec < pRawEnd) && (pRec->ID != _R('END!'));
				pRec = ((RawRecordHeader *)(((char *)(pRec + 1)) + SwapByteOrder32(pRec->SizeMSB))))
			{
				RecordData rec = {pRec + 1, SwapByteOrder32(pRec->SizeMSB)};
				Records[pRec->ID].push_back(rec);
			}
		}

		RecordData *LookupRecord(NRGRecordType type, unsigned index = 0)
		{
			RecordMap::iterator it = Records.find(type);
			if (it == Records.end())
				return NULL;
			if (index >= it->second.size())
				return NULL;
			return &it->second[index];
		}

		bool Valid()
		{
			return !Records.empty();
		}
	};
}