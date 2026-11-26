#ifndef OUTSHINE_APP_BENCHMARKINPUT_H
#define OUTSHINE_APP_BENCHMARKINPUT_H

#include <Scenario.h>
#include <chrono>
#include <cstdint>
#include <span>
#include <expected>
#include <string>
#include <string_view>

namespace outshine::App {
struct ScenarioBenchmarkOptions;
struct ScenarioBenchmarkReport;

using BenchmarkClock = std::chrono::steady_clock;

[[nodiscard]] double BenchmarkElapsedMs(BenchmarkClock::time_point since) noexcept;

[[nodiscard]] std::expected<Scenario::Document, std::string>
ReadBenchmarkScenario(const ScenarioBenchmarkOptions &options,
                      std::string_view resourceDirectory,
                      ScenarioBenchmarkReport &report);

[[nodiscard]] std::expected<void, std::string> SaveBenchmarkImage(ScenarioBenchmarkReport &report,
                                                                  std::span<const uint8_t> rgba,
                                                                  std::string_view path);

void CaptureBenchmarkHost(ScenarioBenchmarkReport &report, bool before);

}
#endif
