#include "./BankService.h"
#include "./Config.h"
#include "./Database.h"
#include "./Util.h"
#include <string>
#include <string_view>
#include "pqxx/pqxx"

using std::string;
using std::string_view;


namespace rinhaback::api
{
	int BankService::postTransaction(
		PostTransactionResponse* response, int accountId, int value, string_view description)
	{
		auto& connection = databaseConnection.getConnection();
		pqxx::nontransaction tx{connection};

		const auto result = tx.exec_prepared(DatabaseConnection::POST_TRANSACTION_STMT, accountId, value, description);
		const auto resultStr = result[0][0].as<pqxx::zview>();
		pqxx::array_parser array{std::string_view(resultStr.c_str() + 1, resultStr.size() - 2)};
		const auto status = std::stoi(array.get_next().second);

		if (status != HTTP_STATUS_OK)
			return status;

		const auto balance = std::stoi(array.get_next().second);
		const auto overdraft = std::stoi(array.get_next().second);

		response->balance = balance;
		response->overdraft = -overdraft;

		return HTTP_STATUS_OK;
	}

	int BankService::getStatement(GetStatementResponse* response, int accountId)
	{
		auto& connection = databaseConnection.getConnection();
		pqxx::nontransaction tx{connection};

		const auto accountResult = tx.exec_prepared(DatabaseConnection::GET_ACCOUNT_STMT, accountId);

		if (accountResult.empty())
			return HTTP_STATUS_NOT_FOUND;

		const auto balance = accountResult[0][0].as<int>();
		const auto overdraft = accountResult[0][1].as<int>();

		response->balance = balance;
		response->overdraft = -overdraft;
		response->date = getCurrentDateTimeAsString();

		for (const auto transactionResult : tx.exec_prepared(DatabaseConnection::GET_TRANSACTIONS_STMT, accountId))
		{
			const auto value = transactionResult[0].as<int>();
			const auto description = transactionResult[1].as<pqxx::zview>();
			const auto date = transactionResult[2].as<pqxx::zview>();
			response->lastTransactions.emplace_back(value, string(description), string(date));
		}

		return HTTP_STATUS_OK;
	}
}  // namespace rinhaback::api
