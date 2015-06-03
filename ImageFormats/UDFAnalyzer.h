#pragma once
#include "ImageFormats.h"

struct UDFAnalysisResult
{
	bool foundBDMV;
	bool foundVIDEO_TS;
	bool isUDF;
};

UDFAnalysisResult AnalyzeUDFImage(const BazisLib::ManagedPointer<ImageFormats::AIParsedCDImage> &pImg);