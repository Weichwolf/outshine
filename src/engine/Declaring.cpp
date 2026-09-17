#include <limits>
#include <type_traits>
#include <cmath>
#include "ScenarioWrite.h"
#include "AssetValidation.h"
#include "WeatherValidation.h"
#include "PlayerValidation.h"
#include "BodyValidation.h"
#include "WorldValidation.h"
#include "OsmValidation.h"
#include "AudioOcclusion.h"
#include "EngineHeld.h"
#include "ReadTextFile.h"
#include "ActionHostAdapter.h"
#include "Ephemeris.h"
#include "CivilTime.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <expected>
#include <exception>
#include <numeric>
#include <memory>
#include <optional>
#include <string>
#include <span>
#include <vector>
#include <utility>
#include <string_view>

namespace outshine {

namespace Says {
constexpr auto kUnknownGenerator = "no generator registered for kind: ";
constexpr auto kInvalidWheelStep = "wheelStepPx requires a finite nonnegative value";
constexpr auto kInvalidWheelEvent = "wheel position and pixel displacement must be finite";
constexpr auto kInputHostMissing = "a bound input action requires an offered host";
}

namespace {

[[nodiscard]] std::vector<Core::Shows>
PrepareSurfaces(std::span<const Scenario::Surface> surfaces) {
  std::vector<size_t> ordered(surfaces.size());
  std::ranges::iota(ordered, size_t{0});
  std::ranges::stable_sort(
      ordered, [&surfaces](size_t a, size_t b) { return surfaces[a].Z < surfaces[b].Z; });
  std::vector<Core::Shows> laid;
  laid.reserve(ordered.size());
  for (const size_t at : ordered) {
    const Scenario::Surface *const surface = &surfaces[at];
    Core::Shows shows;
    shows.Markup = surface->Document;
    shows.Style = surface->Style;
    shows.Programme = surface->Programme;
    shows.LeftFrac = surface->Where.LeftFrac;
    shows.TopFrac = surface->Where.TopFrac;
    shows.WidthFrac = surface->Where.WidthFrac;
    shows.HeightFrac = surface->Where.HeightFrac;
    laid.push_back(std::move(shows));
  }
  return laid;
}

[[nodiscard]] Holds<bool>
DispatchInput(Host *host, const InputMap &bindings, std::span<const Core::InputPump::Fired> fired) {
  if (host == nullptr) { return std::unexpected(Says::kInputHostMissing); }
  bool acted = false;
  for (const auto &action : fired) {
    const std::string *const named = bindings.ActionNamed(action.Action);
    if (named == nullptr) { continue; }
    const Argument value{
        .Is = Argument::Kind::Number, .Number = static_cast<double>(action.Value), .Text = {}};
    acted = host->calls(*named, std::span<const Argument>(&value, 1)) || acted;
  }
  return acted;
}

}

Holds<bool> Engine::handleEvent(const SDL_Event &event) {
  if (const auto permission = S_->MutationPermission(); !permission) {
    return std::unexpected(permission.error());
  }
  if (S_->Picture.Standing && event.type == SDL_EVENT_MOUSE_WHEEL) {
    const double displacementPx =
        -static_cast<double>(event.wheel.y) * S_->Session.Declared.WheelStepPx;
    if (!std::isfinite(event.wheel.mouse_x) || !std::isfinite(event.wheel.mouse_y) ||
        !std::isfinite(event.wheel.y) || !std::isfinite(displacementPx)) {
      return std::unexpected(Says::kInvalidWheelEvent);
    }
    return S_->Picture.Standing->Wheeled(static_cast<double>(event.wheel.mouse_x),
                                         static_cast<double>(event.wheel.mouse_y),
                                         displacementPx,
                                         S_->Error);
  }
  std::array<Core::InputPump::Fired, 2> fired{};
  const size_t many = Core::InputPump::Translate(event, S_->Session.Bound, fired);
  if (many != 0) {
    return DispatchInput(S_->Offered,
                         S_->Session.Bound,
                         std::span<const Core::InputPump::Fired>(fired.data(), many));
  }
  if (!S_->Picture.Standing || event.type != SDL_EVENT_MOUSE_BUTTON_DOWN) { return false; }

  size_t surface = 0;
  const Ui::Touched found = S_->Picture.Standing->Under(
      static_cast<double>(event.button.x), static_cast<double>(event.button.y), surface);
  if (!found.Held() || found.Action.empty()) { return false; }
  const std::string &action = found.Action;
  if (S_->Offered == nullptr) {
    S_->Error = "a surface declares the call '" + action +
                "' and no host was offered to answer it -- the client calls Offers before it "
                "hands an event in";
    return std::unexpected(S_->Error);
  }

  Script::Program programme;
  const std::string text = S_->Picture.Standing->ProgrammeOf(surface) + "\n" + action + ";\n";
  if (!programme.Read(text, S_->Error)) { return std::unexpected(S_->Error); }
  ActionHostAdapter answering(S_->Offered);
  if (!programme.Run(answering, S_->Error)) { return std::unexpected(S_->Error); }
  return answering.Fired();
}

Result Engine::setSurfaces(std::span<const Scenario::Surface> surfaces) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands, so there is no picture for a surface to be laid over -- a "
                "scenario is declared before its surfaces are exchanged";
    return std::unexpected(S_->Error);
  }
  if (!surfaces.empty() &&
      !S_->Picture.Face.Opens(S_->Session.Under.Shipped + "/fonts", S_->Error)) {
    return std::unexpected(S_->Error);
  }
  std::vector<Scenario::Surface> candidate(surfaces.begin(), surfaces.end());
  auto laid = PrepareSurfaces(candidate);
  if (!S_->Picture.Standing->Redeclare(laid, S_->Error)) { return std::unexpected(S_->Error); }
  S_->Session.Declared.Surfaces = std::move(candidate);
  S_->Picture.Shown.Surfaces = std::move(laid);
  return {};
}

