#ifndef OUTSHINE_APP_PANORAMACAPTURE_H
#define OUTSHINE_APP_PANORAMACAPTURE_H

#include <expected>
#include <string>
#include <string_view>

namespace outshine {
class Engine;

namespace Scenario {
struct Document;
}

namespace App {
struct ScenarioBenchmarkReport;

[[nodiscard]] std::expected<void, std::string>
CheckPanoramaSource(const Scenario::Document &scenario, const ScenarioBenchmarkReport &report);

[[nodiscard]] std::expected<std::string, std::string>
CapturePanorama(Engine &engine, const ScenarioBenchmarkReport &report, std::string_view output);
}
}
#endif
