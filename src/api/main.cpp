#include "mimalloc-new-delete.h"
#include "./BankService.h"
#include "./Config.h"
#include "./Util.h"
#include <atomic>
#include <exception>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <csignal>
#include "mongoose.h"
#include "task_thread_pool.hpp"
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
	static task_thread_pool::task_thread_pool threadPool{Config::netWorkers};
	static BankService bankService;

	static void httpHandler(mg_connection* conn, int ev, void* evData)
	{
		struct AsyncResponse
		{
			int statusCode = 0;
			char* json = nullptr;
		};

		if (ev == MG_EV_HTTP_MSG)
		{
			const auto httpMessage = (mg_http_message*) evData;
			const bool isGet = mg_strcmp(httpMessage->method, MG_GET) == 0;
			const bool isPost = !isGet && mg_strcmp(httpMessage->method, MG_POST) == 0;
			mg_str captures[2];

			if (isGet && mg_match(httpMessage->uri, MG_EXTRATO_PATH, captures))
			{
				const auto accountId = parseInt(string_view(captures[0].ptr, captures[0].len)).value_or(-1);

				threadPool.submit_detach(
					[accountId](const auto connId, const auto mgr)
					{
						AsyncResponse asyncResponse;
						BankService::GetStatementResponse serviceResponse;

						asyncResponse.statusCode = (accountId < 0)
							? HTTP_STATUS_UNPROCESSABLE_CONTENT
							: bankService.getStatement(&serviceResponse, accountId);

						if (asyncResponse.statusCode == HTTP_STATUS_OK)
						{
							const auto docJson = yyjson_mut_doc_new(nullptr);
							const auto rootJson = yyjson_mut_obj(docJson);
							yyjson_mut_doc_set_root(docJson, rootJson);

							const auto saldoJson = yyjson_mut_obj_add_obj(docJson, rootJson, "saldo");
							yyjson_mut_obj_add_int(docJson, saldoJson, "total", serviceResponse.balance);
							yyjson_mut_obj_add_int(docJson, saldoJson, "limite", serviceResponse.overdraft);
							yyjson_mut_obj_add_str(docJson, saldoJson, "data_extrato", serviceResponse.date.c_str());

							const auto ultimasTransacoesJson =
								yyjson_mut_obj_add_arr(docJson, rootJson, "ultimas_transacoes");

							for (const auto& transaction : serviceResponse.lastTransactions)
							{
								const auto ultimaTransacaoJson = yyjson_mut_obj(docJson);
								yyjson_mut_obj_add_int(docJson, ultimaTransacaoJson, "valor", abs(transaction.value));
								yyjson_mut_obj_add_str(
									docJson, ultimaTransacaoJson, "tipo", (transaction.value < 0 ? "d" : "c"));
								yyjson_mut_obj_add_str(
									docJson, ultimaTransacaoJson, "descricao", transaction.description.c_str());
								yyjson_mut_obj_add_str(
									docJson, ultimaTransacaoJson, "realizada_em", transaction.realized_at.c_str());

								yyjson_mut_arr_append(ultimasTransacoesJson, ultimaTransacaoJson);
							}

							asyncResponse.json = yyjson_mut_write(docJson, 0, nullptr);
							yyjson_mut_doc_free(docJson);
						}

						mg_wakeup(mgr, connId, &asyncResponse, sizeof(asyncResponse));
					},
					conn->id, conn->mgr);
			}
			else if (isPost && mg_match(httpMessage->uri, MG_TRANSACOES_PATH, captures))
			{
				const auto accountId = parseInt(string_view(captures[0].ptr, captures[0].len)).value_or(-1);
				const auto bodyCopy = mg_strdup(httpMessage->body);

				threadPool.submit_detach(
					[accountId](const auto connId, const auto mgr, const auto bodyCopy)
					{
						AsyncResponse asyncResponse;
						asyncResponse.statusCode = HTTP_STATUS_UNPROCESSABLE_CONTENT;

						const auto inDocJson = yyjson_read(bodyCopy.ptr, bodyCopy.len, 0);

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

								asyncResponse.statusCode = bankService.postTransaction(
									&serviceResponse, accountId, (valor * (tipo == 'd' ? -1 : 1)), descricaoStrView);

								if (asyncResponse.statusCode == HTTP_STATUS_OK)
								{
									const auto outDocJson = yyjson_mut_doc_new(nullptr);
									const auto outRootJson = yyjson_mut_obj(outDocJson);
									yyjson_mut_doc_set_root(outDocJson, outRootJson);

									yyjson_mut_obj_add_int(outDocJson, outRootJson, "saldo", serviceResponse.balance);
									yyjson_mut_obj_add_int(
										outDocJson, outRootJson, "limite", serviceResponse.overdraft);

									asyncResponse.json = yyjson_mut_write(outDocJson, 0, nullptr);
									yyjson_mut_doc_free(outDocJson);
								}
							}
						}

						mg_wakeup(mgr, connId, &asyncResponse, sizeof(asyncResponse));

						yyjson_doc_free(inDocJson);
						free(const_cast<char*>(bodyCopy.ptr));
					},
					conn->id, conn->mgr, bodyCopy);
			}
			else
				mg_http_reply(conn, HTTP_STATUS_INTERNAL_SERVER_ERROR, RESPONSE_HEADERS, "{%m:%m}\n", MG_ESC("error"),
					MG_ESC("Unsupported URI"));
		}
		else if (ev == MG_EV_WAKEUP)
		{
			const auto asyncResponse = reinterpret_cast<const AsyncResponse*>(static_cast<mg_str*>(evData)->ptr);
			mg_http_reply(conn, asyncResponse->statusCode, RESPONSE_HEADERS, "%s",
				(asyncResponse->json ? asyncResponse->json : "{}"));
			free(asyncResponse->json);
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

		mg_mgr mgr;
		mg_mgr_init(&mgr);
		mg_http_listen(&mgr, Config::listenAddress.c_str(), httpHandler, nullptr);
		mg_wakeup_init(&mgr);

		std::cout << "Server listening on " << Config::listenAddress << std::endl;

		while (!finish)
			mg_mgr_poll(&mgr, Config::pollTime);

		std::cout << "Exiting" << std::endl;

		mg_mgr_free(&mgr);

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
