#ifndef OUTSHINE_CLIENT_BENCHMARKSCENARIO_H
#define OUTSHINE_CLIENT_BENCHMARKSCENARIO_H

#include "BenchmarkOptions.h"
#include <expected>
#include <string>

namespace outshine::Client {
[[nodiscard]] std::expected<std::string, std::string>
RunScenarioBenchmark(const ScenarioBenchmarkOptions &options);
}

#endif
