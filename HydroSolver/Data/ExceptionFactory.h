#pragma once
#include "../../stdafx.h"
#include "../defines.h"


namespace reservoir_simulator
{
	namespace custom_exceptions
	{
		struct eWellJobTime_TransferDate : public std::runtime_error
		{
			eWellJobTime_TransferDate(const std::string& msg) : 
				runtime_error{ msg }
			{}
		};

		struct eAccumulatedPerforations_AddNewJob : public std::runtime_error
		{
			eAccumulatedPerforations_AddNewJob(const std::string& msg) :
				runtime_error{ msg }
			{}
		};
	} // custom_exceptions

	namespace custom_warnings
	{
		static std::wostream& out = std::wcout;
		struct wNoPermeabilityYData
		{
			wNoPermeabilityYData()
			{
				out
					<< L">>>>>There is no permeability data in Y direction. "
					<< L"Permeability in X direction is used instead."
					<< std::endl;
			}
		};
		struct wNoPermeabilityZData
		{
			wNoPermeabilityZData()
			{
				out
					<< L">>>>>There is no permeability data in Z direction. "
					<< L"Permeability in X direction is used instead."
					<< std::endl;
			}
		};
		struct wNoPorosityData
		{
			wNoPorosityData(double val)
			{
				out
					<< L">>>>>There is no porosity data. "
					<< L"Permeability value = " + std::to_wstring(val) + L" is used instead."
					<< std::endl;
			}
		};

		struct wWellOperationDataNotFound
		{
			wWellOperationDataNotFound(const WellName& well_name)
			{
				out 
					<< L">>>>>The Well " +
					well_name +
					L" does not have data on perforations. It is ignored."
					<< std::endl;
			}
		};
		struct wWellOperationDataIsEmpty
		{
			wWellOperationDataIsEmpty(const WellName& well_name)
			{
				out
					<< L">>>>>The Well " +
					well_name +
					L" operation data is found, but it is empty. The well is ignored."
					<< std::endl;
			}
		};
		struct wWellMERnotFound
		{
			wWellMERnotFound(const WellName& well_name)
			{
				out
					<< L">>>>>The Well " +
					well_name +
					L" does not contain information abut MER. The well is ignored.\n"
					<< std::endl;
			}
		};

		struct wWellinitializationFailure
		{
			wWellinitializationFailure(const WellName& well_name, const std::exception& e)
			{
				out
					<< L">>>>>The Well " +
					well_name +
					L" can not be initialized with message:\n"
					<< e.what()
					<< std::endl;
			}
		};

		/*struct wNoPerforationData 
		{
			wNoPerforationData(const WellName& well_name)
			{
				out << 
					L">>>>>The well " + well_name + 
					L" does not contain any perforation data.\n";
			}
		};*/
		struct wFirstPerforationMoved
		{
			wFirstPerforationMoved(const WellName& well_name, 
				double oldTime, double newTime)
			{
				out << 
					L">>>>>Well " + well_name + 
					L": first job date " + std::to_wstring(oldTime) + 
					L" was moved to " + std::to_wstring(newTime) + 
					L" to be in accordance with the MER dates.\n";
			}
		};
		struct wLastPerforationMoved
		{
			wLastPerforationMoved(const WellName& well_name, 
				double oldTime, double newTime)
			{
				out <<
					L">>>>>Well " + well_name + 
					L": last job date " + std::to_wstring(oldTime) + 
					L" was moved to " + std::to_wstring(newTime)
					+ L" to be in accordance with the MER dates.\n";
			}
		};
		struct wNoFolderCreated
		{
			wNoFolderCreated()
			{
				out << L">>>>>Could not create folder dam//gdm.\n";
					/*LogFileSpace::LogFile::WriteLog(L"class_ReservoirSimulator", L"method_SaveFlowField2File",
						L"warning", L"Could not create folder dam//gdm", "");*/
			}
		};
		struct wMERDataRemoved
		{
			wMERDataRemoved(const WellName& well_name)
			{
				out << 
					L">>>>>Well " + well_name + 
					L" does not show any overall debit at any time frame. " + 
					L"Its MER record made empty.\n";
				//	LogFileSpace::LogFile::Well_AllMER_DataRemoved(Name());
			}
		};
		struct wDuplicateMERrecordWithZeroDebitDeleted
		{
			wDuplicateMERrecordWithZeroDebitDeleted()
			{
				out << L"Duplicate MER record with zero overall debit erased.\n\n";
			}
		};

