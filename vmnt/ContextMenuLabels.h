#pragma once
#include <bzshlp/Win32/i18n.h>

#ifdef BAZISLIB_LOCALIZATION_ENABLED
#include "vmnt_lng.h"
#endif

//We define it here so that the LNG file builder will recognize the _TR statements
static inline const TCHAR *txtSelectAndMount() {return _TR(IDS_SELECTANDMOUNT, "Select drive letter && mount");}
static inline const TCHAR *txtCreateISO() { return _TR(IDS_CREATEISO, "Create an ISO image"); }
static inline const TCHAR *txtBuildISO() { return _TR(IDS_BUILDISO, "Build an ISO image"); }
static inline const TCHAR *txtSelectAnotherImage() { return _TR(IDS_MOUNTANOTHERIMAGE, "Mount another disc image"); }
static inline const TCHAR *txtMountToExistingDrive() { return _TR(IDS_MOUNTTODRIVE, "Mount to an existing virtual drive"); }
