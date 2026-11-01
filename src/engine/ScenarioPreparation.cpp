#include "ScenarioPreparation.h"
#include "EngineState.h"
#include "GroundView.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace outshine {
namespace {
[[nodiscard]] bool SameUiSurface(const Ui::SurfaceDeclaration &a, const Ui::SurfaceDeclaration &b) {
  return a.Markup == b.Markup && a.Style == b.Style && a.Programme == b.Programme &&
         a.LeftFrac == b.LeftFrac && a.TopFrac == b.TopFrac && a.WidthFrac == b.WidthFrac &&
         a.HeightFrac == b.HeightFrac;
}

[[nodiscard]] bool SameSurfaces(const std::vector<Ui::SurfaceDeclaration> &a,
                                const std::vector<Ui::SurfaceDeclaration> &b) {
  if (a.size() != b.size()) { return false; }
  for (size_t at = 0; at < a.size(); ++at) {
    if (!SameUiSurface(a[at], b[at])) { return false; }
  }
  return true;
}

[[nodiscard]] bool HasGeneratedContent(const Scenario::Document &scenario) {
  return !scenario.Generators.empty() ||
         std::ranges::any_of(scenario.Assets, [](const Scenario::Asset &asset) {
           return asset.Kind == "generated";
         });
}

[[nodiscard]] bool SamePicture(const Core::Declaration &a, const Core::Declaration &b);

void PublishConfiguration(ScenarioSession &session,
                          Scenario::Document scenario,
                          std::vector<std::string> carried,
                          std::optional<ViewBook> &views,
                          InputMap &bindings) {
  session.Declared = std::move(scenario);
  ++session.DeclarationRevision;
  session.AudioBodies.clear();
  session.Sounding.reset();
  session.Taken = true;
  session.Carried = std::move(carried);
  static_assert(std::is_nothrow_move_assignable_v<std::optional<ViewBook>>);
  session.Views = std::move(views);
  static_assert(std::is_nothrow_move_assignable_v<InputMap>);
  session.Bound = std::move(bindings);
}

[[nodiscard]] bool SameRenderPlan(const Core::Declaration &a, const Core::Declaration &b) {
  return a.Stages == b.Stages && a.Outputs == b.Outputs && a.Transfer == b.Transfer &&
         a.Precision == b.Precision && a.Antialiasing == b.Antialiasing &&
         a.Exposure == b.Exposure && a.Content == b.Content;
}

[[nodiscard]] bool SamePicture(const Core::Declaration &a, const Core::Declaration &b) {
  return SameRenderPlan(a, b) && a.Haze == b.Haze && a.SurfaceWidthPx == b.SurfaceWidthPx &&
         a.SurfaceHeightPx == b.SurfaceHeightPx && a.InitialGeometry == b.InitialGeometry &&
         a.MetresPerUnit == b.MetresPerUnit && a.Fps == b.Fps && a.Fill == b.Fill &&
         a.OrbitDegPerFrame == b.OrbitDegPerFrame && a.PictureLeftFrac == b.PictureLeftFrac &&
         a.PictureTopFrac == b.PictureTopFrac && a.PictureWidthFrac == b.PictureWidthFrac &&
         a.PictureHeightFrac == b.PictureHeightFrac && a.IndirectLight[0] == b.IndirectLight[0] &&
         a.IndirectLight[1] == b.IndirectLight[1] && a.IndirectLight[2] == b.IndirectLight[2] &&
         a.KeyLux == b.KeyLux && a.KeyFromClock == b.KeyFromClock && a.DrawsSky == b.DrawsSky &&
         a.ShadowRadiusM == b.ShadowRadiusM && a.KeyElevationDeg == b.KeyElevationDeg &&
         a.KeyBearingDeg == b.KeyBearingDeg;
}

}

ScenarioPreparation::ScenarioPreparation() = default;
ScenarioPreparation::~ScenarioPreparation() = default;

Result PreparedRuntimeDeclaration::ValidateGroundView(const Scenario::Document &scenario,
                                                      int heightPx) const {
  if (!scenario.Ground.Declared || !scenario.Views.empty() || !Targeted) { return {}; }
  if (ValidGroundView(Targeted->View(), heightPx)) { return {}; }
  return std::unexpected(std::string(kInvalidGroundView));
}

Result PreparedRuntimeDeclaration::Publish(EngineRenderState &picture,
                                           WorldRegionState &world,
                                           Core::Declaration declared) {
  if (Targeted) {
    if (auto published = Targeted->Publish(picture.Standing); !published) { return published; }
  }
  picture.Shown = std::move(declared);
  if (!picture.Targeted) {
    picture.PendingGeometry = std::move(Headless.Geometry);
    picture.PendingAudioOcclusion = std::move(Headless.Occlusion);
  } else {
    world.AudioOcclusion = Headless.Occlusion ? std::move(*Headless.Occlusion) : TriangleBvh{};
  }
  return {};
}

std::optional<Result> ScenarioPreparation::TryReuse(EngineRenderState &picture,
                                                    ScenarioSession &session,
                                                    std::string &error) {
  if (!picture.Standing || Declared.Ground.Declared || session.Declared.Ground.Declared ||
      HasGeneratedContent(Declared) || HasGeneratedContent(session.Declared) ||
      !SamePicture(picture.Shown, Rendering) ||
      session.Declared.Ground.VegetationEnabled != Declared.Ground.VegetationEnabled ||
      session.Declared.Providers != Declared.Providers ||
      session.Declared.Routes != Declared.Routes) {
    return std::nullopt;
  }
  if (!SameSurfaces(picture.Shown.Surfaces, Rendering.Surfaces) &&
      !picture.Standing->Redeclare(Rendering.Surfaces, error)) {
    return Result{std::unexpected(error)};
  }
  if (View) { picture.Standing->Eye(*View); }
  picture.Shown = std::move(Rendering);
  error.clear();
  PublishConfiguration(session, std::move(Declared), std::move(Carried), Views, Bindings);
  return Result{};
}

Result ScenarioPreparation::Publish(EngineRenderState &picture,
                                    ScenarioSession &session,
                                    std::unique_ptr<WorldRegionState> &activeWorld) {
  if (auto published = Runtime.Publish(picture, *World, std::move(Rendering)); !published) {
    return published;
  }
  activeWorld = std::move(World);
  PublishConfiguration(session, std::move(Declared), std::move(Carried), Views, Bindings);
  return {};
}

}
