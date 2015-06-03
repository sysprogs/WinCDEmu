#pragma once
#include "../ImageFormats.h"

namespace ImageFormats
{
	class MDSParser : public AICDImageFormatParser
	{
	public:
		virtual ProbeResult Probe(const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath, bool SecondPass);
		virtual ManagedPointer<AIParsedCDImage> OpenImage(const TempString &FileExtension, const ConstManagedPointer<AIFile> &pFile, const String&FullFilePath);
	};
}