namespace {

[[nodiscard]] bool SameShows(const Core::Shows &a, const Core::Shows &b) {
  return a.Markup == b.Markup && a.Style == b.Style && a.Programme == b.Programme &&
         a.LeftFrac == b.LeftFrac && a.TopFrac == b.TopFrac && a.WidthFrac == b.WidthFrac &&
         a.HeightFrac == b.HeightFrac;
}

[[nodiscard]] bool SameSurfaces(const std::vector<Core::Shows> &a,
                                const std::vector<Core::Shows> &b) {
  if (a.size() != b.size()) { return false; }
  for (size_t at = 0; at < a.size(); ++at) {
    if (!SameShows(a[at], b[at])) { return false; }
  }
  return true;
}

[[nodiscard]] bool HasGeneratedContent(const Scenario::Document &scenario) {
  return !scenario.Generators.empty() ||
         std::ranges::any_of(scenario.Assets, [](const Scenario::Asset &asset) {
           return asset.Kind == "generated";
         });
}

[[nodiscard]] std::expected<std::optional<Geometry>, std::string>
BuildGeneratedGeometry(const Scenario::Document &scenario,
                       const Generators::Registry &registry,
                       const GroundQuery &ground) {
  class Stands final : public Generators::HeightSampler {
  public:
    explicit Stands(const GroundQuery &from) noexcept : From_(from) {}

    [[nodiscard]] std::optional<double>
    sampleHeightAslM(const LongitudeLatitudeHeight &at) const override {
      return From_.At({.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg}).AslM();
    }

  private:
    const GroundQuery &From_;
  };

  const Stands stands(ground);
  Generators::Request asked;
  asked.LatitudeDeg = scenario.Ground.Origin.LatitudeDeg;
  asked.LongitudeDeg = scenario.Ground.Origin.LongitudeDeg;
  asked.ExtentM = scenario.Ground.Origin.RadiusM;
  asked.Ground = &stands;
  Geometry made;
  const auto offered =
      [&registry, &asked, &made](
          const std::string &kind,
          std::span<const Scenario::Setting> settings) -> std::expected<void, std::string> {
    const Generators::Generator *const stood = registry.named(kind);
    if (stood == nullptr) { return std::unexpected(Says::kUnknownGenerator + kind); }
    std::vector<Generators::Parameter> parameters;
    parameters.reserve(settings.size());
    for (const auto &setting : settings) {
      parameters.push_back({.Name = setting.Name, .Value = setting.Value});
    }
    auto request = asked;
    request.Parameters = parameters;
    auto product = stood->make(request);
    if (!product) {
      return std::unexpected("the generator of kind '" + kind + "' refused: " + product.error());
    }
    if (product->parts() == 0 && product->surfaces() == 0 && product->images() == 0 &&
        product->lamps() == 0) {
      return {};
    }
    if (!made.append(*product)) {
      return std::unexpected("the generator of kind '" + kind + "' made unpublishable geometry");
    }
    return {};
  };
  for (const Scenario::Asset &shown : scenario.Assets) {
    if (shown.Kind != "generated") { continue; }
    if (const auto product = offered(shown.Uri, {}); !product) {
      return std::unexpected(product.error());
    }
  }
  for (const Scenario::Generating &named : scenario.Generators) {
    if (const auto product = offered(named.Kind, named.Parameters); !product) {
      return std::unexpected(product.error());
    }
  }
  if (made.parts() == 0) { return std::optional<Geometry>{}; }
  return std::optional<Geometry>{std::move(made)};
}

struct HeadlessDeclaration {
  std::optional<Geometry> Geometry;
  std::optional<TriangleBvh> Occlusion;
};

[[nodiscard]] bool
PrepareTargetedDeclaration(Render::SceneRenderer &renderer,
                           const Core::Declaration &declared,
                           const Ui::Font *font,
                           HeadlessDeclaration &headless,
                           std::vector<std::vector<Ui::Layout::Scrolled>> &wasScrolled,
                           std::unique_ptr<Core::Live> &candidate,
                           std::string &error) {
  if (!renderer.BeginsWorldCandidate(error)) { return false; }
  if (!Core::Live::Prepare(renderer, declared, font, candidate, error)) {
    renderer.AbandonsWorldCandidate();
    return false;
  }
  if (!wasScrolled.empty() && !candidate->Scrolled(std::move(wasScrolled), error)) {
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    return false;
  }
  if (headless.Geometry && !candidate->SetGeometry(headless.Geometry->clone(), 0, error)) {
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    return false;
  }
  if (!renderer.PublishesWorldCandidate(error)) {
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    return false;
  }
  return true;
}

[[nodiscard]] std::expected<HeadlessDeclaration, std::string>
PrepareHeadlessDeclaration(const Scenario::Document &scenario,
                           const Generators::Registry &registry,
                           const GroundQuery &ground,
                           std::span<const float> groundPositionsM,
                           std::span<const uint32_t> groundIndex) {
  auto made = BuildGeneratedGeometry(scenario, registry, ground);
  if (!made) { return std::unexpected(std::move(made.error())); }
  HeadlessDeclaration prepared;
  prepared.Geometry = std::move(*made);
  if (!prepared.Geometry) { return prepared; }
  auto occlusion = Core::BuildAudioOcclusion(*prepared.Geometry, groundPositionsM, groundIndex);
  if (!occlusion) { return std::unexpected(std::move(occlusion.error())); }
  prepared.Occlusion.emplace(std::move(*occlusion));
  return prepared;
}

struct PreparedRuntimeDeclaration {
  HeadlessDeclaration Headless;
  std::unique_ptr<Core::Live> Targeted;
};

[[nodiscard]] std::expected<PreparedRuntimeDeclaration, std::string>
PrepareRuntimeDeclaration(const Scenario::Document &scenario,
                          const Generators::Registry &registry,
                          const GroundQuery &ground,
                          std::span<const float> groundPositionsM,
                          std::span<const uint32_t> groundIndex,
                          Seen &picture,
                          const Core::Declaration &declared) {
  auto headless =
      PrepareHeadlessDeclaration(scenario, registry, ground, groundPositionsM, groundIndex);
  if (!headless) { return std::unexpected(std::move(headless.error())); }
  PreparedRuntimeDeclaration prepared{.Headless = std::move(*headless), .Targeted = {}};
  std::vector<std::vector<Ui::Layout::Scrolled>> wasScrolled;
  if (picture.Standing) { wasScrolled = picture.Standing->Scrolled(); }
  if (!picture.Targeted) { return prepared; }
  std::string error;
  if (!PrepareTargetedDeclaration(picture.Device,
                                  declared,
                                  &picture.Face,
                                  prepared.Headless,
                                  wasScrolled,
                                  prepared.Targeted,
                                  error)) {
    return std::unexpected(std::move(error));
  }
  return prepared;
}

[[nodiscard]] bool SamePicture(const Core::Declaration &a, const Core::Declaration &b);
[[nodiscard]] bool SameStand(const Core::Declaration &a, const Core::Declaration &b);

void PublishConfiguration(Kept &session,
                          std::optional<ViewBook> &views,
                          InputMap &bindings) noexcept {
  static_assert(std::is_nothrow_move_assignable_v<std::optional<ViewBook>>);
  session.Views = std::move(views);
  static_assert(std::is_nothrow_move_assignable_v<InputMap>);
  session.Bound = std::move(bindings);
}

[[nodiscard]] std::optional<Result> ReuseDeclaration(const Scenario::Document &scenario,
                                                     Core::Declaration &declared,
                                                     Seen &picture,
                                                     Kept &session,
                                                     std::optional<ViewBook> &views,
                                                     InputMap &bindings,
                                                     std::string &error) {
  if (!picture.Standing || HasGeneratedContent(scenario) || HasGeneratedContent(session.Declared) ||
      !SamePicture(picture.Shown, declared) || !SameStand(picture.Shown, declared) ||
      session.Declared.Ground.VegetationEnabled != scenario.Ground.VegetationEnabled) {
    return std::nullopt;
  }
  if (!SameSurfaces(picture.Shown.Surfaces, declared.Surfaces) &&
      !picture.Standing->Redeclare(declared.Surfaces, error)) {
    return Result{std::unexpected(error)};
  }
  picture.Shown = std::move(declared);
  session.Declared = scenario;
  ++session.DeclarationRevision;
  session.AudioBodies.clear();
  session.Sounding.reset();
  session.Carried = Unacted(scenario);
  error.clear();
  PublishConfiguration(session, views, bindings);
  return Result{};
}

[[nodiscard]] bool SameRenderPlan(const Core::Declaration &a, const Core::Declaration &b) {
  return a.Stages == b.Stages && a.Outputs == b.Outputs && a.Transfer == b.Transfer &&
         a.Precision == b.Precision && a.Exposure == b.Exposure;
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

[[nodiscard]] bool SameStand(const Core::Declaration &a, const Core::Declaration &b) {
  return SamePicture(a, b) && a.Stands == b.Stands && a.Variant == b.Variant &&
         a.Animation == b.Animation && a.Clip == b.Clip;
}

}

void Engine::ships() {
  if (S_->World.Offering.count() > 0) { return; }
  const auto shipped = offers(S_->World.Shipping.Offered());
  if (!shipped) { std::terminate(); }
}

namespace {
[[nodiscard]] const Scenario::Asset *FirstGltfAsset(const Scenario::Document &scenario) {
  for (const Scenario::Asset &asset : scenario.Assets) {
    if (asset.Kind == "gltf") { return &asset; }
  }
  return nullptr;
}

[[nodiscard]] std::expected<std::optional<ViewBook>, std::string>
PrepareViews(const Scenario::Document &scenario) {
  if (scenario.Views.empty()) { return std::optional<ViewBook>{}; }
  const std::string_view starting = scenario.Played.View.empty()
                                        ? std::string_view(scenario.Views.front().Id)
                                        : std::string_view(scenario.Played.View);
  auto views = ViewBook::Stand(scenario.Views, starting);
  if (!views) { return std::unexpected(std::move(views.error())); }
  return std::optional<ViewBook>{std::move(*views)};
}
}

namespace Says {
constexpr auto InvalidSimulationTiming =
    "simulation requires a finite positive step and positive catch-up count with finite duration";
constexpr auto RevisionExhausted = "declaration revision exhausted";
}

namespace {
[[nodiscard]] Result ValidateDeclarationInputs(const Scenario::Document &scenario) {
  if (const auto valid = ValidateBodyDynamics(scenario.Bodies); !valid) {
    return std::unexpected(std::string(valid.error()));
  }
  if (const auto valid = ValidatePlayer(scenario.Played); !valid) {
    return std::unexpected(std::string(valid.error()));
  }
  if (const auto valid = ValidateWorld(scenario.Ground); !valid) {
    return std::unexpected(std::string(valid.error()));
  }
  if (const auto valid = ValidateWeather(scenario.Ground.Sky); !valid) {
    return std::unexpected(std::string(valid.error()));
  }
  for (const auto &asset : scenario.Assets) {
    const auto valid = ValidateAssetPlayback(asset);
    if (!valid) { return std::unexpected(std::string(valid.error())); }
  }
  for (const auto &feature : scenario.Ground.Osm) {
    const auto valid = ValidateOsmStructure(feature);
    if (!valid) { return std::unexpected(std::string(valid.error())); }
  }
  if (!std::isfinite(scenario.WheelStepPx) || scenario.WheelStepPx < 0.0) {
    return std::unexpected(Says::kInvalidWheelStep);
  }
  if (!std::isfinite(scenario.Motion.StepS) || scenario.Motion.StepS <= 0.0 ||
      scenario.Motion.MostStepsInArrears <= 0 ||
      !std::isfinite(scenario.Motion.StepS * scenario.Motion.MostStepsInArrears)) {
    return std::unexpected(Says::InvalidSimulationTiming);
  }
  return {};
}

[[nodiscard]] Result ValidateOfferedGenerators(const Scenario::Document &scenario,
                                               const Generators::Registry &registry,
                                               std::string &error) {
  const auto offers = [&registry](const std::string &kind) {
    return registry.named(kind) != nullptr;
  };
  for (const Scenario::Generating &named : scenario.Generators) {
    if (offers(named.Kind)) { continue; }
    error = "the scenario declares a generator of kind '" + named.Kind +
            "' and nothing offers that kind -- a declaration nobody can act on is a refusal, "
            "never a line that is counted and dropped";
    return std::unexpected(error);
  }
  for (const Scenario::Asset &shown : scenario.Assets) {
    if (shown.Kind != "generated" || offers(shown.Uri)) { continue; }
    error = "the scenario stands the generated asset '" + shown.Uri +
            "' and nothing offers a generator of that kind -- an asset names a generator the "
            "way a scenario names anything, and a name nobody answers is a refusal";
    return std::unexpected(error);
  }
  return {};
}

void PrepareImportedAssets(const Scenario::Document &scenario,
                           const std::string &assets,
                           Core::Declaration &declared) {
  const Scenario::Asset *const subject = FirstGltfAsset(scenario);

  if (subject != nullptr) {
    declared.Stands = Beneath(assets, subject->Uri);
    bool first = true;
    for (const Scenario::Asset &shown : scenario.Assets) {
      if (shown.Kind != "gltf") { continue; }
      if (first) {
        first = false;
        continue;
      }
      declared.Joins.push_back(Beneath(assets, shown.Uri));
    }
    declared.Variant = subject->Variant;
    declared.Overriding = subject->Surfaces;
    declared.Animation = subject->Animation;
    declared.Clip = subject->Clip;
  }
}

void PrepareRenderSettings(const Scenario::Document &scenario, Core::Declaration &declared) {
  declared.DrawsSky = scenario.Ground.Declared && scenario.Ground.AirDensityKgM3 > 0.0;
  const Scenario::Patch whole;
  const Scenario::Patch &picture = scenario.Render.Declared ? scenario.Render.Picture : whole;
  if (scenario.Render.Declared) {
    if (scenario.Render.Fps > 0.0) { declared.Fps = scenario.Render.Fps; }
    declared.Fill = scenario.Render.Fill;
    declared.OrbitDegPerFrame = scenario.Render.OrbitDegPerFrame;
    declared.Stages = scenario.Render.Stages;
    declared.Outputs = scenario.Render.Outputs;
    declared.Transfer = scenario.Render.Transfer;
    declared.Precision = scenario.Render.Precision;
    declared.Exposure = scenario.Render.Exposure > 0.0 ? scenario.Render.Exposure : 0.0;
  }
  declared.PictureLeftFrac = picture.LeftFrac;
  declared.PictureTopFrac = picture.TopFrac;
  declared.PictureWidthFrac = picture.WidthFrac;
  declared.PictureHeightFrac = picture.HeightFrac;
}

[[nodiscard]] Result PrepareLighting(const Scenario::Document &scenario,
                                     Core::Declaration &declared,
                                     std::string &error) {
  if (scenario.Lit.Declared) {
    declared.KeyLux = scenario.Lit.Key.Lux;
    declared.KeyElevationDeg = scenario.Lit.Key.ElevationDeg;
    declared.KeyBearingDeg = scenario.Lit.Key.BearingDeg;
    for (int at = 0; at < 3; ++at) { declared.IndirectLight[at] = scenario.Lit.IndirectLight[at]; }
    declared.ShadowRadiusM = scenario.Lit.ShadowRadiusM;
  }
  {
    const bool anglePut = scenario.Lit.Declared && (scenario.Lit.Key.ElevationDeg != 0.0 ||
                                                    scenario.Lit.Key.BearingDeg != 0.0);
    if (scenario.Ground.Declared && anglePut && scenario.Time.Declared) {
      error = "this scenario declares a clock AND hand-sets the key light to " +
              Said(scenario.Lit.Key.ElevationDeg) + " degrees up on bearing " +
              Said(scenario.Lit.Key.BearingDeg) +
              " -- over a place on Earth only one of the two can be true, and a sun that does "
              "not follow the hour disagrees with its own shadows the moment the clock moves";
      return std::unexpected(error);
    }
    if (scenario.Ground.Declared && !anglePut) {
      int64_t whenS = 0;
      const bool live = !scenario.Time.Declared || scenario.Time.Live;
      if (scenario.Time.Start.empty() || !ParseIsoUtc(scenario.Time.Start.c_str(), whenS)) {
        if (!live && scenario.Time.Declared) {
          error = "this scenario declares a clock that is neither LIVE nor a stated instant -- "
                  "'" +
                  scenario.Time.Start +
                  "' is not an ISO 8601 UTC time, and a sky has to "
                  "stand at some hour";
          return std::unexpected(error);
        }
        whenS = static_cast<int64_t>(std::time(nullptr));
      }
      const Solar sun = SolarAt({.LongitudeDeg = scenario.Ground.Origin.LongitudeDeg,
                                 .LatitudeDeg = scenario.Ground.Origin.LatitudeDeg},
                                static_cast<double>(whenS));
      declared.KeyElevationDeg = static_cast<double>(sun.SunElDeg);
      declared.KeyBearingDeg = static_cast<double>(sun.SunAzDeg);
      declared.KeyFromClock = true;
    }
  }

  return {};
}
}

Result Engine::declare(const Scenario::Document &scenario) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  if (const auto valid = ValidateDeclarationInputs(scenario); !valid) { return valid; }
  if (S_->Session.DeclarationRevision == std::numeric_limits<uint64_t>::max()) {
    S_->Error = Says::RevisionExhausted;
    return std::unexpected(S_->Error);
  }
  auto views = PrepareViews(scenario);
  if (!views) {
    S_->Error = std::move(views.error());
    return std::unexpected(S_->Error);
  }
  ships();
  if (const auto valid = ValidateOfferedGenerators(scenario, S_->World.Offering, S_->Error);
      !valid) {
    return valid;
  }
  Core::Declaration declared;
  declared.Haze = scenario.Ground.Sky.Haze;
  declared.SurfaceWidthPx = S_->Picture.Frame.WidthPx;
  declared.SurfaceHeightPx = S_->Picture.Frame.HeightPx;
  PrepareImportedAssets(scenario, S_->Session.Under.Assets, declared);
  PrepareRenderSettings(scenario, declared);
  if (const auto lit = PrepareLighting(scenario, declared, S_->Error); !lit) { return lit; }

