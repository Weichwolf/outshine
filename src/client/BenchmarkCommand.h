#ifndef OUTSHINE_CLIENT_BENCHMARKCOMMAND_H
#define OUTSHINE_CLIENT_BENCHMARKCOMMAND_H

#include "BenchmarkOptions.h"
#include <expected>
#include <span>
#include <string_view>

namespace outshine::Client {

[[nodiscard]] std::expected<BenchmarkOptions, std::string_view>
ReadBenchmarkOptions(std::span<const char *const> arguments) noexcept;
[[nodiscard]] int RunBenchmarkCommand(std::span<const char *const> arguments);

}

#endif
