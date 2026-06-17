#include "LogFile.h"

namespace LogFileSpace
{
	void LogFile::Well_InstantiationStarted(const std::wstring&) {}
	void LogFile::Well_InstantiationEnded(const std::wstring&) {}
	void LogFile::Well_RequestTimeIsBeforeFirstMER(const std::wstring&, int, int) {}
	void LogFile::Well_RequestTimeIsAfterLastMER(const std::wstring&, int, int) {}
	void LogFile::Well_NoMER_Data(const std::wstring&) {}
	void LogFile::Well_out_of_domain(const std::wstring&) {}
	void LogFile::Well_AllMER_DataRemoved(const std::wstring&) {}
	void LogFile::Well_InitialMER_RecordsRemoved(const std::wstring&, int) {}
	void LogFile::Well_LastMER_RecordsRemoved(const std::wstring&, int) {}
	void LogFile::Well_OverallTimeFrame(const std::wstring&, const std::pair<double, double>&) {}
	void LogFile::Perforation_AveragingStarted(int) {}
	void LogFile::Perforation_AveragingEnded(int) {}
	void LogFile::Perforation_FirstMoved(const std::wstring&, double, double) {}
	void LogFile::Perforation_LastMoved(const std::wstring&, int, int) {}
	void LogFile::WriteLog(std::wstring, bool) {}
	void LogFile::WriteLog(std::wstring&&, std::wstring&&, std::wstring&&, std::wstring&&, const char*) {}
	void LogFile::Clear() {}
}
