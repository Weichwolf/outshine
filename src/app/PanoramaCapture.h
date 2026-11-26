#ifndef OUTSHINE_APP_PANORAMACAPTURE_H
#define OUTSHINE_APP_PANORAMACAPTURE_H

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace outshine {
class Engine;
struct FarPanorama;

namespace Render {
struct MetalProductInputs;
}

namespace Scenario {
struct Document;
}

namespace App {
struct ScenarioBenchmarkReport;

[[nodiscard]] std::expected<void, std::string>
CheckPanoramaSource(const Scenario::Document &scenario, const ScenarioBenchmarkReport &report);

[[nodiscard]] std::expected<std::string, std::string>
CapturePanorama(Engine &engine, const ScenarioBenchmarkReport &report, std::string_view output);

#ifdef __APPLE__
[[nodiscard]] std::expected<std::string, std::string>
CapturePreparedSurfacePanorama(const Render::MetalProductInputs &surfaces,
                               const FarPanorama &reference,
                               const ScenarioBenchmarkReport &report,
                               std::string_view productRevision,
                               const std::filesystem::path &directory);
#endif
}
}
#endif
