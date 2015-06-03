#pragma once
#include <bzscore/autofile.h>
#include <bzscore/objmgr.h>
#include <map>

//! Contains various classes handling the CD/DVD image formats
/*! This namespace contains various classes and interfaces, that are common to image format parsers.

	A single-track image format parser typically inherits ParsedCDImageBase class, provides information about
	sector count and handles sector read operations.

	A multi-track image format parser inherits MultiTrackCDImageBase class and calls AddDataTrack() for every
	track being recognized. Note that presently only data tracks are supported. 

	\remarks In current version, WinCDEmu rebuilds the TOC of the disk according to provided track information.
			 The tracks are always placed sequentially (original track placement from the image file is discarded).
			 To implement precise track positioning, AddDataTrack() should be replaced by another method, creating
			 more precise track records.
	
	\attention Presently, WinCDEmu supports only data tracks and only single-session disks.	Images containing more than one
			   session info, will be clipped.
*/
namespace ImageFormats
{
	using namespace BazisLib;

	// Various format parsers try probing every binary file and return their results. 
	// The parser with the highest result is finally selected.
	enum ProbeResult
	{
		NoMatch = 0,			//Explicit contradiction found
		ProbableMatch = 0x01,	//File can be of this format, however, neither file extension nor internal binary structures matched.
		NameOnlyMatch = 0x02,	//File extension matches, but binary format does not
		
		//The next codes can be returned during second-pass only
		
		FormatOnlyMatch = 0x1000,	//Extension does not match, but final format seems to
		NameAndFormatMatch = NameOnlyMatch | FormatOnlyMatch,		//Both name and internal format match

		SufficientMatchToSkipOthers = NameAndFormatMatch,
	};

	enum {CD_SECTOR_SIZE = 2048};

	enum CDTrackModeType	//Raw sector size is always 2352 bytes. Data area size in a sector is 2048 bytes.
	{
		kUnknown     = 0x00,
		kAUDIO       = 0x01,
		kMODE1       = 0x02,	//Raw sector = [12 bytes sync] [4 bytes hdr] [2048 bytes data] [ecc + etc]
		
		kMODE2       = 0x103,	//Raw sector = [12 bytes sync] [4 bytes hdr] [8 bytes subhdr] [2048 bytes data] [ecc + etc]
		kMODE2_FORM1 = 0x104,
		kMODE2_FORM2 = 0x105,
		kMODE2_MIXED = 0x106,

		kMODE2_MASK	= 0x100,
	};

	struct CDTrackMode
	{
		CDTrackModeType Type;
		unsigned MainSectorSize;
		unsigned SubSectorSize;
		unsigned DataOffsetInSector;
	};


	//! Allows querying information and reading data from the image.
	/*!
		\attention Note that this class it NOT thread-safe
	*/
	class AIParsedCDImage : AUTO_INTERFACE
	{
	public:
		//CD/DVD sector size is always 2048 bytes
		virtual unsigned GetSectorCount() = 0;
		virtual unsigned GetTrackCount() = 0;
		virtual unsigned GetTrackEndSector(unsigned TrackNumber) = 0;

		virtual size_t ReadSectorsBoundsChecked(unsigned FirstSector, void *pBuffer, unsigned SectorCount) = 0;
	};

	class AICDImageFormatParser : AUTO_INTERFACE
	{
	public:
		//!  parser with maximum score will be selected. SecondPass is true, when no parsers were found during first pass
		virtual ProbeResult Probe(const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath, bool SecondPass) = 0;
		virtual ManagedPointer<AIParsedCDImage> OpenImage(const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath) = 0;
	};

	class ParsedCDImageBase : public AIParsedCDImage
	{
	protected:
		DECLARE_REFERENCE(AIFile, m_pFile);
		unsigned m_SectorCount;

	protected:
		ParsedCDImageBase(const ConstManagedPointer<AIFile> &pFile, unsigned SectorCount)
			: INIT_REFERENCE(m_pFile, pFile)
			, m_SectorCount(SectorCount)
		{
			ASSERT(m_pFile && m_pFile->Valid());
		}

		void SetSectorCount(unsigned SectorCount)
		{
			ASSERT(!m_SectorCount && SectorCount);
			m_SectorCount = SectorCount;
		}

		inline bool ReadSectorFromFile(ULONGLONG FileOffset, void *pBuffer, size_t BufferOffset, size_t SectorSize = CD_SECTOR_SIZE)
		{
			ASSERT(m_pFile && pBuffer);
			if (m_pFile->Seek(FileOffset, FileFlags::FileBegin) != FileOffset)
				return false;
			return (m_pFile->Read(((char *)pBuffer) + BufferOffset, SectorSize) == SectorSize);
		}


	public:
		virtual unsigned GetSectorCount() {return m_SectorCount;}
		virtual unsigned GetTrackCount() {return 1;}
		virtual unsigned GetTrackEndSector(unsigned TrackNumber) {return m_SectorCount;}

	};

	class ImageFormatDatabase
	{
	private:
		typedef std::map<String, ManagedPointer<AICDImageFormatParser>> ParserMap;
		ParserMap m_RegisteredParsers;

	public:
		ImageFormatDatabase();
		ManagedPointer<AICDImageFormatParser> FindParserByPrimaryFileExtension(const TempString &ParserName);

		ManagedPointer<AIParsedCDImage> OpenCDImage(const ConstManagedPointer<AIFile> &pFile, const String&FileName);
		bool IsAValidCDImageName(const String&filePath);
	};

}
