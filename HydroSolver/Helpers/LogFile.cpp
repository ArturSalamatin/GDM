#include "LogFile.h"
//#include <fstream>
//#include <string>

//std::ofstream LogFileSpace::LogFile::logFile("logfile.txt", std::ios_base::out);

 void LogFileSpace::LogFile::Well_InstantiationStarted(const std::wstring& name)
{
	WriteLog(L"Well " + name + L" is being instantiated...");
}

 void LogFileSpace::LogFile::Well_InstantiationEnded(const std::wstring& name)
{
	WriteLog(L"Well " + name + L" is instantiated.", true);
}

 void LogFileSpace::LogFile::Well_RequestTimeIsBeforeFirstMER(const std::wstring& name, int curTime, int firstDate)
{
	/*WriteLog(">>>>>>Well " + name + ": The time moment " + std::to_string(curTime) +
	" is before the earliest MER time moment " + std::to_string(firstDate) + ".");
	WriteLog(">>>Zero debit is assumed", true);*/
}

 void LogFileSpace::LogFile::Well_RequestTimeIsAfterLastMER(const std::wstring& name, int curTime, int lastDate)
{
	/*WriteLog(">>>>>>Well " + name + ": The time moment " + std::to_string(curTime) +
	" is after the latest MER time moment " + std::to_string(lastDate) + ".");
	LogFileSpace::LogFile::WriteLog(">>>Zero debit is assumed", true);*/
}

 void LogFileSpace::LogFile::Well_NoMER_Data(const std::wstring& name)
{
	WriteLog(L">>>>>Well " + name + L": there is no MER data. Zero overall debit was assumed.");

	//	std::wcout << JSON::JSONMessage::CreateJSONMessage(
	//		имя блока, от которого пришло сообщение;
	//	имя блока в основном блоке;
	//статус: success or error; 
	//параметры: любая строка или объект =  CreateObject или CreateArray)
	//				JSON::CreateJSON::CreateArray(вектор строк)
	//					JSON::CreateJSON::CreateObject(вектор пар: ключ-значение = строка или такой же объект JSON)

}

 void LogFileSpace::LogFile::Well_out_of_domain(const std::wstring& name)
{
	WriteLog(L">>>>>Well " + name + L": there is no MER data. Zero overall debit was assumed.");
}

 void LogFileSpace::LogFile::Well_AllMER_DataRemoved(const std::wstring& name)
{
	WriteLog(L">>>>>Well " + name + L" does not show any overall debit at any time frame. Its MER record made empty.");
}

 void LogFileSpace::LogFile::Well_InitialMER_RecordsRemoved(const std::wstring& name, int i)
{
	WriteLog(L">>>>>Well " + name + L":  " + std::to_wstring(i) +
		L" initial MER records have been deleted. They contained zero overall debit.");
}

 void LogFileSpace::LogFile::Well_LastMER_RecordsRemoved(const std::wstring& name, int i)
{
	WriteLog(L">>>>>Well " + name + L":  " + std::to_wstring(i) +
		L" last MER records have been deleted. They contained zero overall debit.");
}

 void LogFileSpace::LogFile::Well_OverallTimeFrame(const std::wstring& name, std::pair<double, double> interval)
{
	WriteLog(L"Well " + name + L" start date: " + std::to_wstring((int)interval.first) + L" end date: " + std::to_wstring((int)interval.second));
}

 void LogFileSpace::LogFile::Perforation_AveragingStarted(int id)
{
	WriteLog(L"   Perforation averaging in layer " + std::to_wstring(id) + L" within production month is started...");
}

 void LogFileSpace::LogFile::Perforation_AveragingEnded(int id)
{
	WriteLog(L"   Perforation averaging in layer " + std::to_wstring(id) + L" within production month ended.");
}

 void LogFileSpace::LogFile::Perforation_FirstMoved(const std::wstring& name, double oldDate, double newDate)
{
	WriteLog(L">>>>>Well " + name + L": first job date " + std::to_wstring(oldDate) + L" was moved to " + std::to_wstring(newDate)
		+ L" to be in accordance with the MER dates.");
}

 void LogFileSpace::LogFile::Perforation_LastMoved(const std::wstring& name, int oldDate, int newDate)
{
	WriteLog(L">>>>>Well " + name + L": last job date " + std::to_wstring(oldDate) + L" was moved to " + std::to_wstring(newDate)
		+ L" to be in accordance with the MER dates.");
}

//	static std::ofstream logFile;

void LogFileSpace::LogFile::WriteLog(std::wstring logLine, bool isNewLine)
{
	/*std::ofstream logFile(lutyiPizdeccc + L"logfile.txt", std::ios_base::app);
	UniversalSCParser::CP1251Encoder enc;
	logFile << enc.ToString(logLine) << std::endl;

	if (isNewLine)
	logFile << std::endl;

	logFile.close();*/
}

//	static std::ofstream logFile;

void LogFileSpace::LogFile::WriteLog(std::wstring&& Sender, std::wstring&& ChildSender, std::wstring&& Status, std::wstring&& Options, const char* msg0)
{
	std::string msg(msg0);
	UniversalSCParser::CP1251Encoder enc;
	std::wstring eWhat = std::move(Options) + enc.ToWString(msg);

	SetConsoleOutputCP(1251);
	HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdOut != NULL && stdOut != INVALID_HANDLE_VALUE)
	{
		DWORD written = 0;
		UniversalSCParser::CP1251Encoder temp;
		auto message =
			JSON::JSONMessage::CreateJSONMessage(std::move(Sender), std::move(ChildSender), std::move(Status), std::move(eWhat));
		message += L"\r\n";
		auto message1 = temp.ToString(message);
		//WriteConsoleA
		WriteFile
		(stdOut, message1.c_str(), message1.length(), &written, NULL);
	}
}

 void LogFileSpace::LogFile::Clear()
{
	//	std::ofstream logFile("logfile.txt", std::ios_base::out);
	//	logFile.clear();
	auto buffer = std::make_unique<char[]>(5);
	std::setvbuf(stdout, buffer.get(), _IOFBF, 5);
}
