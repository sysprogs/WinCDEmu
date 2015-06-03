#pragma once

#include <bzshlp/Win32/RegistrySerializer.h>

enum DriverLetterMode
{
	drvlAutomatic,
	drvlStartingFromGiven,
	drvlAskEveryTime,
};

static const LPCTSTR lpSupportedExtensions [] = { _T("iso"), _T("cue"), _T("img"), _T("nrg"), _T("mds"), _T("ccd") };

DECLARE_SERIALIZEABLE_STRUC6_I(_RegistryParamsBase,
							   unsigned, DriveLetterPolicy, drvlAskEveryTime,
							   unsigned, StartingDriveLetterIndex, ('V' - 'A'),
							   bool, DisableAutorun, false,
							   bool, PersistentDefault, false,
							   unsigned, Language, 0,
							   unsigned, DefaultMMCProfile, 0);

DECLARE_SERIALIZEABLE_STRUC5_I(_ISOCreatorRegistryParamsBase,
								bool, CloseWindowAutomatically, false,
								bool, EjectDiscAutomatically, false,
								bool, MountISOAutomatically, false,
								bool, OpenFolderAutomatically, false,
								int, ReadBufferSize, 0);

namespace
{
	TCHAR tszWinCDEmuRegistryPath[] = _T("SOFTWARE\\SysProgs\\WinCDEmu");
	TCHAR tszWinCDEmuISOCreatorRegistryPath[] = _T("SOFTWARE\\SysProgs\\WinCDEmu\\ISOCreator");
}

template <class _SerializeableStructure, LPCTSTR lpRegistryPath, HKEY hRootKey = HKEY_LOCAL_MACHINE, bool AutoLoad = true> class _RegistryParametersT : public _SerializeableStructure
{
public:
	_RegistryParametersT()
	{
		if (AutoLoad)
			LoadFromRegistry();
	}

	void LoadFromRegistry()
	{
		BazisLib::Win32::RegistryKey key(hRootKey, lpRegistryPath);
		BazisLib::Win32::RegistrySerializer::Deserialize(key, *this);
	}

	void SaveToRegistry()
	{
		BazisLib::Win32::RegistryKey key(hRootKey, lpRegistryPath);
		BazisLib::Win32::RegistrySerializer::Serialize(key, *this);
	}
};

typedef _RegistryParametersT<_RegistryParamsBase, tszWinCDEmuRegistryPath, HKEY_CURRENT_USER> RegistryParams;
typedef _RegistryParametersT<_ISOCreatorRegistryParamsBase, tszWinCDEmuISOCreatorRegistryPath, HKEY_CURRENT_USER> ISOCreatorRegistryParams;