  declared.Surfaces = PrepareSurfaces(scenario.Surfaces);
  if (!declared.Surfaces.empty() &&
      !S_->Picture.Face.Opens(S_->Session.Under.Shipped + "/fonts", S_->Error)) {
    return std::unexpected(S_->Error);
  }

  InputMap bindings;
  if (!bindings.Build(scenario.Input, S_->Error)) { return std::unexpected(S_->Error); }
  if (!Core::InputPump::CatalogueReady()) {
    S_->Error = "the declared bindings did not open a pump, so no event could reach an action";
    return std::unexpected(S_->Error);
  }
  if (auto reused = ReuseDeclaration(
          scenario, declared, S_->Picture, S_->Session, *views, bindings, S_->Error)) {
    return std::move(*reused);
  }

  auto prepared = PrepareRuntimeDeclaration(scenario,
                                            S_->World.Offering,
                                            S_->World.Stack.Ground(),
                                            S_->World.GroundPositionsM,
                                            S_->World.GroundIndex,
                                            S_->Picture,
                                            declared);
  if (!prepared) {
    S_->Error = std::move(prepared.error());
    return std::unexpected(S_->Error);
  }
  S_->World.Bakes.Clear();
  S_->World.Stack.Footprints().ResetDerived();
  S_->World.Pieces.Clear();
  S_->World.Sheets.Clear();
  S_->World.PiecesFramed = false;
  S_->World.GroundPublished.Reset();
  S_->World.Crowns.reset();
  S_->World.Instances.clear();
  S_->World.Placed = S_->World.Instanced = 0;
  S_->World.Grown = false;
  S_->Picture.Shown = std::move(declared);
  if (!S_->Picture.Targeted) {
    S_->Picture.PendingGeometry = std::move(prepared->Headless.Geometry);
    S_->Picture.PendingAudioOcclusion = std::move(prepared->Headless.Occlusion);
    S_->Session.Declared = scenario;
    ++S_->Session.DeclarationRevision;
    S_->Session.AudioBodies.clear();
    S_->Session.Sounding.reset();
    S_->Session.Taken = true;
    S_->Session.Carried = Unacted(scenario);
    S_->Error.clear();
    PublishConfiguration(S_->Session, *views, bindings);
    return {};
  }
  Core::Live::HandOffRenderer(S_->Picture.Standing);
  S_->Picture.Standing = std::move(prepared->Targeted);
  S_->Session.Declared = scenario;
  ++S_->Session.DeclarationRevision;
  S_->Session.AudioBodies.clear();
  S_->Session.Sounding.reset();
  S_->Session.Taken = true;
  S_->Session.Carried = Unacted(scenario);
  S_->Error.clear();
  S_->World.AudioOcclusion =
      prepared->Headless.Occlusion ? std::move(*prepared->Headless.Occlusion) : TriangleBvh{};
  PublishConfiguration(S_->Session, *views, bindings);
  return {};
}

