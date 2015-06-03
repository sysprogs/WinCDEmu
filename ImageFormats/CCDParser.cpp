#include "stdafx.h"
#include "CCDParser.h"
#include "RawCDFormats.h"
#include <bzshlp/textfile.h>
#include "IMGParser.h"
//#include "MultiTrackImage.h"

#include <vector>
#include <map>

using namespace BazisLib;
using namespace ImageFormats;


class ParsedCCDFile
{
private:
	typedef std::map<DynamicStringA, int> TOCEntry;
	
public:
	unsigned m_Version;
	std::vector<TOCEntry> m_TOCEntries;
	bool m_bValid;

	struct TrackRecord
	{
		CDTrackMode Mode;
	};

	std::vector<TrackRecord> m_Tracks;

	String m_BinaryFile;

private:
	enum CCDSectionType
	{
		CCDUnknown,
		CCDGlobal,
		CCDDisc,
		CCDSession,
		CCDEntry,
		CCDTrack,
	};

	CCDSectionType DetectSectionType(const TempStringA &sectionName, unsigned *pParam)
	{
		if (pParam)
			*pParam = 0;
		if (!sectionName.icompare("[CloneCD]"))
			return CCDGlobal;
		else if (!sectionName.icompare("[Disc]"))
			return CCDDisc;
		else if (!sectionName.icompare(0, 9, "[Session "))
		{
			if (pParam)
				*pParam = atoi(sectionName.GetConstBuffer() + 9);
			return CCDSession;
		}
		else if (!sectionName.icompare(0, 7, "[Entry "))
		{
			if (pParam)
				*pParam = atoi(sectionName.GetConstBuffer() + 7);
			return CCDEntry;
		}
		else if (!sectionName.icompare(0, 7, "[Track "))
		{
			if (pParam)
				*pParam = atoi(sectionName.GetConstBuffer() + 7);
			return CCDTrack;
		}
		else
			return CCDUnknown;
	}

	template <typename _Ty> inline _Ty ConstrainInt(_Ty val, _Ty minVal, _Ty maxVal)
	{
		if (val < minVal)
			return minVal;
		if (val > maxVal)
			return maxVal;
		return val;
	}

	
public:
	ParsedCCDFile(const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath)
		: m_bValid(false)
		, m_BinaryFile(_T(""))
	{
		ASSERT(pFile && pFile->Valid());
		pFile->Seek(0, FileFlags::FileBegin);
		if (pFile->GetSize() > 16384)
			return;

		m_BinaryFile = Path::GetPathWithoutExtension(FullFilePath) + _T(".img");
		if (!File::Exists(m_BinaryFile))
			return;

		ManagedPointer<TextANSIFileReader> pReader = new TextANSIFileReader(pFile);
		if (!pReader)
			return;

		DynamicStringA str;

		CCDSectionType CurrentSection = CCDUnknown;
		unsigned sectionParam = 0;

		while (!pReader->IsEOF())
		{
			str = pReader->ReadLine();
			if (str.empty())
				continue;
			if (str[0] == '[')
			{
				CurrentSection = DetectSectionType(str, &sectionParam);
				continue;
			}
			
			off_t eqOff = str.find('=');
			if (eqOff == -1)
				continue;

			TempStringA key = str.substr(0, eqOff).Strip(" \t");
			TempStringA val = str.substr(eqOff + 1).Strip(" \t");

			int intVal = atoi(val.GetConstBuffer());

			if (key.empty())
				continue;

			switch (CurrentSection)
			{
			case CCDGlobal:
				if (!key.icompare("Version"))
					m_Version = intVal;
				break;
			case CCDDisc:
				if (!key.icompare("TocEntries"))
					m_TOCEntries.reserve(ConstrainInt(intVal, 0, 99));
				/*if (!key.icompare("Sessions"))
					...(ConstrainInt(intVal, 1, 99));*/
				break;
			case CCDSession:
				break;
			case CCDEntry:
				{
					unsigned entryIdx = ConstrainInt<unsigned>(sectionParam, 0, 99);
					if (entryIdx >= m_TOCEntries.size())
						m_TOCEntries.resize(entryIdx + 1);	//Should not normally happen
					m_TOCEntries[entryIdx][key] = intVal;
				}
				break;
			case CCDTrack:
				if (!key.icompare("mode"))
				{
					unsigned trackIdx = ConstrainInt<unsigned>(sectionParam - 1, 0, 99);
					if (trackIdx >= m_Tracks.size())
						m_Tracks.resize(trackIdx + 1);
					TrackRecord *pTrack = &m_Tracks[trackIdx];
					pTrack->Mode.MainSectorSize = 2352;
					pTrack->Mode.SubSectorSize = 0;
					switch (intVal)
					{
					case 0:
						pTrack->Mode.Type = kAUDIO;
						break;
					case 1:
						pTrack->Mode.Type = kMODE1;
						break;
					case 2:
						pTrack->Mode.Type = kMODE2_MIXED;
						break;
					}
					
				}
				break;
			}
		}
		if (!m_Tracks.empty())
			m_bValid = true;
	}

	bool IsValid() {return m_bValid;}
};

ImageFormats::ProbeResult ImageFormats::CCDParser::Probe( const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath, bool SecondPass )
{
	bool nameMatch = !FileExtension.icompare(_T("ccd"));
	if (!SecondPass)
		return nameMatch ? NameOnlyMatch : NoMatch;
	else
	{
		ASSERT(pFile);
		ParsedCCDFile parsed(pFile, FullFilePath);
		ProbeResult ret = nameMatch ? NameOnlyMatch : NoMatch;
		if (!parsed.IsValid())
			return NoMatch;
		return (ProbeResult)(ret | FormatOnlyMatch);
	}
}

/*class CCDImage : public MultiTrackCDImageBase
{
public:
	CCDImage(const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath, bool *pValid)
		: MultiTrackCDImageBase(pFile)
	{
		ASSERT(pFile && pValid);
		*pValid = false;

		ParsedCCDFile parsed(pFile, FullFilePath);
		if (!parsed.IsValid())
			return;



		*pValid = true;
	}
};*/

ManagedPointer<AIParsedCDImage> ImageFormats::CCDParser::OpenImage( const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath )
{
	ParsedCCDFile parsed(pFile, FullFilePath);
	if (!parsed.IsValid())
		return NULL;
	if (parsed.m_Tracks.empty())
		return NULL;
	//Treat the entire image as a single-track one
	RawCDFormatDescriptor format = {2352, 16};
	ManagedPointer<AIFile> pBinaryFile = new ACFile(parsed.m_BinaryFile, FileModes::OpenReadOnly.ShareAll());
	if (!pBinaryFile || !pBinaryFile->Valid())
		return NULL;
	format.InitialOffset = DetectDataOffsetInSector(pBinaryFile, format.SectorSize, format.InitialOffset);
	return new IMGImage(pBinaryFile, &format);
}
