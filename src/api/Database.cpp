#include "./Database.h"
#include "./Config.h"
#include <mutex>


namespace rinhaback::api
{
	Connection::Connection()
	{
		connection.prepare(POST_TRANSACTION_STMT,
			R"""(
			select post_transaction($1, $2, $3)
			)""");

		connection.prepare(GET_ACCOUNT_STMT,
			R"""(
			select balance, overdraft
			    from account
			    where id = $1
			)""");

		connection.prepare(GET_TRANSACTIONS_STMT,
			R"""(
			select val, description, to_char(datetime, 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"') datetime
			    from transaction
			    where account_id = $1
			    order by id desc
			    limit 10
			)""");
	}

	Connection::~Connection()
	{
		connection.close();
	}


	ConnectionPool::ConnectionPool()
	{
		for (unsigned i = 0; i < Config::dbWorkers; ++i)
			connections.emplace(std::make_unique<Connection>());
	}

	ConnectionHolder ConnectionPool::getConnection()
	{
		std::unique_lock lock(connectionsMutex);

		connectionsCondVar.wait(lock, [this] { return !connections.empty(); });
		assert(!connections.empty());

		auto connection = std::move(connections.top());
		connections.pop();

		lock.unlock();

		return {connection.release(), [this](auto connection) { releaseConnection(connection); }};
	}

	void ConnectionPool::releaseConnection(Connection* connection)
	{
		std::unique_lock lock(connectionsMutex);
		connections.emplace(connection);

		lock.unlock();
		connectionsCondVar.notify_one();
	}
}  // namespace rinhaback::api
