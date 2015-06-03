#include "stdafx.h"
#include <bzscore/file.h>
#include <bzscore/buffer.h>
#include "CDSpeed.h"
#include <ntddscsi.h>

#define IOCTL_CDROM_BASE                 FILE_DEVICE_CD_ROM

using namespace BazisLib;

#pragma pack(push, sensedata, 1)
typedef struct _SENSE_DATA {
	UCHAR ErrorCode:7;
	UCHAR Valid:1;
	UCHAR SegmentNumber;
	UCHAR SenseKey:4;
	UCHAR Reserved:1;
	UCHAR IncorrectLength:1;
	UCHAR EndOfMedia:1;
	UCHAR FileMark:1;
	UCHAR Information[4];
	UCHAR AdditionalSenseLength;
	UCHAR CommandSpecificInformation[4];
	UCHAR AdditionalSenseCode;
	UCHAR AdditionalSenseCodeQualifier;
	UCHAR FieldReplaceableUnitCode;
	UCHAR SenseKeySpecific[3];
} SENSE_DATA, *PSENSE_DATA;
#pragma pack(pop, sensedata)

typedef enum _FEATURE_NUMBER {
	FeatureProfileList             = 0x0000,
	FeatureCore                    = 0x0001,
	FeatureMorphing                = 0x0002,
	FeatureRemovableMedium         = 0x0003,
	FeatureWriteProtect            = 0x0004,
	// Reserved             0x0005 - 0x000f 
	FeatureRandomReadable          = 0x0010,
	// Reserved             0x0011 - 0x001c 
	FeatureMultiRead               = 0x001d,
	FeatureCdRead                  = 0x001e,
	FeatureDvdRead                 = 0x001f,
	FeatureRandomWritable          = 0x0020,
	FeatureIncrementalStreamingWritable = 0x0021,
	FeatureSectorErasable          = 0x0022,
	FeatureFormattable             = 0x0023,
	FeatureDefectManagement        = 0x0024,
	FeatureWriteOnce               = 0x0025,
	FeatureRestrictedOverwrite     = 0x0026,
	FeatureCdrwCAVWrite            = 0x0027,
	FeatureMrw                     = 0x0028,
	// Reserved                    - 0x0029 
	FeatureDvdPlusRW               = 0x002a,
	// Reserved                    - 0x002b 
	FeatureRigidRestrictedOverwrite = 0x002c,
	FeatureCdTrackAtOnce           = 0x002d,
	FeatureCdMastering             = 0x002e,
	FeatureDvdRecordableWrite      = 0x002f,
	FeatureDDCDRead                = 0x0030,
	FeatureDDCDRWrite              = 0x0031,
	FeatureDDCDRWWrite             = 0x0032,
	// Reserved             0x0030 - 0x00ff 
	FeaturePowerManagement         = 0x0100,
	FeatureSMART                   = 0x0101,
	FeatureEmbeddedChanger         = 0x0102,
	FeatureCDAudioAnalogPlay       = 0x0103,
	FeatureMicrocodeUpgrade        = 0x0104,
	FeatureTimeout                 = 0x0105,
	FeatureDvdCSS                  = 0x0106,
	FeatureRealTimeStreaming       = 0x0107,
	FeatureLogicalUnitSerialNumber = 0x0108,
	// Reserved                    = 0x0109,
	FeatureDiscControlBlocks       = 0x010a,
	FeatureDvdCPRM                 = 0x010b
	// Reserved             0x010c - 0xfeff
	// Vendor Unique        0xff00 - 0xffff
} FEATURE_NUMBER, *PFEATURE_NUMBER;

#define IOCTL_CDROM_GET_CONFIGURATION     CTL_CODE(IOCTL_CDROM_BASE, 0x0016, METHOD_BUFFERED, FILE_READ_ACCESS)

typedef struct _GET_CONFIGURATION_IOCTL_INPUT {
	FEATURE_NUMBER  Feature;
	ULONG  RequestType;
	PVOID  Reserved[2];
} GET_CONFIGURATION_IOCTL_INPUT, *PGET_CONFIGURATION_IOCTL_INPUT;

typedef struct _GET_CONFIGURATION_HEADER {
	UCHAR  DataLength[4];
	UCHAR  Reserved[2];
	UCHAR  CurrentProfile[2];
	UCHAR  Data[0];
} GET_CONFIGURATION_HEADER, *PGET_CONFIGURATION_HEADER;

#define SCSI_GET_CONFIGURATION_REQUEST_TYPE_ALL 0x0

#define SCSI_GET_CONFIGURATION_REQUEST_TYPE_CURRENT 0x1

#define SCSI_GET_CONFIGURATION_REQUEST_TYPE_ONE 0x2

//TODO: refactor, create a "SCSI device accessor" class

