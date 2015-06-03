#pragma once

//Internal CD filesystem structures. Copied from WDK\src\filesys\cdfs\Win7\cd.h

#define FIRST_VD_SECTOR             16

CHAR CdHsgId[] = "CDROM";
CHAR CdIsoId[] = "CD001";
CHAR CdXaId[] = "CD-XA001";

#define LEN_ROOT_DE                 (34)

typedef struct _RAW_ISO_VD {

    UCHAR       DescType;           // volume type: 1 = standard, 2 = coded
    UCHAR       StandardId[5];      // volume structure standard id = CD001
    UCHAR       Version;            // volume structure version number = 1
    UCHAR       VolumeFlags;        // volume flags
    UCHAR       SystemId[32];       // system identifier
    UCHAR       VolumeId[32];       // volume identifier
    UCHAR       Reserved[8];        // reserved 8 = 0
    ULONG       VolSpaceI;          // size of the volume in LBN's Intel
    ULONG       VolSpaceM;          // size of the volume in LBN's Motorola
    UCHAR       CharSet[32];        // character set bytes 0 = ASCII
    USHORT      VolSetSizeI;        // volume set size Intel
    USHORT      VolSetSizeM;        // volume set size Motorola
    USHORT      VolSeqNumI;         // volume set sequence number Intel
    USHORT      VolSeqNumM;         // volume set sequence number Motorola
    USHORT      LogicalBlkSzI;      // logical block size Intel
    USHORT      LogicalBlkSzM;      // logical block size Motorola
    ULONG       PathTableSzI;       // path table size in bytes Intel
    ULONG       PathTableSzM;       // path table size in bytes Motorola
    ULONG       PathTabLocI[2];     // LBN of 2 path tables Intel
    ULONG       PathTabLocM[2];     // LBN of 2 path tables Motorola
    UCHAR       RootDe[LEN_ROOT_DE];// dir entry of the root directory
    UCHAR       VolSetId[128];      // volume set identifier
    UCHAR       PublId[128];        // publisher identifier
    UCHAR       PreparerId[128];    // data preparer identifier
    UCHAR       AppId[128];         // application identifier
    UCHAR       Copyright[37];      // file name of copyright notice
    UCHAR       Abstract[37];       // file name of abstract
    UCHAR       Bibliograph[37];    // file name of bibliography
    UCHAR       CreateDate[17];     // volume creation date and time
    UCHAR       ModDate[17];        // volume modification date and time
    UCHAR       ExpireDate[17];     // volume expiration date and time
    UCHAR       EffectDate[17];     // volume effective date and time
    UCHAR       FileStructVer;      // file structure version number = 1
    UCHAR       Reserved3;          // reserved
    UCHAR       ResApp[512];        // reserved for application
    UCHAR       Reserved4[653];     // remainder of 2048 bytes reserved

} RAW_ISO_VD;
typedef RAW_ISO_VD *PRAW_ISO_VD;


