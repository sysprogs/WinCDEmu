#include "stdafx.h"
#include "MDSParser.h"
#include "MDSFormats.h"
#include <algorithm>
#include <vector>
#include "../MultiTrackImage.h"
#include "../RawCDFormats.h"

using namespace BazisLib;
using namespace ImageFormats;

struct ParsedTrackRecord
{
	ULONGLONG OffsetInFile;
	
	unsigned SectorSize;
	unsigned SectorCount;

	CDTrackMode Mode;
	unsigned TrackFileIndex;
};

static const CDTrackModeType s_MDSTrackModeTypes[] = {
	kMODE2,			//Code 0
	kAUDIO,			//Code 1
	kMODE1,			//Code 2
	kMODE2,			//Code 3
	kMODE2_FORM1,	//Code 4
	kMODE2_FORM2,	//Code 5
	kUnknown,		//Code 6
	kMODE2,			//Code 7
};

C_ASSERT(__countof(s_MDSTrackModeTypes) == 8);

class MDSSession
{
private:
	String ReadANSIString(const ConstManagedPointer<AIFile> &pFile, ULONGLONG Offset)
	{
		char sz[256] = {0,};
		pFile->Seek(Offset, FileFlags::FileBegin);
		pFile->Read(sz, sizeof(sz) - 1);
		return ANSIStringToString(ConstStringA(sz));
	}

	String ReadUnicodeString(const ConstManagedPointer<AIFile> &pFile, ULONGLONG Offset)
	{
		wchar_t wsz[256] = {0,};
		pFile->Seek(Offset, FileFlags::FileBegin);
		pFile->Read(wsz, _countof(wsz) - 1);
		return String(wsz);
	}
public:
	std::vector<String> MDFFiles;
	std::vector<ParsedTrackRecord> Tracks;

public:
	MDSSession(MDSHeader *pHeader, const ConstManagedPointer<AIFile> &pFile, MDSSessionBlock *pSessionHeader, const String&MDSFilePath, bool *pSuccessful)
	{
		ASSERT(pFile && pSessionHeader && pSuccessful);
		*pSuccessful = false;
		
		for (unsigned i = 0; i < pSessionHeader->TotalBlockCount; i++)
		{
			MDSTrackBlock track;
			MDSTrackExtraBlock extraInfo;
			pFile->Seek(pSessionHeader->TrackBlocksOffset + i * sizeof(MDSTrackBlock), FileFlags::FileBegin);
			if (pFile->Read(&track, sizeof(track)) != sizeof(track))
				return;

			if (!track.ExtraOffset)
				continue;

			if ((pHeader->MediumType & 0x10))	//If this is a DVD, use different handling
			{
				extraInfo.Length = track.ExtraOffset;
				extraInfo.Pregap = 0;
			}
			else
			{
				pFile->Seek(track.ExtraOffset, FileFlags::FileBegin);
				if (pFile->Read(&extraInfo, sizeof(extraInfo)) != sizeof(extraInfo))
					return;

				if (!extraInfo.Length)
					continue;
			}

			ParsedTrackRecord parsedTrack = {0,};

			if (track.FooterOffset)
			{
				MDSFooter footer;
				pFile->Seek(track.FooterOffset);
				if (pFile->Read(&footer, sizeof(footer)) != sizeof(footer))
					return;
				if (!footer.FilenameOffset)
					continue;
				String fn = footer.WidecharFilename ? ReadUnicodeString(pFile, footer.FilenameOffset) : ReadANSIString(pFile, footer.FilenameOffset);
				if (fn.empty())
					return;
				if ((fn.length() >= 2) && (fn[0] == '*') && (fn[1] == '.'))
					fn.replace(0, 1, Path::GetPathWithoutExtension(MDSFilePath));

				std::vector<String>::iterator it = std::find(MDFFiles.begin(), MDFFiles.end(), fn);
				if (it == MDFFiles.end())
					parsedTrack.TrackFileIndex = MDFFiles.size(), MDFFiles.push_back(fn);
				else
					parsedTrack.TrackFileIndex = it - MDFFiles.begin();
			}

			parsedTrack.OffsetInFile = track.StartOffset;
			parsedTrack.SectorSize = track.SectorSize;
			parsedTrack.SectorCount = extraInfo.Length;

			if ((track.Mode & 0x07) >= __countof(s_MDSTrackModeTypes))
				return;

			parsedTrack.Mode.Type = s_MDSTrackModeTypes[track.Mode & 0x07];
			parsedTrack.Mode.MainSectorSize = track.SectorSize;
			parsedTrack.Mode.SubSectorSize = 0;

			if (track.SubchannelMode == 8)
			{
				parsedTrack.Mode.MainSectorSize -= 96;
				parsedTrack.Mode.SubSectorSize = 96;
			}

			{
				ManagedPointer<AIFile> pMdfFile = new ACFile(MDFFiles[parsedTrack.TrackFileIndex], FileModes::OpenReadOnly.ShareAll());
				if (pMdfFile && pMdfFile->Valid())
				{
					parsedTrack.Mode.DataOffsetInSector = DetectDataOffsetInSector(pMdfFile, parsedTrack.SectorSize, 0, parsedTrack.OffsetInFile);
				}
			}

			Tracks.push_back(parsedTrack);
		}

		*pSuccessful = true;				
	}

};

