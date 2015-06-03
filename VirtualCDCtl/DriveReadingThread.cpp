#include "stdafx.h"
#include "DriveReadingThread.h"

using namespace BazisLib;
enum {kCDSectorSize = 2048};

#pragma region CDROM_TOC definition - imported from ntddcdrm.h
#define MAXIMUM_NUMBER_TRACKS 100
typedef struct _TRACK_DATA {
	UCHAR Reserved;
	UCHAR Control : 4;
	UCHAR Adr : 4;
	UCHAR TrackNumber;
	UCHAR Reserved1;
	UCHAR Address[4];
} TRACK_DATA, *PTRACK_DATA;

typedef struct _CDROM_TOC {

	//
	// Header
	//

	UCHAR Length[2];  // add two bytes for this field
	UCHAR FirstTrack;
	UCHAR LastTrack;

	//
	// Track data
	//

	TRACK_DATA TrackData[MAXIMUM_NUMBER_TRACKS];
} CDROM_TOC, *PCDROM_TOC;

typedef struct _CDROM_READ_TOC_EX {
	UCHAR Format    : 4;
	UCHAR Reserved1 : 3; // future expansion
	UCHAR Msf       : 1;
	UCHAR SessionTrack;
	UCHAR Reserved2;     // future expansion
	UCHAR Reserved3;     // future expansion
} CDROM_READ_TOC_EX, *PCDROM_READ_TOC_EX;

#define CDROM_TOC_SIZE sizeof(CDROM_TOC)
#define IOCTL_CDROM_BASE                 FILE_DEVICE_CD_ROM
#define IOCTL_CDROM_READ_TOC              CTL_CODE(IOCTL_CDROM_BASE, 0x0000, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_CDROM_READ_TOC_EX           CTL_CODE(IOCTL_CDROM_BASE, 0x0015, METHOD_BUFFERED, FILE_READ_ACCESS)
#define CDROM_READ_TOC_EX_FORMAT_TOC      0x00

#pragma endregion

BackgroundDriveReader::BackgroundDriveReader(const BazisLib::String &drivePath, size_t bufferSize, ActionStatus *pStatus) 
	: m_File(drivePath, BazisLib::FileModes::OpenReadOnly.ShareAll(), pStatus)
	, m_SourcePath(drivePath)
	, m_BufferSize(bufferSize)
	, m_bAbort(false)
	, m_BytesDone(0)
	, m_BytesDoneOnLastRemainingTimeReset(0)
	, m_bPaused(false)
	, m_ContinueEvent(true, kManualResetEvent)
	, m_pErrorHandler(NULL)
	, m_QueuedBadSectors(0)
	, m_ProcessedBadSectors(0)
	, m_bCancelBadSectorReReading(false)
{
	ASSERT(pStatus);
	if (!pStatus->Successful())
		return;

	GET_LENGTH_INFORMATION getLengthInfo = {0,};
	DWORD done = m_File.DeviceIoControl(IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &getLengthInfo, sizeof(getLengthInfo), pStatus);
	if (done != sizeof(getLengthInfo))
	{
		if (pStatus->Successful())
			ASSIGN_STATUS(pStatus, UnknownError);
		return;
	}

	m_VolumeSize = getLengthInfo.Length.QuadPart - 150 * 2048;	//Read() at offset 0 corresponds to sector #150. So, the last readable sector is 150 sectors before the m_VolumeSize.
	m_FinalStatus = MAKE_STATUS(Pending);
}

BazisLib::ActionStatus BackgroundDriveReader::CheckTOC(TOCError *pAdditionalError)
{
	CDROM_TOC toc = {0, };
	ActionStatus st;
	size_t minTOCSize = FIELD_OFFSET(CDROM_TOC, TrackData) + (sizeof(TRACK_DATA) * 2);
	bool bMSF = false;
	CDROM_READ_TOC_EX tocRq = {0, };
	tocRq.Format = CDROM_READ_TOC_EX_FORMAT_TOC;
	tocRq.Msf = false;

	if (m_File.DeviceIoControl(IOCTL_CDROM_READ_TOC_EX, &tocRq, sizeof(tocRq), &toc, sizeof(toc), &st) < minTOCSize)
	{
		bMSF = true;
		if (m_File.DeviceIoControl(IOCTL_CDROM_READ_TOC, NULL, 0, &toc, sizeof(toc), &st) < minTOCSize)
		{
			*pAdditionalError = teCannotReadTOC;
			if (st.Successful())
				return MAKE_STATUS(UnknownError);
			else
				return st;
		}
	}

	size_t tocLen = MAKEWORD(toc.Length[1], toc.Length[0]);
	if ((tocLen + 2) < minTOCSize)
	{
		*pAdditionalError = teCannotReadTOC;
		return MAKE_STATUS(InvalidParameter);
	}

	if ((toc.TrackData[0].Adr != 0x01) || (toc.TrackData[0].Control != 0x04))
	{
		*pAdditionalError = teNonDataTrack;
		return MAKE_STATUS(OperationAborted);
	}

	if (toc.TrackData[1].TrackNumber != 0xAA)
	{
		*pAdditionalError = teMultipleTracks;
		return MAKE_STATUS(OperationAborted);
	}

	/*if (!bMSF)
	{
		unsigned endOfTrackAddr = (toc.TrackData[1].Address[0] << 24) | (toc.TrackData[1].Address[1] << 16) | (toc.TrackData[1].Address[2] << 8) | toc.TrackData[1].Address[3];
		MessageBox(0,String::sFormat(L"Adjusted volume size = %I64d (%d sectors)\nEnd of track 1: %02X:%02X:%02X.%02X (%d sectors)",
			m_VolumeSize,
			(int)(m_VolumeSize / 2048),
			toc.TrackData[1].Address[0] & 0xFF,
			toc.TrackData[1].Address[1] & 0xFF,
			toc.TrackData[1].Address[2] & 0xFF,
			toc.TrackData[1].Address[3] & 0xFF, endOfTrackAddr).c_str(), L"Info", MB_ICONINFORMATION);

	}*/

	return MAKE_STATUS(Success);
}

