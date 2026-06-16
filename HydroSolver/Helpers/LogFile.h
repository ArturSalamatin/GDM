#pragma once
#include "../stdafx.h"

#include "../Utils/UniversalSVParser.h"
#include "../Utils/JSON/JSONCreate.h"

namespace LogFileSpace
{
	class LogFile
	{
	public:
		static std::ofstream logFile;

		static void Well_InstantiationStarted(const std::wstring& name);
		static void Well_InstantiationEnded(const std::wstring& name);
		static void Well_RequestTimeIsBeforeFirstMER(const std::wstring& name, int curTime, int firstDate);
		static void Well_RequestTimeIsAfterLastMER(const std::wstring& name, int curTime, int lastDate);
		static void Well_NoMER_Data(const std::wstring& name);
		static void Well_out_of_domain(const std::wstring& name);
		static void Well_AllMER_DataRemoved(const std::wstring& name);
		static void Well_InitialMER_RecordsRemoved(const std::wstring& name, int i);
		static void Well_LastMER_RecordsRemoved(const std::wstring& name, int i);
		static void Well_OverallTimeFrame(const std::wstring& name, const std::pair<double, double>& interval);
		
		static void Perforation_AveragingStarted(int id);
		static void Perforation_AveragingEnded(int id);
		static void Perforation_FirstMoved(const std::wstring& name, double oldDate, double newDate);
		static void Perforation_LastMoved(const std::wstring& name, int oldDate, int newDate);

	public:

	//	static std::ofstream logFile;
		static void WriteLog(std::wstring logLine, bool isNewLine = false);

		//	static std::ofstream logFile;
		static void WriteLog(
			 std::wstring&& Sender,
			 std::wstring&& ChildSender,
			 std::wstring&& Status,
			 std::wstring&& Options = L"",
			const char* msg0 = "");

		static void Clear();
	};
}