class ParsedMDSFile
{
private:
	bool m_bValid;

public:
	String MDFFilePath;
	std::list<MDSSession> Sessions;

public:
	ParsedMDSFile(const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath)
		: m_bValid(false)
		, MDFFilePath(L"")
	{
		pFile->Seek(0, FileFlags::FileBegin);
		MDSHeader hdr;
		if (pFile->Read(&hdr, sizeof(MDSHeader)) != sizeof(MDSHeader))
			return;
		if (memcmp(hdr.Signature, "MEDIA DESCRIPTOR", sizeof(hdr.Signature)))
			return;

		for (unsigned i = 0; i < hdr.SessionCount; i++)
		{
			MDSSessionBlock session;
			pFile->Seek(hdr.SessionsBlocksOffset + i * sizeof(MDSSessionBlock), FileFlags::FileBegin);
			if (pFile->Read(&session, sizeof(MDSSessionBlock)) != sizeof(MDSSessionBlock))
				return;
			bool bSessionOk = false;
			Sessions.push_back(MDSSession(&hdr, pFile, &session, FullFilePath, &bSessionOk));
			if (!bSessionOk)
				Sessions.pop_back();			
		}
		if (Sessions.size())
			m_bValid = true;
	}

	bool Valid() const
	{
		return m_bValid;
	}

	ManagedPointer<AIFile> OpenFirstMDFFile() const
	{
		if (!m_bValid || Sessions.empty())
			return NULL;
		if (Sessions.begin()->MDFFiles.empty())
			return NULL;
		ManagedPointer<AIFile> pFile = new ACFile(*Sessions.begin()->MDFFiles.begin(), FileModes::OpenReadOnly.ShareAll());
		if (!pFile || !pFile->Valid())
			return NULL;
		return pFile;
	}
};

ImageFormats::ProbeResult ImageFormats::MDSParser::Probe( const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath, bool SecondPass )
{
	bool nameMatch = !FileExtension.icompare(_T("mds"));
	if (!SecondPass)
		return nameMatch ? NameOnlyMatch : NoMatch;
	else
	{
		ASSERT(pFile);
		ParsedMDSFile parsedMDS(pFile, FullFilePath);
		if (!parsedMDS.Valid())
			return NoMatch;
		ProbeResult ret = nameMatch ? NameOnlyMatch : NoMatch;
		return (ProbeResult)(ret | FormatOnlyMatch);
	}
}

class MDSImage : public MultiTrackCDImageBase
{
public:
	MDSImage(const ParsedMDSFile &MDSFile, const ConstManagedPointer<AIFile> &pFile)
		: MultiTrackCDImageBase(pFile)
	{
		if (!m_pFile || !m_pFile->Valid())
			return;
		ASSERT(MDSFile.Valid());
		if (!MDSFile.Sessions.empty())
		{
			const MDSSession *pFirstSession = &*MDSFile.Sessions.begin();
			for (size_t i = 0; i < pFirstSession->Tracks.size(); i++)
			{
				const ParsedTrackRecord *pTrack = &*(pFirstSession->Tracks.begin() + i);
				if ((pTrack->Mode.Type != kAUDIO) && !pTrack->TrackFileIndex)
					if (!AddDataTrack(pTrack->Mode, pTrack->OffsetInFile, pTrack->OffsetInFile + (ULONGLONG)pTrack->SectorSize * pTrack->SectorCount))
						return;
			}
		}
	}
};


ManagedPointer<AIParsedCDImage> ImageFormats::MDSParser::OpenImage( const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath )
{
	ParsedMDSFile parsedFile(pFile, FullFilePath);
	if (!parsedFile.Valid() || parsedFile.Sessions.empty())
		return NULL;
	ManagedPointer<AIFile> pMDF = parsedFile.OpenFirstMDFFile();
	if (!pMDF)
		return NULL;
	return new MDSImage(parsedFile, pMDF);
}