		struct wDuplicateMERrecordUnited
		{
			wDuplicateMERrecordUnited()
			{
				out << L"Duplicate MER record with non-zero overall debit united into a single record.\n\n";
			}
		};

		struct wMERrecordsAtSameDate
		{
			wMERrecordsAtSameDate(const WellName& well_name, std::map<std::wstring, float>& r1, std::map<std::wstring, float>& r2)
			{
				out << L"Well " << well_name << L" MER data:\n";
				out << std::setw(8) << std::left
					<< L"time"
					<< std::setw(10) << std::left
					<< L"oil_v"
					<< std::setw(10) << std::left
					<< L"water_v"
					<< std::setw(12) << std::left
					<< L"oil_m"
					<< std::setw(12) << std::left
					<< L"water_m"
					<< std::setw(12) << std::left
					<< L"pump_water"
					<< std::setw(11) << std::left
					<< L"idle_time"
					<< std::setw(5) << std::left
					<< L"type"
					<< std::setw(8) << std::left
					<< L"is_work"
					<< std::endl;

				
				{
					auto& m = r1;
					out << std::setw(8) << std::left
						<< m.at(L"time")
						<< std::setw(10) << std::left
						<< m.at(L"oil_v")
						<< std::setw(10) << std::left
						<< m.at(L"water_v")
						<< std::setw(12) << std::left
						<< m.at(L"oil_m")
						<< std::setw(12) << std::left
						<< m.at(L"water_m")
						<< std::setw(12) << std::left
						<< m.at(L"pump_water")
						<< std::setw(11) << std::left
						<< m.at(L"idle_time")
						<< std::setw(5) << std::left
						<< m.at(L"type")
						<< std::setw(8) << std::left
						<< m.at(L"is_work")
						<< std::endl;
				}
				{
					auto& m = r2;
					out << std::setw(8) << std::left
						<< m.at(L"time")
						<< std::setw(10) << std::left
						<< m.at(L"oil_v")
						<< std::setw(10) << std::left
						<< m.at(L"water_v")
						<< std::setw(12) << std::left
						<< m.at(L"oil_m")
						<< std::setw(12) << std::left
						<< m.at(L"water_m")
						<< std::setw(12) << std::left
						<< m.at(L"pump_water")
						<< std::setw(11) << std::left
						<< m.at(L"idle_time")
						<< std::setw(5) << std::left
						<< m.at(L"type")
						<< std::setw(8) << std::left
						<< m.at(L"is_work")
						<< std::endl;
				}
			}
		};
	} // custom_warnings

	namespace custom_messages
	{
		struct mWellInitializationStarted
		{
			mWellInitializationStarted(const WellName& well_name)
			{
				std::wcout << L"...Well " + well_name + L" initialization started\n";
			}
		};
		struct mWellInitializationDone
		{
			mWellInitializationDone(const WellName& well_name)
			{
				std::wcout << L"...Well " + well_name + L" initialization done\n\n";
			}
		};
		struct mMERInitializationStarted
		{
			mMERInitializationStarted(const WellName& well_name)
			{
				std::wcout << L"...Initialization of MER of well " + well_name + L" started\n";
			}
		};
		struct mMERInitializationDone
		{
			mMERInitializationDone(const WellName& well_name)
			{
				std::wcout << L"...Initialization of MER of well " + well_name + L" done\n";
			}
		};

		struct mWellOverallTimeFrame
		{
			mWellOverallTimeFrame(const WellName& well_name,
				const mer_descriptor::TimeFrame& frame)
			{
				std::wcout <<
					L"Well " + well_name + L" start date: " +
					std::to_wstring((int)frame.start) + L"; end date: " + std::to_wstring((int)frame.end) 
					<< std::endl;
			}
		};
		struct mMERnoData
		{
			mMERnoData()
			{
				std::wcout << L"...Well does not have any MER data\n";
			}
		};

