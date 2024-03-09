#pragma once

#include "pqxx/pqxx"


namespace rinhaback::api
{
	class DatabaseConnection
	{
	public:
		static constexpr auto POST_TRANSACTION_STMT = "postTransaction";
		static constexpr auto GET_ACCOUNT_STMT = "getAccount";
		static constexpr auto GET_TRANSACTIONS_STMT = "getTransactions";

	public:
		explicit DatabaseConnection();
		~DatabaseConnection();

		DatabaseConnection(const DatabaseConnection&) = delete;
		DatabaseConnection& operator=(const DatabaseConnection&) = delete;

	public:
		void ping() { }

		auto& getConnection()
		{
			return connection;
		}

	private:
		pqxx::connection connection;
	};

	inline thread_local DatabaseConnection databaseConnection;
}  // namespace rinhaback::api
