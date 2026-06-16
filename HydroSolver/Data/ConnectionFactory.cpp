#include "ConnectionFactory.h"

namespace reservoir_simulator
{
	namespace factories
	{
		/////////////////// ConnectionFactory
		ConnectionFactory::ConnectionFactory(const std::string& connection_name) noexcept :
			connection_name{ connection_name }, db_loader{ connection_name }
		{}

		grdecl_memory::DBLoader& ConnectionFactory::Loader() { return db_loader; }
		const std::string& ConnectionFactory::uuid() { return connection_name; }

	} // factories
} // reservoir_simulator