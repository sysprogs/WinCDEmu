#include "stdafx.h"
#include "ImageFormats.h"
#include "IMGParser.h"
#include "NRG/NRGParser.h"
#include "MDS/MDSParser.h"
#include "CUEParser.h"
#include "CCDParser.h"

using namespace ImageFormats;
using namespace BazisLib;

ImageFormatDatabase::ImageFormatDatabase()
{
	//m_RegisteredParsers[_T("iso")] = new ISOParser(); //IMG parser is more accurate
	m_RegisteredParsers[_T("img")] = new IMGParser();
	m_RegisteredParsers[_T("nrg")] = new NRGParser();
	m_RegisteredParsers[_T("mds")] = new MDSParser();
	m_RegisteredParsers[_T("cue")] = new CUEParser();
	m_RegisteredParsers[_T("ccd")] = new CCDParser();
}

ManagedPointer<AICDImageFormatParser> ImageFormatDatabase::FindParserByPrimaryFileExtension( const TempString &ParserName )
{
	ParserMap::iterator it = m_RegisteredParsers.find(ParserName);
	if (it == m_RegisteredParsers.end())
		return NULL;
	return it->second;
}

ManagedPointer<AIParsedCDImage> ImageFormatDatabase::OpenCDImage( const ConstManagedPointer<AIFile> &pFile, const String&FileName )
{
	if (!pFile || !pFile->Valid())
		return NULL;
	ManagedPointer<AICDImageFormatParser> pParser;
	ProbeResult BestScore = NoMatch;
	TempString ext = Path::GetExtensionExcludingDot(FileName);

	//TODO: Implement third pass trying to detect internal FS structures
	for (int pass = 0; pass < 2; pass++)
	{
		for (ParserMap::iterator it = m_RegisteredParsers.begin(); it != m_RegisteredParsers.end(); it++)
		{
			ProbeResult score = it->second->Probe(ext, pFile, FileName, (pass != 0));
			if (score > BestScore)
			{
				BestScore = score;
				pParser = it->second;
			}
		}

		if (pParser)
		{
			if (pass)
				return pParser->OpenImage(ext, pFile, FileName);

			//Decide whether we need a second format detection pass
			ProbeResult score2 = pParser->Probe(ext, pFile, FileName, true);
			if (score2 >= SufficientMatchToSkipOthers)
				return pParser->OpenImage(ext, pFile, FileName);
		}
	}

	return NULL;
}

bool ImageFormats::ImageFormatDatabase::IsAValidCDImageName( const String&filePath )
{
	TempString ext = Path::GetExtensionExcludingDot(filePath);

	for (ParserMap::iterator it = m_RegisteredParsers.begin(); it != m_RegisteredParsers.end(); it++)
	{
		if (it->second->Probe(ext, ManagedPointer<AIFile>(NULL), filePath, false) >= NameOnlyMatch)
			return true;
	}
	return false;
}