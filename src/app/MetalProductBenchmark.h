#ifndef OUTSHINE_APP_METALPRODUCTBENCHMARK_H
#define OUTSHINE_APP_METALPRODUCTBENCHMARK_H

#include <expected>
#include <string>

namespace outshine::App {

struct ScenarioBenchmarkOptions;

[[nodiscard]] std::expected<std::string, std::string>
RunMetalProductBenchmark(const ScenarioBenchmarkOptions &options);

}
#endif
