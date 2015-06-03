#pragma once
#include <bzscore/string.h>
#include <bzscore/objmgr.h>
#include <bzscore/sync.h>
#include <list>

class PersistentImageDatabase : AUTO_OBJECT
{
private:
	BazisLib::String m_RegPath;
	BazisLib::Mutex m_Mutex;

public:
	void SetRegistryPath(const BazisLib::TempString &regPath)
	{
		m_RegPath = regPath;
	}

	BazisLib::String AddImage(const wchar_t *pImage, char suggestedDriveLetter, unsigned MMCProfile);
	bool RemoveImageByKey(const BazisLib::String &Key);

	struct ImageRecord
	{
		BazisLib::String Key, ImagePath;
		char DriveLetter;
		unsigned short MMCProfile;
	};

	std::list<ImageRecord> LoadImageList();
	void ReportMountStatus(const BazisLib::String &Key, NTSTATUS status);

};