int BackgroundDriveReader::ThreadBody()
{
	m_FinalStatus = DoMainReadLoop();

	if (m_FinalStatus.Successful() && !m_BadSectors.Empty())
	{
		bool bRepeatInfinitely = false;
		for (IErrorHandler::BadSectorsAction action = IErrorHandler::Ignore;;)
		{
			if (!bRepeatInfinitely)
			{
				MutexLocker lck(m_MainMutex);
				if (m_pErrorHandler)
					action = m_pErrorHandler->OnBadSectorsFound(&m_BadSectors);
			}

			if (action == IErrorHandler::Ignore)
				break;
			else if (action == IErrorHandler::RereadContinuous)
				bRepeatInfinitely = true;

			m_FinalStatus = ReReadBadSectors();
			//TODO: if we return from here, set m_QueuedBadSectors to 0
			if (m_bCancelBadSectorReReading)
				break;

			if (!m_FinalStatus.Successful())
				break;
			if (m_BadSectors.Empty())
				break;
		}
	}
	m_QueuedBadSectors = 0;

	MutexLocker lck(m_MainMutex);
	m_File.Close();
	m_pTargetFile->Close();
	return !m_FinalStatus.Successful();
}

ActionStatus BackgroundDriveReader::DoMainReadLoop()
{
	if (m_BufferSize == 0)
		m_BufferSize = kDefaultReadBufferSize;

	CBuffer buf(m_BufferSize);
	if (!buf.GetData())
		return MAKE_STATUS(NoMemory);

	m_BytesDone = 0;

	bool bPerSectorMode = false;
	ULONGLONG endOfFailedBlock = 0;

	while (!m_bAbort)
	{
		ULONGLONG todo = m_VolumeSize - m_BytesDone;
		if (todo > buf.GetAllocated())
			todo = buf.GetAllocated();

		if (bPerSectorMode)
			if (todo > kCDSectorSize)
				todo = kCDSectorSize;

		if (!todo)
			return MAKE_STATUS(Success);

		ActionStatus st;

		if (bPerSectorMode)
			if (m_File.Seek(m_BytesDone, FileFlags::FileBegin, &st) == m_BytesDone)
				st = MAKE_STATUS(Success);

		size_t rdone = 0;
		if (!bPerSectorMode || st.Successful())
		{
			st = ActionStatus();
			rdone = m_File.Read(buf.GetData(), (size_t)todo, &st);
		}

		if (m_bAbort)
			return MAKE_STATUS(OperationAborted);

		if (rdone != (size_t)todo)
		{
			if (!bPerSectorMode)
			{
				bPerSectorMode = true;
				endOfFailedBlock = m_BytesDone + todo;

				if (!rdone)
					continue;
			}
			else
			{
				//We are already in the per-sector mode (1 sector is read per 1 request).
				//If read in this mode failed, we remember the failed block and continue.
				
				ASSERT(!rdone);	//Successfully reading less than a full CD sector?

				if (!m_BytesDone)
					return st;	//First sector unreadable - aborting read

				m_File.Seek(m_BytesDone + kCDSectorSize, FileFlags::FileBegin);

				memset(buf.GetData(), 0, kCDSectorSize);
				rdone = kCDSectorSize;
				m_BadSectors.AddSector((BadSectorDatabase::SectorNumberType)(m_BytesDone / kCDSectorSize));
			}
		}
		else if (m_BytesDone >= endOfFailedBlock)
			bPerSectorMode = false;

		size_t wrTodo = rdone;
		char *pWriteBuf = (char *)buf.GetData();

		do
		{
			st = ActionStatus();
			size_t done = m_pTargetFile->Write(pWriteBuf, wrTodo, &st);
			wrTodo -= done;
			pWriteBuf += done;

			if (!done)
			{
				if (m_bAbort)
					return MAKE_STATUS(OperationAborted);
				else 
				{
					MutexLocker lck(m_MainMutex);
					if (!m_pErrorHandler)
						return st;
					else if (m_pErrorHandler->OnWriteError(st))
						continue;
					else
						return MAKE_STATUS(OperationAborted);
				}
					
			}
			else
				break;
		} while (wrTodo);

		{
			MutexLocker lck(m_RemainingTimeUpdateMutex);
			m_RateCalculator.ReportNewRelativeDone(todo);
			m_RemainingTimeEstimator.ReportNewRelativeDone(todo);
			m_BytesDone += todo;
		}

		if (m_BytesDone == todo)
			ResetRemainingTimeCalculation();	//Reset time estimator after first block is read and the CD has spinned up

		if (m_bPaused)
			m_ContinueEvent.Wait();
	}

	return MAKE_STATUS(OperationAborted);
}

