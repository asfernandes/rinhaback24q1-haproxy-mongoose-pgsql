#pragma once

#include <charconv>
#include <chrono>
#include <format>
#include <optional>
#include <string_view>


namespace rinhaback::api
{
	static constexpr int HTTP_STATUS_OK = 200;
	static constexpr int HTTP_STATUS_NOT_FOUND = 404;
	static constexpr int HTTP_STATUS_UNPROCESSABLE_CONTENT = 422;
	static constexpr int HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;

	inline std::string getCurrentDateTimeAsString()
	{
		return std::format(
			"{:%FT%TZ}", std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()));
	}

	inline std::optional<int> parseInt(std::string_view str)
	{
		int val;
		const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
		return ptr == str.end() && ec == std::errc() ? std::optional{val} : std::nullopt;
	}
}  // namespace rinhaback::api
