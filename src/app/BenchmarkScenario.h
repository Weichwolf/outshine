#ifndef OUTSHINE_APP_BENCHMARKSCENARIO_H
#define OUTSHINE_APP_BENCHMARKSCENARIO_H

#include "BenchmarkOptions.h"
#include <expected>
#include <string>

namespace outshine::App {
[[nodiscard]] std::expected<std::string, std::string>
RunScenarioBenchmark(const ScenarioBenchmarkOptions &options);
}

#endif
