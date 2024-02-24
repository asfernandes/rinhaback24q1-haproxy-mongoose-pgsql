#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <stack>
#include "pqxx/pqxx"


namespace rinhaback::api
{
	class Connection
	{
	public:
		static constexpr auto POST_TRANSACTION_STMT = "postTransaction";
		static constexpr auto GET_ACCOUNT_STMT = "getAccount";
		static constexpr auto GET_TRANSACTIONS_STMT = "getTransactions";

	public:
		explicit Connection();
		~Connection();

		Connection(const Connection&) = delete;
		Connection& operator=(const Connection&) = delete;

		auto& getConnection()
		{
			return connection;
		}

	private:
		pqxx::connection connection;
	};

	using ConnectionHolder = std::unique_ptr<Connection, std::function<void(Connection*)>>;

	class ConnectionPool
	{
	public:
		explicit ConnectionPool();

		ConnectionPool(const ConnectionPool&) = delete;
		ConnectionPool& operator=(const ConnectionPool&) = delete;

		ConnectionHolder getConnection();

	private:
		void releaseConnection(Connection* connection);

	private:
		std::mutex connectionsMutex;
		std::condition_variable connectionsCondVar;
		std::stack<std::unique_ptr<Connection>> connections;
	};
}  // namespace rinhaback::api
