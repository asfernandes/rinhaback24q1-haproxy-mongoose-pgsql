#pragma once

#include <string>
#include <string_view>
#include <vector>


namespace rinhaback::api
{
	class BankService
	{
	public:
		struct Transaction
		{
			int value;
			std::string description;
			std::string realized_at;
		};

		struct PostTransactionResponse
		{
			int overdraft;
			int balance;
		};

		struct GetStatementResponse
		{
			std::string date;
			int overdraft;
			int balance;
			std::vector<Transaction> lastTransactions;
		};

	public:
		int postTransaction(PostTransactionResponse* response, int accountId, int value, std::string_view description);
		int getStatement(GetStatementResponse* response, int accountId);
	};
}  // namespace rinhaback::api