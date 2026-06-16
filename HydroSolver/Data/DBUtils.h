#pragma once
#include <sstream>
#include <string>
#include <pqxx/pqxx>
#include <future>
#include <fstream>
#include <filesystem>


namespace grdecl_memory {

	class Utils {
	public:
		static std::unique_ptr<pqxx::connection> CreateDBConnection(std::string project_uuid)
		{
			auto con_string = GetCONNSTRNG("localhost", "5432", project_uuid.c_str(), "postgres", "");
			return std::make_unique<pqxx::connection>(con_string.str().c_str());
		}
	private:
		static std::stringstream GetCONNSTRNG(
			const char* host,
			const char* port,
			const char* dbname,
			const char* user,
			const char* pass)
		{
			std::stringstream out;
			out << "host=" << host
				<< " port=" << port
				<< " dbname=" << dbname
				<< " user=" << user
				<< " password=" << pass
				<< " client_encoding=utf8 application_name=pcnac_project_creator";
			return out;
		}
	};

	class IGrdeclSaver {
	public:
		virtual void  SetObject(std::string name) = 0;
		virtual void ResetGRDECL() = 0;
		virtual void ResetGRID(std::string name) = 0;
		virtual std::vector<std::pair<float, float>> GetContour() = 0;
		//template<typename... TArgs, typename TArg> void Add(TArg val, TArgs... rest) { throw std::exception("Add not implemented"); }
	};
	class DBSaver : public IGrdeclSaver {
	public:
		DBSaver(std::string project_uuid)
		{
			//auto con_string = GetCONNSTRNG("localhost", "5432", project_uuid.c_str(), "postgres", "");
			Con =/* std::make_unique<pqxx::connection>(con_string.str().c_str());*/ Utils::CreateDBConnection(project_uuid);
			CurrentTransaction = std::make_unique<pqxx::work>(*Con);

			//10 MB буффер
			//Buffer = std::make_unique<std::byte[]>(BufferSize);
		}

		~DBSaver() {
			FlushBuffer();
			if (DBAsyncUnload.valid())
				DBAsyncUnload.wait();
			CurrentTransaction->commit();
		}

		virtual void SetObject(std::string name)
		{
			FlushBuffer(); //стрим и AddCount очищается вот тут
			if (DBAsyncUnload.valid())
				DBAsyncUnload.wait();
			
			UnlinkGRD(name);
			CreateLO(name);
		}
		virtual void ResetGRID(std::string name)
		{
			UnlinkGRD(name);
		}

		virtual void ResetGRDECL()
		{
			const auto rows = CurrentTransaction->exec("SELECT name FROM input_data.grdecl_i");
			for (const auto row : rows) {
				UnlinkGRD(row["name"].c_str());
			}
			CurrentTransaction->exec("TRUNCATE TABLE input_data.grdecl_i");
		}

		virtual std::vector<std::pair<float, float>> GetContour()
		{
			pqxx::row zone = CurrentTransaction->exec1("SELECT * FROM input_data.object_contour");
			auto arr = zone["countour"].as_array();
			auto elem = arr.get_next();

			std::vector<std::pair<float, float>> out;
			std::pair<float, float> line = { std::numeric_limits<float>::max(),  std::numeric_limits<float>::max() };
			do {
				//auto elem2 = elem.second;
				if (*elem.second.c_str() != '\x00')
				{
					char* end;
					float coord = _strtof_l(elem.second.c_str(), &end, _create_locale(LC_NUMERIC, "en-us"));
					if (line.first == std::numeric_limits<float>::max())
					{
						line.first = coord;
					}
					else {
						line.second = coord;
						out.push_back(line);
						line = { std::numeric_limits<float>::max(),  std::numeric_limits<float>::max() };
					}
				}
				elem = arr.get_next();
			} while (elem.first != pqxx::array_parser::juncture::done);
			return out;
		}


