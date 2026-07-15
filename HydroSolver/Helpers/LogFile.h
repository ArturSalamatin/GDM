#pragma once
#include "../stdafx.h"

namespace LogFileSpace
{
	class LogFile
	{
	public:
		static void Well_InstantiationStarted(const std::string& name);
		static void Well_InstantiationEnded(const std::string& name);
		static void Well_RequestTimeIsBeforeFirstMER(const std::string& name, int curTime, int firstDate);
		static void Well_RequestTimeIsAfterLastMER(const std::string& name, int curTime, int lastDate);
		static void Well_NoMER_Data(const std::string& name);
		static void Well_out_of_domain(const std::string& name);
		static void Well_AllMER_DataRemoved(const std::string& name);
		static void Well_InitialMER_RecordsRemoved(const std::string& name, int i);
		static void Well_LastMER_RecordsRemoved(const std::string& name, int i);
		static void Well_OverallTimeFrame(const std::string& name, const std::pair<double, double>& interval);

		static void Perforation_AveragingStarted(int id);
		static void Perforation_AveragingEnded(int id);
		static void Perforation_FirstMoved(const std::string& name, double oldDate, double newDate);
		static void Perforation_LastMoved(const std::string& name, int oldDate, int newDate);

		static void WriteLog(std::string logLine, bool isNewLine = false);
		static void WriteLog(
			std::string&& Sender,
			std::string&& ChildSender,
			std::string&& Status,
			std::string&& Options = "",
			const char* msg0 = "");
		static void Clear();
	};
}
