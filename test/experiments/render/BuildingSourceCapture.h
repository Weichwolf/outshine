#ifndef OUTSHINE_TEST_BUILDING_SOURCE_CAPTURE_H
#define OUTSHINE_TEST_BUILDING_SOURCE_CAPTURE_H

#include "ImpostorAtlas.h"
#include "BuildingSourceBudget.h"
#include <nlohmann/json_fwd.hpp>
#include <cstddef>
#include <filesystem>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace outshine::Render {
struct ImpostorCapture;
struct ImpostorCaptureProjection;
}

namespace outshine::Test {
class BuildingSourceCapture {
public:
  BuildingSourceCapture(const std::filesystem::path &input, const nlohmann::json &request);
  ~BuildingSourceCapture();
  BuildingSourceCapture(const BuildingSourceCapture &) = delete;
  BuildingSourceCapture &operator=(const BuildingSourceCapture &) = delete;

  [[nodiscard]] const Render::ImpostorCapture &Metadata() const noexcept;
  [[nodiscard]] const Render::ImpostorCaptureProjection &Projection() const noexcept;
  [[nodiscard]] const nlohmann::json &Report() const;
  [[nodiscard]] std::expected<void, std::string>
  ExportOriginals(Generators::BuildingSourceBudget budget,
                  const std::filesystem::path &output,
                  const std::stop_token &stop = {});
  [[nodiscard]] ImpostorAtlas Capture(std::optional<size_t> view = std::nullopt,
                                      const std::filesystem::path &geometryOutput = {});

private:
  struct State;
  std::unique_ptr<State> State_;
};
}
#endif