		void Add() {};
		template<typename TArg, typename... TArgs> void Add(TArg val, TArgs... rest)
		{
			float _val = 0;
			if constexpr (std::is_same_v<TArg, std::string_view>) {
				char* end;
				_val = _strtof_l(val.data(), &end, local);
			}
			else {
				_val = static_cast<float>(val);
			}

			buffer.Add(_val);

			AddCount++;
			if (AddCount > BufferSize * 9 / 10 / sizeof(float))
			{
				FlushBuffer();
			}
			Add(rest...);
		}

	private:
		const size_t BufferSize = 10485760ULL; //10MB
		_locale_t local = _create_locale(LC_NUMERIC, "en-US");
		size_t AddCount = 0;

		std::string CurrentObject;
		std::string table;

		std::future<void> DBAsyncUnload;
		void FlushBuffer();

		std::unique_ptr<pqxx::connection> Con;
		std::unique_ptr<pqxx::work> CurrentTransaction;
		pqxx::blob CurrentLO;

		void UnlinkGRD(std::string name);
		void CreateLO(std::string name);

		template<size_t Buffers, size_t BufferCapacity, typename CharT> class InternalBuffer {
		public:
			InternalBuffer() {
				buffer = std::make_unique<std::unique_ptr<CharT[]>[]>(Buffers);
				for (size_t i = 0; i < Buffers; i++) {
					buffer[i] = std::make_unique<CharT[]>(BufferCapacity);
				}
			}
			void Add() {};
			template<typename TArg, typename... TArgs> void Add(TArg val, TArgs... rest) {
				if (memcpy_s(
					buffer[CurrentBuffer].get() + CurrentBufferOccupancy,
					BufferCapacity - CurrentBufferOccupancy,
					&val, sizeof(val)) != 0) {
					throw std::out_of_range("memcpy_s failed.");
				}
				CurrentBufferOccupancy += sizeof(val);
				Add(rest...);
			}

			std::pair<CharT*, size_t> OccupyBuffer() {
				CharT* curr = buffer[CurrentBuffer].get();
				const size_t size = CurrentBufferOccupancy;

				if (++CurrentBuffer == Buffers) {
					CurrentBuffer = 0;
				}
				CurrentBufferOccupancy = 0;

				return { curr, size };
			}

		private:
			std::unique_ptr<std::unique_ptr<CharT[]>[]> buffer;
			size_t CurrentBuffer = 0;
			size_t CurrentBufferOccupancy = 0;
		};

		//10MB
		InternalBuffer<2, 10485760ULL, std::byte> buffer;
	};

	class TempGRIDSaver : public IGrdeclSaver {
	public:
		TempGRIDSaver(std::string project_uuid) {
			ResetGRDECL();
			Con = Utils::CreateDBConnection(project_uuid);
			CurrentTransaction = std::make_unique<pqxx::nontransaction>(*Con);
		}

		virtual void SetObject(std::string name) {
			if (CurrentStream.is_open())
				CurrentStream.close();
			CurrentStream.open("./temp_grdecl/" + name, std::ios::binary | std::ios::out);
		}
		virtual void ResetGRDECL() {
			std::filesystem::remove_all("./temp_grdecl");
			std::filesystem::create_directory("./temp_grdecl");
		}
		virtual void ResetGRID(std::string name) {
			std::filesystem::remove("./temp_grdecl/" + name);
		}
		virtual std::vector<std::pair<float, float>> GetContour() {
			pqxx::row zone = CurrentTransaction->exec1("SELECT * FROM input_data.object_contour");
			auto arr = zone["countour"].as_array();
			auto elem = arr.get_next();

			std::vector<std::pair<float, float>> out;
			std::pair<float, float> line = { std::numeric_limits<float>::max(),  std::numeric_limits<float>::max() };
			do {
				//auto elem2 = elem.second;
				if (*elem.second.c_str() != '\x00')
				{
					char* end;
					float coord = _strtof_l(elem.second.c_str(), &end, _create_locale(LC_NUMERIC, "en-us"));
					if (line.first == std::numeric_limits<float>::max())
					{
						line.first = coord;
					}
					else {
						line.second = coord;
						out.push_back(line);
						line = { std::numeric_limits<float>::max(),  std::numeric_limits<float>::max() };
					}
				}
				elem = arr.get_next();
			} while (elem.first != pqxx::array_parser::juncture::done);
			return out;
		}


