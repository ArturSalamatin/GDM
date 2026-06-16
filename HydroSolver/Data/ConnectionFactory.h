#pragma once
#include "../stdafx.h"
#include "DBUtils.h"

namespace reservoir_simulator
{
	namespace factories
	{
		/// <summary>
		/// The factory stores connection to a database
		/// </summary>
		class ConnectionFactory
		{
		public:
			ConnectionFactory(
				const std::string& connection_name) noexcept;

			grdecl_memory::DBLoader& Loader();
			const std::string& uuid();
		protected:
			std::string connection_name;
			grdecl_memory::DBLoader db_loader;
		};
	} // factories
} // reservoir_simulator