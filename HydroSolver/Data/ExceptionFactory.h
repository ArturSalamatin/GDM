#pragma once
#include "../stdafx.h"
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
		static std::ostream& out = std::cout;
		struct wNoPermeabilityYData
		{
			wNoPermeabilityYData()
			{
				out
					<< ">>>>>There is no permeability data in Y direction. "
					<< "Permeability in X direction is used instead."
					<< std::endl;
			}
		};
		struct wNoPermeabilityZData
		{
			wNoPermeabilityZData()
			{
				out
					<< ">>>>>There is no permeability data in Z direction. "
					<< "Permeability in X direction is used instead."
					<< std::endl;
			}
		};
		struct wNoPorosityData
		{
			wNoPorosityData(double val)
			{
				out
					<< ">>>>>There is no porosity data. "
					<< "Permeability value = " + std::to_string(val) + " is used instead."
					<< std::endl;
			}
		};

		struct wWellOperationDataNotFound
		{
			wWellOperationDataNotFound(const WellName& well_name)
			{
				out 
					<< ">>>>>The Well " +
					well_name +
					" does not have data on perforations. It is ignored."
					<< std::endl;
			}
		};
		struct wWellOperationDataIsEmpty
		{
			wWellOperationDataIsEmpty(const WellName& well_name)
			{
				out
					<< ">>>>>The Well " +
					well_name +
					" operation data is found, but it is empty. The well is ignored."
					<< std::endl;
			}
		};
		struct wWellMERnotFound
		{
			wWellMERnotFound(const WellName& well_name)
			{
				out
					<< ">>>>>The Well " +
					well_name +
					" does not contain information abut MER. The well is ignored.\n"
					<< std::endl;
			}
		};

		struct wWellinitializationFailure
		{
			wWellinitializationFailure(const WellName& well_name, const std::exception& e)
			{
				out
					<< ">>>>>The Well " +
					well_name +
					" can not be initialized with message:\n"
					<< e.what()
					<< std::endl;
			}
		};

		/*struct wNoPerforationData 
		{
			wNoPerforationData(const WellName& well_name)
			{
				out << 
					">>>>>The well " + well_name + 
					" does not contain any perforation data.\n";
			}
		};*/
		struct wFirstPerforationMoved
		{
			wFirstPerforationMoved(const WellName& well_name, 
				double oldTime, double newTime)
			{
				out << 
					">>>>>Well " + well_name + 
					": first job date " + std::to_string(oldTime) + 
					" was moved to " + std::to_string(newTime) + 
					" to be in accordance with the MER dates.\n";
			}
		};
		struct wLastPerforationMoved
		{
			wLastPerforationMoved(const WellName& well_name, 
				double oldTime, double newTime)
			{
				out <<
					">>>>>Well " + well_name + 
					": last job date " + std::to_string(oldTime) + 
					" was moved to " + std::to_string(newTime)
					+ " to be in accordance with the MER dates.\n";
			}
		};
		struct wNoFolderCreated
		{
			wNoFolderCreated()
			{
				out << ">>>>>Could not create folder dam//gdm.\n";
					/*LogFileSpace::LogFile::WriteLog("class_ReservoirSimulator", "method_SaveFlowField2File",
						"warning", "Could not create folder dam//gdm", "");*/
			}
		};
		struct wMERDataRemoved
		{
			wMERDataRemoved(const WellName& well_name)
			{
				out << 
					">>>>>Well " + well_name + 
					" does not show any overall debit at any time frame. " + 
					"Its MER record made empty.\n";
				//	LogFileSpace::LogFile::Well_AllMER_DataRemoved(Name());
			}
		};
		struct wDuplicateMERrecordWithZeroDebitDeleted
		{
			wDuplicateMERrecordWithZeroDebitDeleted()
			{
				out << "Duplicate MER record with zero overall debit erased.\n\n";
			}
		};

		struct wDuplicateMERrecordUnited
		{
			wDuplicateMERrecordUnited()
			{
				out << "Duplicate MER record with non-zero overall debit united into a single record.\n\n";
			}
		};

		struct wMERrecordsAtSameDate
		{
			wMERrecordsAtSameDate(const WellName& well_name, std::map<std::string, float>& r1, std::map<std::string, float>& r2)
			{
				out << "Well " << well_name << " MER data:\n";
				out << std::setw(8) << std::left
					<< "time"
					<< std::setw(10) << std::left
					<< "oil_v"
					<< std::setw(10) << std::left
					<< "water_v"
					<< std::setw(12) << std::left
					<< "oil_m"
					<< std::setw(12) << std::left
					<< "water_m"
					<< std::setw(12) << std::left
					<< "pump_water"
					<< std::setw(11) << std::left
					<< "idle_time"
					<< std::setw(5) << std::left
					<< "type"
					<< std::setw(8) << std::left
					<< "is_work"
					<< std::endl;

				
				{
					auto& m = r1;
					out << std::setw(8) << std::left
						<< m.at("time")
						<< std::setw(10) << std::left
						<< m.at("oil_v")
						<< std::setw(10) << std::left
						<< m.at("water_v")
						<< std::setw(12) << std::left
						<< m.at("oil_m")
						<< std::setw(12) << std::left
						<< m.at("water_m")
						<< std::setw(12) << std::left
						<< m.at("pump_water")
						<< std::setw(11) << std::left
						<< m.at("idle_time")
						<< std::setw(5) << std::left
						<< m.at("type")
						<< std::setw(8) << std::left
						<< m.at("is_work")
						<< std::endl;
				}
				{
					auto& m = r2;
					out << std::setw(8) << std::left
						<< m.at("time")
						<< std::setw(10) << std::left
						<< m.at("oil_v")
						<< std::setw(10) << std::left
						<< m.at("water_v")
						<< std::setw(12) << std::left
						<< m.at("oil_m")
						<< std::setw(12) << std::left
						<< m.at("water_m")
						<< std::setw(12) << std::left
						<< m.at("pump_water")
						<< std::setw(11) << std::left
						<< m.at("idle_time")
						<< std::setw(5) << std::left
						<< m.at("type")
						<< std::setw(8) << std::left
						<< m.at("is_work")
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
				std::cout << "...Well " + well_name + " initialization started\n";
			}
		};
		struct mWellInitializationDone
		{
			mWellInitializationDone(const WellName& well_name)
			{
				std::cout << "...Well " + well_name + " initialization done\n\n";
			}
		};
		struct mMERInitializationStarted
		{
			mMERInitializationStarted(const WellName& well_name)
			{
				std::cout << "...Initialization of MER of well " + well_name + " started\n";
			}
		};
		struct mMERInitializationDone
		{
			mMERInitializationDone(const WellName& well_name)
			{
				std::cout << "...Initialization of MER of well " + well_name + " done\n";
			}
		};

		struct mWellOverallTimeFrame
		{
			mWellOverallTimeFrame(const WellName& well_name,
				const mer_descriptor::TimeFrame& frame)
			{
				std::cout <<
					"Well " + well_name + " start date: " +
					std::to_string((int)frame.start) + "; end date: " + std::to_string((int)frame.end) 
					<< std::endl;
			}
		};
		struct mMERnoData
		{
			mMERnoData()
			{
				std::cout << "...Well does not have any MER data\n";
			}
		};

		struct mCalcProgress
		{
			mCalcProgress(double curTime, size_t curStep, size_t maxStep)
			{
				constexpr size_t width = 23;
				std::cout 
					<< std::left << std::setw(width) 
					<<  "Simulation progress: "
					<< std::left
					<< std::ceil((float)curStep/(float)maxStep*1000)/1000*100
					<< "%\n"
					<< std::left << std::setw(width)
					<< "Current step: "
					<< std::left
					<< std::to_string(curStep)
					<< "\n"
					<< std::left << std::setw(width)
					<< "Total steps count: "
					<< std::left
					<< std::to_string(maxStep)
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

		static void MERrecordsAtSameDate(const WellName& well_name, std::map<std::string, float>& r1, std::map<std::string, float>& r2)
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