#pragma once

#include <string>
#include <cstdlib>


namespace rinhaback::api
{
	class Config
	{
	private:
		static std::string readEnv(const char* name, const char* defaultVal)
		{
			const auto val = std::getenv(name);
			return val ? val : defaultVal;
		}

	public:
		Config() = delete;

	public:
		static inline const auto netWorkers = (unsigned) std::stoi(readEnv("NET_WORKERS", "1"));
		static inline const auto pollTime = (unsigned) std::stoi(readEnv("POLL_TIME", "1"));
		static inline const auto listenAddress = readEnv("LISTEN_ADDRESS", "127.0.0.1:8080");
	};
}  // namespace rinhaback::api
