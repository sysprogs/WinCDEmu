#include "stdafx.h"
#include "NRGParser.h"
#include "NRGRecordSet.h"
#include "NRGStructures.h"
#include "../MultiTrackImage.h"

using namespace BazisLib;
using namespace ImageFormats;

static const CDTrackMode s_NeroTrackModes[] = {
	{kMODE1,		2048, 0,  0},	//Code 0
	{kUnknown,		0,	  0,  0},	//Code 1
	{kMODE2_FORM1,	2048, 0,  0},	//Code 2
	{kMODE2_FORM2,	2336, 0,  0},	//Code 3
	{kUnknown,		0,	  0,  0},	//Code 4
	{kMODE1,		2352, 0,  16},	//Code 5
	{kMODE2_MIXED,	2352, 0,  24},	//Code 6
	{kAUDIO,		2352, 0,  0},	//Code 7
	{kUnknown,		0,	  0,  0},	//Code 8
	{kUnknown,		0,	  0,  0},	//Code 9
	{kUnknown,		0,	  0,  0},	//Code A
	{kUnknown,		0,	  0,  0},	//Code B
	{kUnknown,		0,	  0,  0},	//Code C
	{kUnknown,		0,	  0,  0},	//Code D
	{kUnknown,		0,	  0,  0},	//Code E
	{kMODE1,		2352, 96, 16},	//Code F
	{kAUDIO,		2352, 96, 0},	//Code 0x10
	{kMODE2_MIXED,	2352, 96, 24},	//Code 0x11
};
C_ASSERT(__countof(s_NeroTrackModes) == 0x12);


class NRGSession
{
public:
	struct ParsedCUERecord
	{
		UCHAR AdrCtl;
		UCHAR Track;
		UCHAR Index;
		unsigned LBA;
	};

	struct ParsedTrackEntry
	{
		unsigned SectorSize;

		ULONGLONG PregapOffset;
		ULONGLONG StartOffset;
		ULONGLONG EndOffset;

		CDTrackMode Mode;
	};

	struct ParsedDAOHeader
	{
		unsigned SessionType;
	};

public:
	std::vector<ParsedCUERecord> CUERecords;
	std::vector<ParsedTrackEntry> TrackRecords;
	ParsedDAOHeader DAOHeader;

private:
	bool m_bValid;

	template <typename _OffsetType, NRGRecordType recType> bool ParseDAOBlock(NRGRecordSet &records, unsigned sessionNumber)
	{
		NRGRecordSet::RecordData *pDAO = records.LookupRecord(recType, sessionNumber);
		if (!pDAO)
			return false;
		NRGDAOHeader *pHdr = (NRGDAOHeader *)pDAO->pRecord;

		DAOHeader.SessionType = pHdr->SessionType;

		unsigned recCount = (pDAO->Size - sizeof(NRGDAOHeader)) / sizeof(_NRGDAOEntry<_OffsetType>);
		_NRGDAOEntry<_OffsetType> *pEntries = (_NRGDAOEntry<_OffsetType> *)(pHdr + 1);
		for (unsigned i = 0; i < recCount; i++)
		{
			ParsedTrackEntry entry = {SwapByteOrder(&pEntries[i].SectorSize),
									SwapByteOrder(&pEntries[i].PregapOffset),
									SwapByteOrder(&pEntries[i].StartOffset),
									SwapByteOrder(&pEntries[i].EndOffset),
									};
			if (pEntries[i].ModeCode >= __countof(s_NeroTrackModes))
				return false;

			entry.Mode = s_NeroTrackModes[pEntries[i].ModeCode];

			if ((entry.Mode.MainSectorSize + entry.Mode.SubSectorSize) != entry.SectorSize)
			{
				entry.Mode.MainSectorSize = entry.SectorSize, entry.Mode.SubSectorSize = 0;
				entry.Mode.DataOffsetInSector = (entry.SectorSize == 2352) ? 16 : 0;
			}

			TrackRecords.push_back(entry);
		}
		return (recCount != 0);
	}