typedef struct _RAW_HSG_VD {

    ULONG       BlkNumI;            // logical block number Intel
    ULONG       BlkNumM;            // logical block number Motorola
    UCHAR       DescType;           // volume type: 1 = standard, 2 = coded
    UCHAR       StandardId[5];      // volume structure standard id = CDROM
    UCHAR       Version;            // volume structure version number = 1
    UCHAR       VolumeFlags;        // volume flags
    UCHAR       SystemId[32];       // system identifier
    UCHAR       VolumeId[32];       // volume identifier
    UCHAR       Reserved[8];        // reserved 8 = 0
    ULONG       VolSpaceI;          // size of the volume in LBN's Intel
    ULONG       VolSpaceM;          // size of the volume in LBN's Motorola
    UCHAR       CharSet[32];        // character set bytes 0 = ASCII
    USHORT      VolSetSizeI;        // volume set size Intel
    USHORT      VolSetSizeM;        // volume set size Motorola
    USHORT      VolSeqNumI;         // volume set sequence number Intel
    USHORT      VolSeqNumM;         // volume set sequence number Motorola
    USHORT      LogicalBlkSzI;      // logical block size Intel
    USHORT      LogicalBlkSzM;      // logical block size Motorola
    ULONG       PathTableSzI;       // path table size in bytes Intel
    ULONG       PathTableSzM;       // path table size in bytes Motorola
    ULONG       PathTabLocI[4];     // LBN of 4 path tables Intel
    ULONG       PathTabLocM[4];     // LBN of 4 path tables Motorola
    UCHAR       RootDe[LEN_ROOT_DE];// dir entry of the root directory
    UCHAR       VolSetId[128];      // volume set identifier
    UCHAR       PublId[128];        // publisher identifier
    UCHAR       PreparerId[128];    // data preparer identifier
    UCHAR       AppId[128];         // application identifier
    UCHAR       Copyright[32];      // file name of copyright notice
    UCHAR       Abstract[32];       // file name of abstract
    UCHAR       CreateDate[16];     // volume creation date and time
    UCHAR       ModDate[16];        // volume modification date and time
    UCHAR       ExpireDate[16];     // volume expiration date and time
    UCHAR       EffectDate[16];     // volume effective date and time
    UCHAR       FileStructVer;      // file structure version number
    UCHAR       Reserved3;          // reserved
    UCHAR       ResApp[512];        // reserved for application
    UCHAR       Reserved4[680];     // remainder of 2048 bytes reserved

} RAW_HSG_VD;
typedef RAW_HSG_VD *PRAW_HSG_VD;


typedef struct _RAW_JOLIET_VD {

    UCHAR       DescType;           // volume type: 2 = coded
    UCHAR       StandardId[5];      // volume structure standard id = CD001
    UCHAR       Version;            // volume structure version number = 1
    UCHAR       VolumeFlags;        // volume flags
    UCHAR       SystemId[32];       // system identifier
    UCHAR       VolumeId[32];       // volume identifier
    UCHAR       Reserved[8];        // reserved 8 = 0
    ULONG       VolSpaceI;          // size of the volume in LBN's Intel
    ULONG       VolSpaceM;          // size of the volume in LBN's Motorola
    UCHAR       CharSet[32];        // character set bytes 0 = ASCII, Joliett Seq here
    USHORT      VolSetSizeI;        // volume set size Intel
    USHORT      VolSetSizeM;        // volume set size Motorola
    USHORT      VolSeqNumI;         // volume set sequence number Intel
    USHORT      VolSeqNumM;         // volume set sequence number Motorola
    USHORT      LogicalBlkSzI;      // logical block size Intel
    USHORT      LogicalBlkSzM;      // logical block size Motorola
    ULONG       PathTableSzI;       // path table size in bytes Intel
    ULONG       PathTableSzM;       // path table size in bytes Motorola
    ULONG       PathTabLocI[2];     // LBN of 2 path tables Intel
    ULONG       PathTabLocM[2];     // LBN of 2 path tables Motorola
    UCHAR       RootDe[LEN_ROOT_DE];// dir entry of the root directory
    UCHAR       VolSetId[128];      // volume set identifier
    UCHAR       PublId[128];        // publisher identifier
    UCHAR       PreparerId[128];    // data preparer identifier
    UCHAR       AppId[128];         // application identifier
    UCHAR       Copyright[37];      // file name of copyright notice
    UCHAR       Abstract[37];       // file name of abstract
    UCHAR       Bibliograph[37];    // file name of bibliography
    UCHAR       CreateDate[17];     // volume creation date and time
    UCHAR       ModDate[17];        // volume modification date and time
    UCHAR       ExpireDate[17];     // volume expiration date and time
    UCHAR       EffectDate[17];     // volume effective date and time
    UCHAR       FileStructVer;      // file structure version number = 1
    UCHAR       Reserved3;          // reserved
    UCHAR       ResApp[512];        // reserved for application
    UCHAR       Reserved4[653];     // remainder of 2048 bytes reserved

} RAW_JOLIET_VD;
typedef RAW_JOLIET_VD *PRAW_JOLIET_VD;