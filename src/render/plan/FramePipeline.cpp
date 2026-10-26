#include "FramePipeline.h"
#include <algorithm>
#include <cmath>
#include <expected>
#include <string>
#include <optional>
#include <utility>

namespace outshine::Render {
namespace {
constexpr double kExposureCalibration = 1.2;
constexpr double kMeteredMiddleGrey = 2.5;

bool DeclareStages(const FramePipelineSettings &settings,
                   PlanSpec &declaration,
                   std::string &error) {
  declaration.Outputs = {Resource::FrameTex};
  if (settings.Presents) { declaration.Outputs.push_back(Resource::Surface); }
  for (const std::string &named : settings.Outputs) {
    const std::optional<Resource> row = ExecutionPlan::ResourceByName(named);
    if (!row) {
      error = "the declaration asks the frame to keep '" + named +
              "', and the catalogue holds no picture by that name -- an output list is checked "
              "against the catalogue because a typo that silently drops a buffer leaves a client "
              "reading zeros and calling them a measurement";
      return false;
    }
    bool already = false;
    for (const Resource held : declaration.Outputs) { already = already || held == *row; }
    if (!already) { declaration.Outputs.push_back(*row); }
  }

  if (!settings.Stages.empty()) {

    declaration.Content.clear();
    for (const std::string &named : settings.Stages) {
      const std::optional<Stage> row = ExecutionPlan::StageByName(named);
      if (!row) {
        error = "the declaration names render stage '" + named +
                "', and the catalogue holds no row by that name -- a stage list is checked "
                "against the catalogue because a typo that silently drops a pass is a picture "
                "nobody can explain";
        return false;
      }
      declaration.Content.push_back(*row);
    }
    declaration.Display = Declared<Transfer>(Transfer::Filmic);
    declaration.Exposure = Declared<float>(1.0f);
    return true;
  }
  declaration.Outputs.push_back(Resource::SceneVelocity);
  declaration.Content = {Stage::OpaqueGeometry, Stage::Overlay};
  if (settings.DrawsSky) {
    declaration.Content.push_back(Stage::Sky);
    declaration.Content.push_back(Stage::AerialPerspective);
  }
  declaration.Content.push_back(Stage::LightVisibility);
  if (settings.Transmissive) {
    declaration.Content.push_back(Stage::TransmissiveGeometry);
    declaration.Content.push_back(Stage::CompositeTransmission);
  }

  declaration.Display = Declared<Transfer>(Transfer::Filmic);
  declaration.Exposure = Declared<float>(1.0f);
  return true;
}

}

std::expected<PlanSpec, std::string> BuildFramePipeline(const FramePipelineSettings &settings) {
  PlanSpec declaration;
  std::string error;
  if (!DeclareStages(settings, declaration, error)) { return std::unexpected(std::move(error)); }
  if (!settings.Antialiasing.empty() && settings.Antialiasing != "none" &&
      settings.Antialiasing != "temporal") {
    error = "render antialiasing must be none or temporal";
    return std::unexpected(std::move(error));
  }
  if (settings.Antialiasing == "temporal" &&
      std::ranges::find(declaration.Content, Stage::TemporalResolve) == declaration.Content.end()) {
    declaration.Content.push_back(Stage::TemporalResolve);
  }
  if (!settings.Transfer.empty()) {
    const std::optional<Transfer> meant = Spells(kTransfers, settings.Transfer);
    if (!meant) {
      error = "the declaration transfers the frame as '" + std::string(settings.Transfer) +
              "', and this engine spells " + Spellings(kTransfers);
      return std::unexpected(std::move(error));
    }
    declaration.Display = Declared<Transfer>(*meant);
  }
  if (!settings.Precision.empty()) {
    const std::optional<ScenePrecision> meant = Spells(kPrecisions, settings.Precision);
    if (!meant) {
      error = "the declaration carries the scene at '" + std::string(settings.Precision) +
              "' precision, and this engine spells " + Spellings(kPrecisions);
      return std::unexpected(std::move(error));
    }
    declaration.Precision = Declared<ScenePrecision>(*meant);
  }
  if (settings.Exposure > 0.0) {
    declaration.Exposure = Declared<float>(static_cast<float>(settings.Exposure));
  } else {
    const double metered = settings.IlluminanceLx;
    if (metered > 0.0) {
      const double ev100 = std::log2(metered / kMeteredMiddleGrey);
      declaration.Exposure =
          Declared<float>(static_cast<float>(1.0 / (kExposureCalibration * std::pow(2.0, ev100))));
    }
  }
  return declaration;
}

}