		void Add() {};
		template<typename TArg, typename... TArgs> void Add(TArg val, TArgs... rest)
		{
			float _val = 0;
			if constexpr (std::is_same_v<TArg, std::string_view>) {
				char* end;
				_val = _strtof_l(val.data(), &end, local);
			}
			else {
				_val = static_cast<float>(val);
			}
			CurrentStream.write(reinterpret_cast<char*>(&_val), sizeof(float));
			Add(rest...);
		}

	private:
		std::ofstream CurrentStream;
		std::unique_ptr<pqxx::connection> Con;
		std::unique_ptr<pqxx::nontransaction> CurrentTransaction;

		_locale_t local = _create_locale(LC_NUMERIC, "en-US");
	};

	class IGrdeclLoader {
	public:
		struct ModelParametrs {
			size_t Nx = 0;
			size_t Ny = 0;
			size_t Nz = 0;
		};
		virtual ModelParametrs GetModelParams() = 0;
		virtual std::vector<float> LoadPillars() = 0;
		virtual std::vector<float> LoadZCORN() = 0;
		virtual std::vector<float> LoadValues(std::string value) = 0;
		virtual std::vector<std::string> GetAvailableGRIDS() = 0;
		virtual std::map<size_t, size_t> GetIdxLayerTable() = 0;
		virtual std::map<size_t, float> GetLayerCoefs(std::string coef_name) = 0;
	};


	class DBLoader : public IGrdeclLoader {
	public:
		DBLoader(std::string project_uuid)
		{
			Con = Utils::CreateDBConnection(project_uuid);
		}

		virtual ModelParametrs GetModelParams();
		virtual std::vector<float> LoadPillars();
		virtual std::vector<float> LoadZCORN();
		virtual std::vector<float> LoadValues(std::string value);
		virtual std::vector<std::string> GetAvailableGRIDS();
		virtual std::map<size_t, size_t> GetIdxLayerTable();
		virtual std::map<size_t, float> GetLayerCoefs(std::string coef_name);
	private:
		std::unique_ptr<pqxx::connection> Con;
		std::vector<float> LoadArray(std::string name);

		bool CheckLOExist();

		std::map<size_t, float> GetPVT(pqxx::nontransaction& w);
		std::map<size_t, float> GetOilConversionCoef(pqxx::nontransaction& w);
		std::map<size_t, float> GetDensity(pqxx::nontransaction& w);
		
	};

	class TempGRDLoader : public IGrdeclLoader {
	public:
		virtual ModelParametrs GetModelParams() {
			std::vector<float> arr = LoadArray("SPECGRID");
			return { static_cast<size_t>(arr[0]), static_cast<size_t>(arr[1]), static_cast<size_t>(arr[2]) };
		}
		virtual std::vector<float> LoadPillars() {
			return LoadArray("COORD");
		}
		virtual std::vector<float> LoadZCORN() {
			return LoadArray("ZCORN");
		}
		virtual std::vector<float> LoadValues(std::string value) {
			return LoadArray(value);
		}
		virtual std::vector<std::string> GetAvailableGRIDS() {
			std::vector<std::string> out;
			for (const auto& entry : std::filesystem::directory_iterator("./temp_grdecl"))
				out.push_back(entry.path().filename().string());
			return out;
		}
		virtual std::map<size_t, size_t> GetIdxLayerTable() { return std::map<size_t, size_t>(); }
		virtual std::map<size_t, float> GetLayerCoefs(std::string coef_name) { return std::map<size_t, float>{}; }
	private:
		std::vector<float> LoadArray(std::string name);
	};
}