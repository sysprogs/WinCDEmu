#include "stdafx.h"
#include "PersistentImageDatabase.h"
#include <bzshlp/WinKernel/registry.h>

using namespace BazisLib;
using namespace BazisLib::DDK;

static const wchar_t wszImagePathKey[] = L"ImagePath";
static const wchar_t wszSuggestedLetterKey[] = L"DriveLetter";
static const wchar_t wszMMCProfile[] = L"MMCProfile";
static const wchar_t wszLastMountStatusKey[] = L"LastAutoMountStatus";

String PersistentImageDatabase::AddImage(const wchar_t *pImage, char suggestedDriveLetter, unsigned MMCProfile)
{
	MutexLocker lck(m_Mutex);
	if (m_RegPath.empty())
		return String();

	RegistryKey key(m_RegPath.c_str());
	if (!key.Valid())
		return String();

	String subkeyName;

	for (int i = 0; i < 1000; i++)
	{
		subkeyName = key.EnumSubkey(i);
		if (subkeyName.empty())
			break;

		RegistryKey subKey = key[subkeyName.c_str()];
		if (!((String)subKey[wszImagePathKey]).icompare(pImage))
			break;
	}

	if (subkeyName.empty())
		for (int i = 0; i < 1000; i++)
		{
			wchar_t wszSubkey[32];
			swprintf(wszSubkey, L"%02d", i);
			RegistryKey subkey(key, wszSubkey, true);
			if (subkey.Valid())
				continue;

			subkeyName = wszSubkey;
			break;
		}


	RegistryKey subKey = key[subkeyName.c_str()];
	subKey[wszImagePathKey] = pImage;
	wchar_t wchLetter = suggestedDriveLetter;
	subKey[wszSuggestedLetterKey] = String(&wchLetter, 1);
	subKey[wszMMCProfile] = MMCProfile;

	return subkeyName;
}

std::list<PersistentImageDatabase::ImageRecord> PersistentImageDatabase::LoadImageList()
{
	MutexLocker lck(m_Mutex);
	std::list<ImageRecord> records;
	if (m_RegPath.empty())
		return records;

	RegistryKey key(m_RegPath.c_str());
	if (!key.Valid())
		return records;

	String subkeyName;

	for (int i = 0; i < 1000; i++)
	{
		subkeyName = key.EnumSubkey(i);
		if (subkeyName.empty())
			break;

		RegistryKey subKey = key[subkeyName.c_str()];
		ImageRecord rec;

		rec.ImagePath = subKey[wszImagePathKey];
		rec.Key = subkeyName;
		rec.DriveLetter = 0;
		String drvLetter = subKey[wszSuggestedLetterKey];
		if (drvLetter.length() >= 1)
			rec.DriveLetter = (char)drvLetter[0];

		rec.MMCProfile = subKey[wszMMCProfile];

		records.push_back(rec);
	}

	return records;
}

bool PersistentImageDatabase::RemoveImageByKey(const String &Key)
{
	MutexLocker lck(m_Mutex);
	if (m_RegPath.empty())
		return false;

	RegistryKey key(m_RegPath.c_str());
	if (!key.Valid())
		return false;

	RegistryKey subKey = key[Key.c_str()];
	return subKey.DeleteSelf();
}

void PersistentImageDatabase::ReportMountStatus(const BazisLib::String &Key, NTSTATUS status)
{
	MutexLocker lck(m_Mutex);
	if (m_RegPath.empty())
		return;

	RegistryKey key(m_RegPath.c_str());
	if (!key.Valid())
		return;

	RegistryKey subKey = key[Key.c_str()];
	subKey[wszLastMountStatusKey] = status;
}