#include "mimalloc-new-delete.h"
#include "./BankService.h"
#include "./Config.h"
#include "./Database.h"
#include "./Util.h"
#include <array>
#include <atomic>
#include <exception>
#include <format>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <csignal>
#include "mongoose.h"
#include "yyjson.h"

using std::string;
using std::string_view;


namespace rinhaback::api
{
	static constexpr auto RESPONSE_HEADERS = "Content-Type: application/json\r\n";

	static const auto MG_GET = mg_str("GET");
	static const auto MG_POST = mg_str("POST");
	static const auto MG_EXTRATO_PATH = mg_str("/clientes/*/extrato");
	static const auto MG_TRANSACOES_PATH = mg_str("/clientes/*/transacoes");

	static std::atomic_bool finish{false};
	static BankService bankService;

	static void httpHandler(mg_connection* conn, int ev, void* evData)
	{
		struct Response
		{
			int statusCode = 0;
			std::array<char, 2000> json = {'{', '}', '\0'};
		};

		if (ev == MG_EV_HTTP_MSG)
		{
			const auto httpMessage = static_cast<mg_http_message*>(evData);
			const bool isGet = mg_strcmp(httpMessage->method, MG_GET) == 0;
			const bool isPost = !isGet && mg_strcmp(httpMessage->method, MG_POST) == 0;
			mg_str captures[2];

			if (isGet && mg_match(httpMessage->uri, MG_EXTRATO_PATH, captures))
			{
				const auto accountId = parseInt(string_view(captures[0].ptr, captures[0].len)).value_or(-1);

				Response response;
				BankService::GetStatementResponse serviceResponse;

				response.statusCode = (accountId < 0) ? HTTP_STATUS_UNPROCESSABLE_CONTENT
													  : bankService.getStatement(&serviceResponse, accountId);

				if (response.statusCode == HTTP_STATUS_OK)
				{
					auto ptr = response.json.begin();

					ptr = std::format_to_n(ptr, response.json.end() - ptr,
						R"({{"saldo":{{"total":{},"data_extrato":"{}","limite":{}}},"ultimas_transacoes":[)",
						serviceResponse.balance, serviceResponse.date, serviceResponse.overdraft)
							  .out;

					bool first = true;

					for (const auto& transaction : serviceResponse.lastTransactions)
					{
						if (first)
							first = false;
						else
							*ptr++ = ',';

						ptr = std::format_to_n(ptr, response.json.end() - ptr,
							R"({{"valor":{},"tipo":"{}","descricao":"{}","realizada_em":"{}"}})",
							abs(transaction.value), (transaction.value < 0 ? 'd' : 'c'), transaction.description,
							transaction.realized_at)
								  .out;
					}

					*ptr++ = ']';
					*ptr++ = '}';
					*ptr = '\0';
				}

				mg_http_reply(conn, response.statusCode, RESPONSE_HEADERS, "%s", response.json.begin());
			}
			else if (isPost && mg_match(httpMessage->uri, MG_TRANSACOES_PATH, captures))
			{
				const auto accountId = parseInt(string_view(captures[0].ptr, captures[0].len)).value_or(-1);

				Response response;
				response.statusCode = HTTP_STATUS_UNPROCESSABLE_CONTENT;

				const auto inDocJson = yyjson_read(httpMessage->body.ptr, httpMessage->body.len, 0);

				const auto inRootJson = yyjson_doc_get_root(inDocJson);
				const auto valorJson = yyjson_obj_get(inRootJson, "valor");
				const auto tipoJson = yyjson_obj_get(inRootJson, "tipo");
				const auto descricaoJson = yyjson_obj_get(inRootJson, "descricao");

				if (accountId >= 0 && yyjson_is_int(valorJson) && yyjson_is_str(tipoJson) &&
					yyjson_get_len(tipoJson) == 1 && yyjson_is_str(descricaoJson))
				{
					const auto valor = (int) yyjson_get_int(valorJson);
					const auto tipo = *yyjson_get_str(tipoJson);
					const auto descricaoStrView =
						string_view(yyjson_get_str(descricaoJson), yyjson_get_len(descricaoJson));

					if (valor > 0 && (tipo == 'c' || tipo == 'd') && descricaoStrView.length() >= 1 &&
						descricaoStrView.length() <= 10)
					{
						BankService::PostTransactionResponse serviceResponse;

						response.statusCode = bankService.postTransaction(
							&serviceResponse, accountId, (valor * (tipo == 'd' ? -1 : 1)), descricaoStrView);

						if (response.statusCode == HTTP_STATUS_OK)
						{
							auto ptr = response.json.begin();

							ptr = std::format_to_n(ptr, response.json.end() - ptr, R"({{"saldo":{},"limite":{}}})",
								serviceResponse.balance, serviceResponse.overdraft)
									  .out;

							*ptr = '\0';
						}
					}
				}

				yyjson_doc_free(inDocJson);

				mg_http_reply(conn, response.statusCode, RESPONSE_HEADERS, "%s", response.json.begin());
			}
			else
				mg_http_reply(conn, HTTP_STATUS_INTERNAL_SERVER_ERROR, RESPONSE_HEADERS, "{%m:%m}\n", MG_ESC("error"),
					MG_ESC("Unsupported URI"));
		}
	}

	static void signalHandler(int)
	{
		finish = true;
	}

	static int run(int argc, const char* argv[])
	{
		std::signal(SIGINT, signalHandler);
		std::signal(SIGTERM, signalHandler);

		std::vector<std::jthread> threads;

		for (unsigned i = 0; i < Config::netWorkers; ++i)
		{
			threads.emplace_back(
				[]
				{
					databaseConnection.ping();

					mg_mgr mgr;
					mg_mgr_init(&mgr);
					mg_http_listen(&mgr, Config::listenAddress.c_str(), httpHandler, nullptr);

					while (!finish)
						mg_mgr_poll(&mgr, Config::pollTime);

					mg_mgr_free(&mgr);
				});
		}

		std::cout << "Server listening on " << Config::listenAddress << std::endl;

		threads.clear();

		std::cout << "Exiting" << std::endl;

		return 0;
	}
}  // namespace rinhaback::api

int main(const int argc, const char* argv[])
{
	using namespace rinhaback::api;

	try
	{
		return run(argc, argv);
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
}