std::vector<int> QueryAvailableReadSpeeds(const BazisLib::String &devicePath, bool *pbDVD)
{
	if (pbDVD)
		*pbDVD = false;

	std::vector<int> vec;

	File device(devicePath, FileModes::OpenReadWrite.ShareAll());
	if (!device.Valid())
		return vec;

	TypedBuffer<SCSI_PASS_THROUGH> pPassThrough;
	const int dataBufferSize = 32768;
	pPassThrough.EnsureSizeAndSetUsed(sizeof(SCSI_PASS_THROUGH) + sizeof(SENSE_DATA) + dataBufferSize);
	pPassThrough.Fill();

	pPassThrough->Length = sizeof(SCSI_PASS_THROUGH);
	const unsigned char cdbModeSense[] = {0x5A, 0x80, 0x2A, 0x00, 
									    0x00, 0x00, 0x00, HIBYTE(dataBufferSize),
										LOBYTE(dataBufferSize), 0};
	
	C_ASSERT(sizeof(cdbModeSense) == 10);

	pPassThrough->CdbLength = sizeof(cdbModeSense);
	pPassThrough->DataIn = SCSI_IOCTL_DATA_IN;
	pPassThrough->SenseInfoOffset = sizeof(SCSI_PASS_THROUGH);
	pPassThrough->SenseInfoLength = sizeof(SENSE_DATA);
	pPassThrough->DataBufferOffset = sizeof(SCSI_PASS_THROUGH) + sizeof(SENSE_DATA);
	pPassThrough->DataTransferLength = dataBufferSize;
	pPassThrough->TimeOutValue = 2;
	memcpy(pPassThrough->Cdb, &cdbModeSense, sizeof(cdbModeSense));

	ActionStatus st;
	device.DeviceIoControl(IOCTL_SCSI_PASS_THROUGH, pPassThrough, (DWORD)pPassThrough.GetSize(), pPassThrough, (DWORD)pPassThrough.GetSize(), &st);
	if (!st.Successful())
		return vec;

	if (pPassThrough->DataTransferLength < 32)
		return vec;

	unsigned char *pBuf = ((unsigned char *)pPassThrough.GetData()) + pPassThrough->DataBufferOffset + 8;

	if ((pBuf[0] & 0x3F) != 0x2A)
		return vec;

	size_t currentSpeed = MAKEWORD(pBuf[29], pBuf[28]);
	size_t descCount = MAKEWORD(pBuf[31], pBuf[30]);

	for (size_t i = 0; i < descCount; i++)
		vec.push_back(MAKEWORD(pBuf[35 + i * 4], pBuf[34 + i * 4]));

	if (pbDVD)
	{
		TypedBuffer<GET_CONFIGURATION_HEADER> pConf;
		pConf.EnsureSizeAndSetUsed(32768);
		GET_CONFIGURATION_IOCTL_INPUT getCfgInput = {FeatureProfileList,};
		getCfgInput.RequestType = SCSI_GET_CONFIGURATION_REQUEST_TYPE_CURRENT;

		size_t done = device.DeviceIoControl(IOCTL_CDROM_GET_CONFIGURATION, &getCfgInput, sizeof(getCfgInput), pConf, pConf.GetAllocated());
		if (done >= sizeof(GET_CONFIGURATION_HEADER))
		{
			USHORT currentProfile = MAKEWORD(pConf->CurrentProfile[1], pConf->CurrentProfile[0]);
			*pbDVD = (currentProfile >= 0x10);
		}
	}
	
	return vec;
}

BazisLib::ActionStatus SetCDReadSpeed(const BazisLib::String &devicePath, unsigned short readSpeed)
{
	ActionStatus st;
	File device(devicePath, FileModes::OpenReadWrite.ShareAll(), &st);
	if (!st.Successful())
		return st;

	TypedBuffer<SCSI_PASS_THROUGH> pPassThrough;
	const int dataBufferSize = 0;
	pPassThrough.EnsureSizeAndSetUsed(sizeof(SCSI_PASS_THROUGH) + sizeof(SENSE_DATA) + dataBufferSize);
	pPassThrough.Fill();

	pPassThrough->Length = sizeof(SCSI_PASS_THROUGH);
	const unsigned char cdbSetSpeed[] = {0xBB, 0x00, HIBYTE(readSpeed), LOBYTE(readSpeed), 
		0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	C_ASSERT(sizeof(cdbSetSpeed) == 12);

	pPassThrough->CdbLength = sizeof(cdbSetSpeed);
	pPassThrough->DataIn = SCSI_IOCTL_DATA_IN;
	pPassThrough->SenseInfoOffset = sizeof(SCSI_PASS_THROUGH);
	pPassThrough->SenseInfoLength = sizeof(SENSE_DATA);
	pPassThrough->DataBufferOffset = sizeof(SCSI_PASS_THROUGH) + sizeof(SENSE_DATA);
	pPassThrough->DataTransferLength = dataBufferSize;
	pPassThrough->TimeOutValue = 2;
	memcpy(pPassThrough->Cdb, &cdbSetSpeed, sizeof(cdbSetSpeed));

	device.DeviceIoControl(IOCTL_SCSI_PASS_THROUGH, pPassThrough, (DWORD)pPassThrough.GetSize(), pPassThrough, (DWORD)pPassThrough.GetSize(), &st);
	if (!st.Successful())
		return st;
	
	if (pPassThrough->ScsiStatus == 0) //SCSISTAT_GOOD
		return MAKE_STATUS(Success);
	else
		return MAKE_STATUS(UnknownError);
}