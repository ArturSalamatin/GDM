#include "LogFile.h"

namespace LogFileSpace
{
	void LogFile::Well_InstantiationStarted(const std::string&) {}
	void LogFile::Well_InstantiationEnded(const std::string&) {}
	void LogFile::Well_RequestTimeIsBeforeFirstMER(const std::string&, int, int) {}
	void LogFile::Well_RequestTimeIsAfterLastMER(const std::string&, int, int) {}
	void LogFile::Well_NoMER_Data(const std::string&) {}
	void LogFile::Well_out_of_domain(const std::string&) {}
	void LogFile::Well_AllMER_DataRemoved(const std::string&) {}
	void LogFile::Well_InitialMER_RecordsRemoved(const std::string&, int) {}
	void LogFile::Well_LastMER_RecordsRemoved(const std::string&, int) {}
	void LogFile::Well_OverallTimeFrame(const std::string&, const std::pair<double, double>&) {}
	void LogFile::Perforation_AveragingStarted(int) {}
	void LogFile::Perforation_AveragingEnded(int) {}
	void LogFile::Perforation_FirstMoved(const std::string&, double, double) {}
	void LogFile::Perforation_LastMoved(const std::string&, int, int) {}
	void LogFile::WriteLog(std::string, bool) {}
	void LogFile::WriteLog(std::string&&, std::string&&, std::string&&, std::string&&, const char*) {}
	void LogFile::Clear() {}
}
