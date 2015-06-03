#pragma once
#include <bzscore/string.h>
#include <bzscore/thread.h>
#include <bzscore/autofile.h>
#include <bzshlp/ratecalc.h>
#include <bzscore/sync.h>
#include "BadSectorDatabase.h"

enum {kDefaultReadBufferSize = 256 * 1024};

class BackgroundDriveReader : private BazisLib::Thread
{
public:
	class IErrorHandler
	{
	public:
		enum BadSectorsAction
		{
			Ignore,
			RereadOnce,
			RereadContinuous,
		};

		virtual bool OnWriteError(BazisLib::ActionStatus status)=0;
		virtual BadSectorsAction OnBadSectorsFound(BadSectorDatabase *pDB)=0;
	};

private:
	BazisLib::String m_SourcePath, m_ImagePath;
	BazisLib::File m_File;
	ULONGLONG m_VolumeSize, m_BytesDone, m_BytesDoneOnLastRemainingTimeReset;
	BazisLib::ActionStatus m_FinalStatus;
	volatile bool m_bAbort;
	size_t m_BufferSize;
	
	volatile bool m_bPaused;
	BazisLib::Win32::Event m_ContinueEvent;

	BazisLib::ManagedPointer<BazisLib::AIFile> m_pTargetFile;
	BazisLib::RateCalculator m_RateCalculator, m_RemainingTimeEstimator;
	BazisLib::Mutex m_RemainingTimeUpdateMutex;
	BazisLib::Mutex m_MainMutex;
	IErrorHandler *m_pErrorHandler;
	BadSectorDatabase m_BadSectors;
	bool m_bCancelBadSectorReReading;

	volatile ULONGLONG m_QueuedBadSectors, m_ProcessedBadSectors;

public:
	BackgroundDriveReader(const BazisLib::String &drivePath, size_t bufferSize, BazisLib::ActionStatus *pStatus);

	ULONGLONG GetVolumeSize() {return m_VolumeSize;}
	
	enum TOCError
	{
		teSuccess,
		teMultipleTracks,
		teNonDataTrack,
		teCannotReadTOC,
	};

	BazisLib::ActionStatus CheckTOC(TOCError *pAdditionalError);

	virtual int ThreadBody();
	BazisLib::ActionStatus DoMainReadLoop();
	BazisLib::ActionStatus ReReadBadSectors();
	BazisLib::ActionStatus SetTargetFile(const BazisLib::String &filePath);

public:
	BazisLib::ActionStatus StartReading();
	BazisLib::ActionStatus QueryStatus();

	void AbortIfStillRunning();
	BazisLib::ActionStatus GetFinalStatus() {return m_FinalStatus;}

	struct ProgressRecord
	{
		ULONGLONG Total, Done;
		ULONGLONG TotalBad, ProcessedBad;
		unsigned BytesPerSecond;
	};

	ProgressRecord GetProgress();

	const wchar_t *GetSourcePath(){return m_SourcePath.c_str();}
	const wchar_t *GetImagePath(){return m_ImagePath.c_str();}

	void Pause()
	{
		m_ContinueEvent.Reset();
		m_bPaused = true;
	}

	void Resume()
	{
		m_bPaused = false;
		m_ContinueEvent.Set();
	}

	bool IsPaused() {return m_bPaused;}

	void SetErrorHandler(IErrorHandler *pErrorHandler)
	{
		BazisLib::MutexLocker lck(m_MainMutex);
		m_pErrorHandler = pErrorHandler;
	}

	void ResetRemainingTimeCalculation();

	BazisLib::TimeSpan GetEstimatedRemainingTime(bool *pAccurate);

	unsigned GetTotalBadSectorCount() {return m_BadSectors.GetTotalCount();}

	void CancelBadSectorReReading() {m_bCancelBadSectorReReading = true;}

};