BazisLib::ActionStatus BackgroundDriveReader::SetTargetFile(const BazisLib::String &filePath)
{
	if (m_pTargetFile)
		return MAKE_STATUS(InvalidState);

	m_ImagePath = filePath;

	ActionStatus st;
	m_pTargetFile = new ACFile(filePath, FileModes::CreateOrTruncateRW, &st);
	if (!st.Successful())
	{
		m_pTargetFile = NULL;
		return st;
	}

	ASSERT(m_pTargetFile->Valid());
	return MAKE_STATUS(Success);
}

BazisLib::ActionStatus BackgroundDriveReader::StartReading()
{
	if (!Start())
		return MAKE_STATUS(UnknownError);
	if (!m_pTargetFile || !m_pTargetFile->Valid())
		return MAKE_STATUS(NotInitialized);
	return MAKE_STATUS(Success);
}

BazisLib::ActionStatus BackgroundDriveReader::QueryStatus()
{
	if (IsRunning())
		return MAKE_STATUS(Pending);
	return m_FinalStatus;
}

void BackgroundDriveReader::AbortIfStillRunning()
{
	{
		MutexLocker lck(m_MainMutex);
		m_bAbort = true;
		Resume();
		if (!IsRunning())
			return;
		if (m_pTargetFile)
			m_pTargetFile->Close();
		m_File.Close();
	}

	Join();
}

BackgroundDriveReader::ProgressRecord BackgroundDriveReader::GetProgress()
{
	ProgressRecord rec;
	rec.Total = m_VolumeSize;
	rec.Done = m_BytesDone;
	rec.BytesPerSecond = m_RateCalculator.GetBytesPerSecond();
	rec.TotalBad = m_QueuedBadSectors;
	rec.ProcessedBad = m_ProcessedBadSectors;
	return rec;
}

BazisLib::TimeSpan BackgroundDriveReader::GetEstimatedRemainingTime(bool *pAccurate)
{
	*pAccurate = (m_RemainingTimeEstimator.GetDownloadedTotal() > (1024 * 1024));
	//TODO: implement advanced prediction algorithm
	return m_RemainingTimeEstimator.GetExpectedRemainingTime(m_VolumeSize - m_BytesDoneOnLastRemainingTimeReset);
}

BazisLib::ActionStatus BackgroundDriveReader::ReReadBadSectors()
{
	std::list<BadSectorDatabase::Record> records;
	m_QueuedBadSectors = m_BadSectors.GetTotalCount();
	m_ProcessedBadSectors = 0;
	m_BadSectors.DetachList(records);
	static char buf[kCDSectorSize];


	for each(BadSectorDatabase::Record rec in records)
		for (BadSectorDatabase::SectorNumberType sec = rec.SectorNumber; sec < (rec.SectorNumber + rec.SectorCount); sec++)
		{
			ActionStatus st;
			ULONGLONG offset = sec;
			offset *= kCDSectorSize;
			if (m_File.Seek(offset, FileFlags::FileBegin, &st) == offset)
				st = MAKE_STATUS(Success);
			if (!st.Successful())
				if (m_bAbort)
					return MAKE_STATUS(OperationAborted);
				else
					return st;

			st = ActionStatus();
			if (m_pTargetFile->Seek(offset, FileFlags::FileBegin, &st) == offset)
				st = MAKE_STATUS(Success);
			if (!st.Successful())
				if (m_bAbort)
					return MAKE_STATUS(OperationAborted);
				else
					return st;

			st = ActionStatus();
			if (m_File.Read(buf, kCDSectorSize, &st) != kCDSectorSize)
				m_BadSectors.AddSector(sec);
			else
			{
				st = ActionStatus();
				if (m_pTargetFile->Write(buf, kCDSectorSize, &st) != kCDSectorSize)
					if (m_bAbort)
						return MAKE_STATUS(OperationAborted);
					else
						return st;
			}

			if (m_bAbort)
				return MAKE_STATUS(OperationAborted);
			if (m_bCancelBadSectorReReading)
				return MAKE_STATUS(Success);

			m_ProcessedBadSectors++;
		}

	return MAKE_STATUS(Success);
}

void BackgroundDriveReader::ResetRemainingTimeCalculation()
{
	MutexLocker lck(m_RemainingTimeUpdateMutex);
	m_RemainingTimeEstimator.Reset();
	m_BytesDoneOnLastRemainingTimeReset = m_BytesDone;
}
