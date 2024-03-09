#include "./Database.h"
#include "./Config.h"
#include <mutex>


namespace rinhaback::api
{
	DatabaseConnection::DatabaseConnection()
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

	DatabaseConnection::~DatabaseConnection()
	{
		connection.close();
	}
}  // namespace rinhaback::api