bool Engine::generated(const Scenario::Document &scenario) {
  auto made = BuildGeneratedGeometry(scenario, S_->World.Offering, S_->World.Stack.Ground());
  if (!made) {
    S_->Error = std::move(made.error());
    return false;
  }
  return !*made || setGeometry(**made);
}

bool Engine::readScenarioInto(std::string_view path, Scenario::Document &out) {
  const std::string held(path);
  const std::expected<std::string, std::string> slurped = ReadTextFile(held, kMostScenarioBytes);
  if (!slurped) {
    S_->Error = slurped.error();
    return false;
  }
  const std::string &text = *slurped;
  size_t remainingBytes = kMostScenarioBytes - text.size();

  if (!ReadScenario(text.c_str(), text.size(), out, S_->Error)) {
    S_->Error = held + ": " + S_->Error;
    return false;
  }

  S_->Session.LayerTrace.clear();
  if (out.Layers.empty()) { return true; }
  const size_t cut = held.find_last_of('/');
  const std::string dir = cut == std::string::npos ? std::string() : held.substr(0, cut + 1);
  for (const Scenario::Layer &layer : out.Layers) {
    const std::string named = layer.Id.empty() ? layer.Path : layer.Id;
    if (!LayerActive(layer, out.Named.Active)) {
      S_->Session.LayerTrace.push_back("layer '" + named + "' is inactive -- its set '" +
                                       layer.Set + "' is not selected by active=\"" +
                                       out.Named.Active + "\"");
      continue;
    }
    const std::string at =
        (!layer.Path.empty() && layer.Path.front() == '/') ? layer.Path : dir + layer.Path;
    const std::expected<std::string, std::string> read = ReadTextFile(at, remainingBytes);
    if (!read) {
      S_->Error = read.error();
      return false;
    }
    const std::string &fragmentText = *read;
    remainingBytes -= fragmentText.size();
    if (!ApplyLayer(out,
                    fragmentText.c_str(),
                    fragmentText.size(),
                    named,
                    S_->Session.LayerTrace,
                    S_->Error)) {
      S_->Error = at + ": " + S_->Error;
      return false;
    }
  }
  out.Layers.clear();
  return true;
}

