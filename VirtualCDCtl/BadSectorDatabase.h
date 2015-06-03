#pragma once
#include <bzscore/sync.h>
#include <list>

class BadSectorDatabase
{
public:
	typedef unsigned SectorNumberType;

	struct Record
	{
		SectorNumberType SectorNumber;
		SectorNumberType SectorCount;

		Record(SectorNumberType number)
			: SectorNumber(number)
			, SectorCount(1)
		{
		}

		SectorNumberType GetFirstFollowingSector()
		{
			return SectorNumber + SectorCount;
		}
	};

private:
	BazisLib::Mutex m_AccessMutex;
	std::list<Record> m_Records;

public:
	void AddSector(SectorNumberType number)
	{
		BazisLib::MutexLocker lck(m_AccessMutex);
		if (!m_Records.empty())
			if (number == m_Records.back().GetFirstFollowingSector())
			{
				m_Records.back().SectorCount++;
				return;
			}
		m_Records.push_back(Record(number));
	}

	SectorNumberType GetTotalCount()
	{
		BazisLib::MutexLocker lck(m_AccessMutex);
		SectorNumberType count = 0;
		for each(Record rec in m_Records)
			count += rec.SectorCount;
		return count;
	}

	bool Empty()
	{
		BazisLib::MutexLocker lck(m_AccessMutex);
		return m_Records.empty();
	}

	void DetachList(__out std::list<Record> &records)
	{
		BazisLib::MutexLocker lck(m_AccessMutex);
		records.clear();
		m_Records.swap(records);
	}

	std::list<Record>::const_iterator begin() {return m_Records.begin();}
	std::list<Record>::const_iterator end() {return m_Records.end();}

};