	template <typename _OffsetType, NRGRecordType recType> bool ParseETNBlock(NRGRecordSet &records, unsigned sessionNumber)
	{
		NRGRecordSet::RecordData *pETN = records.LookupRecord(recType, sessionNumber);
		if (!pETN)
			return false;

		unsigned recCount = pETN->Size / sizeof(_NRGETNEntry<_OffsetType>);
		_NRGETNEntry<_OffsetType> *pEntries = (_NRGETNEntry<_OffsetType> *)pETN->pRecord;
		for (unsigned i = 0; i < recCount; i++)
		{
			ParsedTrackEntry entry = {0,
									  0,
									  SwapByteOrder(&pEntries[i].Offset),
									  SwapByteOrder(&pEntries[i].Offset) + SwapByteOrder(&pEntries[i].Size)};
			if (pEntries[i].Mode >= __countof(s_NeroTrackModes))
				return false;

			entry.Mode = s_NeroTrackModes[pEntries[i].Mode];
			entry.SectorSize = entry.Mode.MainSectorSize + entry.Mode.SubSectorSize;

			TrackRecords.push_back(entry);
		}
		return (recCount != 0);
	}

public:
	NRGSession (NRGRecordSet &records, unsigned sessionNumber)
		: m_bValid(false)
	{
		NRGRecordSet::RecordData *pCue = records.LookupRecord(records.OldFormat ? NRG_CUES : NRG_CUEX, sessionNumber);
		if (pCue)
		{
			unsigned recCount = pCue->Size / sizeof(NRGCueBlock);
			NRGCueBlock *pRecords = (NRGCueBlock *)pCue->pRecord;
			for (unsigned i = 0; i < recCount; i++)
			{
				ParsedCUERecord rec = {pRecords[i].AdrCtl,
					bcd2int8(pRecords[i].Track),
					bcd2int8(pRecords[i].Index),
					records.OldFormat ? pRecords[i].MSF.ToLBA() : SwapByteOrder32(pRecords[i].StartSector)};

				CUERecords.push_back(rec);
			}
		}

		if (records.OldFormat)
			m_bValid = ParseDAOBlock<unsigned, NRG_DAOI>(records, sessionNumber);
		else
			m_bValid = ParseDAOBlock<ULONGLONG, NRG_DAOX>(records, sessionNumber);

		if (!m_bValid)
		{
			if (records.OldFormat)
				m_bValid = ParseETNBlock<unsigned, NRG_ETNF>(records, sessionNumber);
			else
				m_bValid = ParseETNBlock<ULONGLONG, NRG_ETN2>(records, sessionNumber);
		}

		//WARNING! m_bValid is set from here. Check/reset it if further code is added
	}

	bool Valid()
	{
		return m_bValid;
	}

};


class NRGImage : public MultiTrackCDImageBase
{
public:
	NRGImage(const ConstManagedPointer<AIFile> &pFile, bool *pValid)
		: MultiTrackCDImageBase(pFile)
	{
		ASSERT(pFile && pValid);
		*pValid = false;
		NRGRecordSet records(pFile);
		if (!records.Valid())
			return;

		NRGSession session(records, 0);
		if (!session.Valid())
			return;

		if (session.TrackRecords.size())
		{
			for (size_t i = 0; i < session.TrackRecords.size(); i++)
			{
				const NRGSession::ParsedTrackEntry *pEntry = &session.TrackRecords[i];
				if (pEntry->Mode.Type != kAUDIO)
					if (!AddDataTrack(pEntry->Mode, pEntry->StartOffset, pEntry->EndOffset))
						return;
			}
		}
		*pValid = true;
	}
};


ImageFormats::ProbeResult ImageFormats::NRGParser::Probe( const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath, bool SecondPass )
{
	bool nameMatch = !FileExtension.icompare(_T("nrg"));
	if (!SecondPass)
		return nameMatch ? NameOnlyMatch : NoMatch;
	else
	{
		ASSERT(pFile);
		NRGRecordSet records(pFile);
		ProbeResult ret = nameMatch ? NameOnlyMatch : NoMatch;
		if (!records.Valid())
			return NoMatch;
		return (ProbeResult)(ret | FormatOnlyMatch);
	}
}

ManagedPointer<AIParsedCDImage> ImageFormats::NRGParser::OpenImage( const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath )
{
	bool valid = false;
	ManagedPointer<NRGImage> pImage = new NRGImage(pFile, &valid);
	if (!valid)
		return NULL;
	return pImage;//managed_cast<AIParsedCDImage>(pImage);
}