		struct mCalcProgress
		{
			mCalcProgress(double curTime, size_t curStep, size_t maxStep)
			{
				constexpr size_t width = 23;
				std::wcout 
					<< std::left << std::setw(width) 
					<<  L"Simulation progress: "
					<< std::left
					<< std::ceil((float)curStep/(float)maxStep*1000)/1000*100
					<< L"%\n"
					<< std::left << std::setw(width)
					<< L"Current step: "
					<< std::left
					<< std::to_wstring(curStep)
					<< L"\n"
					<< std::left << std::setw(width)
					<< L"Total steps count: "
					<< std::left
					<< std::to_wstring(maxStep)
					<< std::endl;
			}
		};
	}

	class ExceptionFactory
	{
	public:
		static void
			WellJobTime_TransferDate()
		{
			throw custom_exceptions::
				eWellJobTime_TransferDate
			{
				"WellJobTime::TransferDate: wrong date transfer."
			};
		}

		static void
			AccumulatedPerforations_AddNewJob()
		{
			throw custom_exceptions::
				eAccumulatedPerforations_AddNewJob
			{
				"AccumulatedPerforations::AddNewJob: wrong sequence of perforations."
			};
		}
	};

	class WarningFactory
	{
	public:
		static void NoPermeabilityYData()
		{
			custom_warnings::wNoPermeabilityYData{};			
		}
		static void NoPermeabilityZData()
		{
			custom_warnings::wNoPermeabilityZData{};
		}
		static void NoPorosityData(double val)
		{
			custom_warnings::wNoPorosityData{val};
		}
		/*static void NoPerforationData(const WellName& well_name)
		{
			custom_warnings::wNoPerforationData{ well_name };
		}*/
		static void FirstPerforationMoved(const WellName& well_name, double oldTime, double newTime)
		{
			custom_warnings::wFirstPerforationMoved{ well_name, oldTime, newTime };
		}
		static void LastPerforationMoved(const WellName& well_name, double oldTime, double newTime)
		{
			custom_warnings::wLastPerforationMoved{ well_name, oldTime, newTime };
		}
		static void NoFolderCreated()
		{
			custom_warnings::wNoFolderCreated{};
		}
		static void MERDataRemoved(const WellName& well_name)
		{
			custom_warnings::wMERDataRemoved{ well_name };
		}
		static void WellOperationDataNotFound(const WellName& well_name)
		{
			custom_warnings::wWellOperationDataNotFound{ well_name };
		}
		static void WellOperationDataIsEmpty(const WellName& well_name)
		{
			custom_warnings::wWellOperationDataIsEmpty{ well_name };
		}
		static void WellMERnotFound(const WellName& well_name)
		{
			custom_warnings::wWellMERnotFound{ well_name };
		}
		static void WellinitializationFailure(const WellName& well_name, std::exception& e)
		{
			custom_warnings::wWellinitializationFailure{ well_name, e };
		}

		static void MERrecordsAtSameDate(const WellName& well_name, std::map<std::wstring, float>& r1, std::map<std::wstring, float>& r2)
		{
			custom_warnings::wMERrecordsAtSameDate{ well_name, r1, r2 };
		}

		static void DuplicateMERrecordWithZeroDebitDeleted()
		{
			custom_warnings::wDuplicateMERrecordWithZeroDebitDeleted{};
		}

		static void DuplicateMERrecordUnited()
		{
			custom_warnings::wDuplicateMERrecordUnited{};
		}
	};

	class MessageFactory
	{
	public:
		static void WellInitializationStarted(const WellName& well_name)
		{
			custom_messages::mWellInitializationStarted{ well_name };
		}
		static void WellInitializationDone(const WellName& well_name)
		{
			custom_messages::mWellInitializationDone{ well_name };
		}


		static void MERInitializationStarted(const WellName& well_name)
		{
			custom_messages::mMERInitializationStarted{ well_name };
		}
		static void MERInitializationDone(const WellName& well_name)
		{
			custom_messages::mMERInitializationDone{ well_name };
		}

		static void WellOverallTimeFrame(const WellName& well_name,
			const mer_descriptor::TimeFrame& frame)
		{
			custom_messages::mWellOverallTimeFrame{ well_name, frame };
		}

		static void MERnoData()
		{
			custom_messages::mMERnoData{ };
		}

		static void CalcProgress(double curTime, size_t curStep, size_t maxStep)
		{
			custom_messages::mCalcProgress{ curTime, curStep, maxStep };

		}


	};

} // reservoir_simulator