Result Engine::setGeometry(const Geometry &geometry) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  if (!geometry.wellFormed()) {
    S_->Error = "the geometry stands no whole part, and a subject of nothing is a refusal rather "
                "than an empty picture";
    return std::unexpected(S_->Error);
  }
  auto occlusion =
      Core::BuildAudioOcclusion(geometry, S_->World.GroundPositionsM, S_->World.GroundIndex);
  if (!occlusion) {
    S_->Error = occlusion.error();
    return std::unexpected(S_->Error);
  }
  if (!S_->Picture.Standing) {
    S_->Picture.PendingGeometry = geometry.clone();
    S_->Picture.PendingAudioOcclusion.emplace(std::move(*occlusion));
    S_->Error.clear();
    return {};
  }
  if (!Core::Live::ReplacesGeometry(S_->Picture.Device,
                                    *S_->Picture.Standing,
                                    geometry.clone(),
                                    &S_->Picture.Face,
                                    S_->Picture.Standing,
                                    S_->Error)) {
    return std::unexpected(S_->Error);
  }
  S_->World.BindLiveResources(*S_->Picture.Standing);
  S_->World.AudioOcclusion = std::move(*occlusion);
  return {};
}

std::expected<std::string, std::string> Engine::writeScenario() const {
  return WriteScenario(S_->Session.Declared);
}

Result Engine::readScenario(std::string_view path) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  Scenario::Document scenario;
  if (!readScenarioInto(path, scenario)) { return std::unexpected(S_->Error); }
  const std::vector<std::string> traced = S_->Session.LayerTrace;
  const Result stood = declare(scenario);
  if (!stood) { return stood; }
  S_->Session.Carried.insert(S_->Session.Carried.end(), traced.begin(), traced.end());
  return {};
}

EntityRegistry &Engine::entities() {
  return S_->Simulation->Entities;
}

const EntityRegistry &Engine::entities() const {
  return S_->Simulation->Entities;
}

const Scenario::Document &Engine::declaration() const {
  return S_->Session.Declared;
}

}
