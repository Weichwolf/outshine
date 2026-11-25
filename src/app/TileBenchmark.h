#ifndef OUTSHINE_APP_TILEBENCHMARK_H
#define OUTSHINE_APP_TILEBENCHMARK_H

#include <expected>
#include <string>

namespace outshine::App {

struct ScenarioBenchmarkOptions;

[[nodiscard]] std::expected<std::string, std::string>
RunTileBenchmark(const ScenarioBenchmarkOptions &options);

}
#endif
