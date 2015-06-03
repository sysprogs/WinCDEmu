#include "../wcdversion.h"

VS_VERSION_INFO VERSIONINFO
 FILEVERSION WINCDEMU_VERSION
 PRODUCTVERSION WINCDEMU_VERSION
 FILEFLAGSMASK 0x17L
#ifdef _DEBUG
 FILEFLAGS 0x1L
#else
 FILEFLAGS 0x0L
#endif
 FILEOS 0x4L
 FILETYPE 0x1L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "000004b0"
        BEGIN
            VALUE "Comments", COMMENTS_STR
            VALUE "CompanyName", COMPANY_STR
            VALUE "FileDescription", "WinCDEmu mounter"
            VALUE "FileVersion", WINCDEMU_VERSION_STR
            VALUE "LegalCopyright", COPYRIGHT_STR
            VALUE "LegalTrademarks", TRADEMARKS_STR
            VALUE "OriginalFilename", "vmnt.exe"
            VALUE "ProductName", PRODUCTNAME_STR
            VALUE "ProductVersion", WINCDEMU_VERSION_STR
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x0, 1200
    END
END

