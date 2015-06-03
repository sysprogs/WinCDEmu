#include "stdafx.h"
#include "CUEParser.h"
#include "RawCDFormats.h"
#include <bzshlp/textfile.h>
#include "IMGParser.h"

using namespace BazisLib;
using namespace ImageFormats;

ActionStatus ParseCUEFile(const String& OriginalPath, String *pBinaryPath, const ManagedPointer<AIFile> &pFile, RawCDFormatDescriptor *pDesc)
{
	if (!pFile || !pDesc || !pBinaryPath)
		return MAKE_STATUS(InvalidParameter);
	if (pFile->GetSize() >= 1024)
		return MAKE_STATUS(ParsingFailed);
	pFile->Seek(0, FileFlags::FileBegin);
	ManagedPointer<TextANSIUNIXFileReader> pReader = new TextANSIUNIXFileReader(pFile);
	if (!pReader)
		return MAKE_STATUS(UnknownError);
	DynamicStringA str = pReader->ReadLine().Strip("\r");
	while ((str.length() < 4) || (memcmp(str.c_str(), "FILE", 4)))
	{
		if (pReader->IsEOF())
			break;
		str = pReader->ReadLine().Strip("\r");
	}
	FastStringRoutines::_SplitConfigLineT<3, TempStringA, false> firstLine(str);
	if (firstLine.count() < 3)
		return MAKE_STATUS(ParsingFailed);

	if (firstLine[0].icompare("FILE") || firstLine[2].icompare("BINARY"))
		return MAKE_STATUS(ParsingFailed);

	String fp(ANSIStringToString(firstLine[1]));

	String imgDir = Path::GetDirectoryName(OriginalPath);
	*pBinaryPath = Path::Combine(imgDir, fp);

	if (!File::Exists(*pBinaryPath))
	{
		*pBinaryPath = Path::Combine(imgDir, Path::GetFileName(fp));
		if (!File::Exists(*pBinaryPath))
		{
			*pBinaryPath = Path::Combine(Path::GetPathWithoutExtension(OriginalPath), _T(".") + Path::GetExtensionExcludingDot(fp));
			if (!File::Exists(*pBinaryPath))
			{
				Directory dir(imgDir);
				bool found = false;
				for (Directory::enumerator iter = dir.FindFirstFile(Path::GetPathWithoutExtension(OriginalPath) + L".*"); iter.Valid(); iter.Next())
				{
					if (iter.GetSize() > (1024 * 1024))
					{
						*pBinaryPath = iter.GetFullPath();
						found = true;
						break;
					}
				}

				if (!found)
					return MAKE_STATUS(FileNotFound);
			}
		}
	}

	unsigned idx = 0;
	while (!pReader->IsEOF() && (idx < 10))
	{
		str = pReader->ReadLine().Strip("\r");
		FastStringRoutines::_SplitConfigLineT<3, TempStringA> trackLine(str);
		if (trackLine.count() < 3)
			continue;
		if (trackLine[0].icompare("TRACK") || (atoi(trackLine[1].GetConstBuffer()) != 1))
			continue;
		
		TempStringA mode = trackLine[2];
		off_t slash = mode.find('/');
		if (slash == -1)
			return MAKE_STATUS(ParsingFailed);
		pDesc->SectorSize = atoi(mode.substr(slash + 1).GetConstBuffer());
		pDesc->InitialOffset = 0;
		if (mode == "MODE1/2352")
			pDesc->InitialOffset = 16;
		else if (mode == "MODE2/2352")
			pDesc->InitialOffset = 24;
		else if (mode == "MODE2/2336")
			pDesc->InitialOffset = 16;
		return MAKE_STATUS(Success);
	}
	return MAKE_STATUS(ParsingFailed);
}


ImageFormats::ProbeResult ImageFormats::CUEParser::Probe( const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath, bool SecondPass )
{
	bool nameMatch = !FileExtension.icompare(_T("cue"));
	if (!SecondPass)
		return nameMatch ? NameOnlyMatch : NoMatch;
	else
	{
		ASSERT(pFile);
		String binPath (L"");
		RawCDFormatDescriptor format;
		ActionStatus st = ParseCUEFile(FullFilePath, &binPath, pFile, &format);
		ProbeResult ret = nameMatch ? NameOnlyMatch : NoMatch;
		if (!st.Successful())
			return NoMatch;
		return (ProbeResult)(ret | FormatOnlyMatch);
	}
}

ManagedPointer<AIParsedCDImage> ImageFormats::CUEParser::OpenImage( const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath )
{
	String binPath (L"");
	RawCDFormatDescriptor format;
	if (!ParseCUEFile(FullFilePath, &binPath, pFile, &format).Successful())
		return NULL;
	ManagedPointer<AIFile> pImageFile = new ACFile(binPath, FileModes::OpenReadOnly.ShareAll());
	if (!pImageFile || !pImageFile->Valid())
		return NULL;
	format.InitialOffset = DetectDataOffsetInSector(pImageFile, format.SectorSize, format.InitialOffset);
	return new IMGImage(pImageFile, &format);
}