#include "ScenarioWrite.h"
#include "EngineHeld.h"
#include "Ephemeris.h"
#include "CivilTime.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <expected>
#include <numeric>
#include <optional>
#include <string>
#include <span>
#include <vector>
#include <utility>
#include <string_view>

namespace outshine {

Result Engine::handleEvent(const SDL_Event &event) {
  if (!S_->Picture.Standing) { return std::unexpected(S_->Error); }
  if (event.type == SDL_EVENT_MOUSE_WHEEL) {
    float xPx = 0.0f;
    float yPx = 0.0f;
    SDL_GetMouseState(&xPx, &yPx);
    return S_->Picture.Standing->Wheeled(static_cast<double>(xPx),
                                         static_cast<double>(yPx),
                                         -static_cast<double>(event.wheel.y) *
                                             S_->Session.Declared.WheelStepPx,
                                         S_->Error)
               ? Result{}
               : std::unexpected(S_->Error);
  }
  if (S_->Session.Pumping && (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)) {
    std::array<Core::InputPump::Fired, 2> fired{};
    const size_t many = S_->Session.Pump.Translate(event, fired);
    if (S_->Offered == nullptr) { return std::unexpected(S_->Error); }
    bool acted = false;
    for (size_t at = 0; at < many; ++at) {
      const std::string *const named = S_->Session.Bound.ActionNamed(fired[at].Action);
      if (named == nullptr) { continue; }
      const Argument value{
          .Is = Argument::Kind::Number, .Number = static_cast<double>(fired[at].Value), .Text = {}};
      acted = S_->Offered->calls(*named, std::span<const Argument>(&value, 1)) || acted;
    }
    return acted ? Result{} : std::unexpected(S_->Error);
  }
  if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN) { return std::unexpected(S_->Error); }

  size_t surface = 0;
  const Ui::Touched found = S_->Picture.Standing->Under(
      static_cast<double>(event.button.x), static_cast<double>(event.button.y), surface);
  if (!found.Held() || found.Action.empty()) { return std::unexpected(S_->Error); }
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
  Forwarding answering(S_->Offered);
  if (!programme.Run(answering, S_->Error)) { return std::unexpected(S_->Error); }
  return answering.Fired() ? Result{} : std::unexpected(S_->Error);
}

Result Engine::setSurfaces(const std::vector<Scenario::Surface> &surfaces) {
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands, so there is no picture for a surface to be laid over -- a "
                "scenario is declared before its surfaces are exchanged";
    return std::unexpected(S_->Error);
  }
  if (!surfaces.empty() &&
      !S_->Picture.Face.Opens(S_->Session.Under.Shipped + "/fonts", S_->Error)) {
    return std::unexpected(S_->Error);
  }
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
  S_->Session.Declared.Surfaces = surfaces;
  return S_->Picture.Standing->Redeclare(std::move(laid), S_->Error) ? Result{}
                                                                     : std::unexpected(S_->Error);
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

[[nodiscard]] bool SamePicture(const Core::Declaration &a, const Core::Declaration &b) {
  return a.SurfaceWidthPx == b.SurfaceWidthPx && a.SurfaceHeightPx == b.SurfaceHeightPx &&
         a.Built == b.Built && a.MetresPerUnit == b.MetresPerUnit && a.Fps == b.Fps &&
         a.Fill == b.Fill && a.OrbitDegPerFrame == b.OrbitDegPerFrame &&
         a.PictureLeftFrac == b.PictureLeftFrac && a.PictureTopFrac == b.PictureTopFrac &&
         a.PictureWidthFrac == b.PictureWidthFrac && a.PictureHeightFrac == b.PictureHeightFrac &&
         a.IndirectLight[0] == b.IndirectLight[0] && a.IndirectLight[1] == b.IndirectLight[1] &&
         a.IndirectLight[2] == b.IndirectLight[2] && a.KeyLux == b.KeyLux &&
         a.KeyFromClock == b.KeyFromClock && a.Exposure == b.Exposure && a.DrawsSky == b.DrawsSky &&
         a.Stages == b.Stages && a.ShadowRadiusM == b.ShadowRadiusM &&
         a.KeyElevationDeg == b.KeyElevationDeg && a.KeyBearingDeg == b.KeyBearingDeg;
}

[[nodiscard]] bool SameStand(const Core::Declaration &a, const Core::Declaration &b) {
  return SamePicture(a, b) && a.Stands == b.Stands && a.Variant == b.Variant &&
         a.Animation == b.Animation && a.Clip == b.Clip;
}

} // namespace

void Engine::ships() {
  if (S_->World.Offering.count() > 0) { return; }
  (void)S_->World.Offering.offers(S_->World.Shipping.Offered());
}

Result Engine::declare(const Scenario::Document &scenario) {
  ships();
  const auto offers = [this](const std::string &kind) {
    return S_->World.Offering.named(kind) != nullptr;
  };
  for (const Scenario::Generating &named : scenario.Generators) {
    if (offers(named.Kind)) { continue; }
    S_->Error = "the scenario declares a generator of kind '" + named.Kind +
                "' and nothing offers that kind -- a declaration nobody can act on is a refusal, "
                "never a line that is counted and dropped";
    return std::unexpected(S_->Error);
  }
  for (const Scenario::Asset &shown : scenario.Assets) {
    if (shown.Kind != "generated" || offers(shown.Uri)) { continue; }
    S_->Error = "the scenario stands the generated asset '" + shown.Uri +
                "' and nothing offers a generator of that kind -- an asset names a generator the "
                "way a scenario names anything, and a name nobody answers is a refusal";
    return std::unexpected(S_->Error);
  }
  const Scenario::Asset *const subject = scenario.subject();

  Core::Declaration declared;
  declared.Haze = scenario.Ground.Sky.Haze;
  declared.SurfaceWidthPx = S_->Picture.Frame.WidthPx;
  declared.SurfaceHeightPx = S_->Picture.Frame.HeightPx;
  if (subject != nullptr) {
    declared.Stands = Beneath(S_->Session.Under.Assets, subject->Uri);
    bool first = true;
    for (const Scenario::Asset &shown : scenario.Assets) {
      if (shown.Kind != "gltf") { continue; }
      if (first) {
        first = false;
        continue;
      }
      declared.Joins.push_back(Beneath(S_->Session.Under.Assets, shown.Uri));
    }
    declared.Variant = subject->Variant;
    declared.Overriding = subject->Surfaces;
    declared.Animation = subject->Animation;
    declared.Clip = subject->Clip;
  }
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
      S_->Error = "this scenario declares a clock AND hand-sets the key light to " +
                  Said(scenario.Lit.Key.ElevationDeg) + " degrees up on bearing " +
                  Said(scenario.Lit.Key.BearingDeg) +
                  " -- over a place on Earth only one of the two can be true, and a sun that does "
                  "not follow the hour disagrees with its own shadows the moment the clock moves";
      return std::unexpected(S_->Error);
    }
    if (scenario.Ground.Declared && !anglePut) {
      int64_t whenS = 0;
      const bool live = !scenario.Time.Declared || scenario.Time.Live;
      if (scenario.Time.Start.empty() || !ParseIsoUtc(scenario.Time.Start.c_str(), whenS)) {
        if (!live && scenario.Time.Declared) {
          S_->Error = "this scenario declares a clock that is neither LIVE nor a stated instant -- "
                      "'" +
                      scenario.Time.Start +
                      "' is not an ISO 8601 UTC time, and a sky has to "
                      "stand at some hour";
          return std::unexpected(S_->Error);
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

  std::vector<size_t> ordered(scenario.Surfaces.size());
  std::ranges::iota(ordered, size_t{0});
  std::ranges::stable_sort(ordered, [&scenario](size_t a, size_t b) {
    return scenario.Surfaces[a].Z < scenario.Surfaces[b].Z;
  });
  for (const size_t at : ordered) {
    const Scenario::Surface *const surface = &scenario.Surfaces[at];
    Core::Shows shows;
    shows.Markup = surface->Document;
    shows.Style = surface->Style;
    shows.Programme = surface->Programme;
    shows.LeftFrac = surface->Where.LeftFrac;
    shows.TopFrac = surface->Where.TopFrac;
    shows.WidthFrac = surface->Where.WidthFrac;
    shows.HeightFrac = surface->Where.HeightFrac;
    declared.Surfaces.push_back(std::move(shows));
  }
  if (!declared.Surfaces.empty() &&
      !S_->Picture.Face.Opens(S_->Session.Under.Shipped + "/fonts", S_->Error)) {
    return std::unexpected(S_->Error);
  }

  S_->Session.Pumping = false;
  if (!scenario.Input.empty()) {
    if (!S_->Session.Bound.Build(scenario.Input, S_->Error)) { return std::unexpected(S_->Error); }
    if (!S_->Session.Pump.Open(S_->Session.Bound)) {
      S_->Error = "the declared bindings did not open a pump, so no event could reach an action";
      return std::unexpected(S_->Error);
    }
    S_->Session.Pumping = true;
  }

  S_->Session.Volumes.reset();
  if (!scenario.Volumes.empty()) {
    auto stood = TriggerField::Stand(scenario.Volumes, scenario.Events);
    if (!stood) {
      S_->Error = stood.error();
      return std::unexpected(S_->Error);
    }
    S_->Session.Volumes.emplace(std::move(*stood));
  }

  S_->Session.Views.reset();
  if (!scenario.Views.empty()) {
    const std::string_view starting = scenario.Played.View.empty()
                                          ? std::string_view(scenario.Views.front().Id)
                                          : std::string_view(scenario.Played.View);
    auto stood = ViewBook::Stand(scenario.Views, starting);
    if (!stood) {
      S_->Error = stood.error();
      return std::unexpected(S_->Error);
    }
    S_->Session.Views.emplace(std::move(*stood));
  }

  if (S_->Picture.Standing && SamePicture(S_->Picture.Shown, declared)) {
    if (!SameStand(S_->Picture.Shown, declared) &&
        !S_->Picture.Standing->Restands(
            declared.Stands, declared.Variant, declared.Animation, declared.Clip, S_->Error)) {
      return std::unexpected(S_->Error);
    }
    if (!SameSurfaces(S_->Picture.Shown.Surfaces, declared.Surfaces) &&
        !S_->Picture.Standing->Redeclare(declared.Surfaces, S_->Error)) {
      return std::unexpected(S_->Error);
    }
    S_->Picture.Shown = std::move(declared);
    S_->Session.Declared = scenario;
    S_->Session.Carried = Unacted(scenario);
    S_->Error.clear();
    return {};
  }

  std::vector<std::vector<Ui::Layout::Scrolled>> wasScrolled;
  if (S_->Picture.Standing) { wasScrolled = S_->Picture.Standing->Scrolled(); }
  S_->World.Bakes.Clear();
  S_->World.Pieces.Clear();
  S_->World.Sheets.Clear();
  S_->World.PiecesFramed = false;
  S_->Picture.Standing.reset();
  S_->Picture.Shown = declared;
  if (!S_->Picture.Targeted) {
    S_->Session.Declared = scenario;
    S_->Session.Taken = true;
    S_->Session.Carried = Unacted(scenario);
    S_->Error.clear();
    return generated(scenario) ? Result{} : std::unexpected(S_->Error);
  }
  if (!Core::Live::Open(S_->Picture.Device,
                        std::move(declared),
                        &S_->Picture.Face,
                        S_->Picture.Standing,
                        S_->Error)) {
    S_->World.Bakes.Clear();
    S_->World.Pieces.Clear();
    S_->World.Sheets.Clear();
    S_->Picture.Standing.reset();
    return std::unexpected(S_->Error);
  }
  if (!wasScrolled.empty() && !S_->Picture.Standing->Scrolled(std::move(wasScrolled), S_->Error)) {
    return std::unexpected(S_->Error);
  }
  S_->Session.Declared = scenario;
  S_->Session.Taken = true;
  S_->Session.Carried = Unacted(scenario);
  S_->Error.clear();
  return generated(scenario) ? Result{} : std::unexpected(S_->Error);
}

bool Engine::generated(const Scenario::Document &scenario) {
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

  const Stands stands(S_->World.Stack.Ground());

  Generators::Request asked;
  asked.LatitudeDeg = scenario.Ground.Origin.LatitudeDeg;
  asked.LongitudeDeg = scenario.Ground.Origin.LongitudeDeg;
  asked.ExtentM = scenario.Ground.Origin.RadiusM;
  asked.Ground = &stands;

  Geometry made;
  const auto offered = [&](const std::string &kind) {
    const Generators::Generator *const stood = S_->World.Offering.named(kind);
    if (stood == nullptr) { return true; }
    if (stood->make(asked, made)) { return true; }
    S_->Error = "the generator of kind '" + kind + "' refused to make anything";
    return false;
  };

  for (const Scenario::Asset &shown : scenario.Assets) {
    if (shown.Kind != "generated") { continue; }
    if (!offered(shown.Uri)) { return false; }
  }
  for (const Scenario::Generating &named : scenario.Generators) {
    if (!offered(named.Kind)) { return false; }
  }
  return made.parts() == 0 || setGeometry(made);
}

bool Engine::readScenarioInto(std::string_view path, Scenario::Document &out) {
  const std::string held(path);
  const std::expected<std::string, std::string> slurped = SlurpFile(held);
  if (!slurped) {
    S_->Error = slurped.error();
    return false;
  }
  const std::string &text = *slurped;

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
    const std::expected<std::string, std::string> read = SlurpFile(at);
    if (!read) {
      S_->Error = read.error();
      return false;
    }
    const std::string &fragmentText = *read;
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
  return true;
}

Result Engine::setGeometry(const Geometry &geometry) {
  if (!geometry.wellFormed()) {
    S_->Error = "the geometry stands no whole part, and a subject of nothing is a refusal rather "
                "than an empty picture";
    return std::unexpected(S_->Error);
  }
  Gltf::Subject handed;
  if (!handed.Assemble(geometry)) {
    S_->Error = handed.Error();
    return std::unexpected(S_->Error);
  }
  S_->Blocks(handed);
  if (!S_->Picture.Standing) {
    S_->Picture.Handed = std::move(handed);
    S_->Picture.Carrying = true;
    S_->Error.clear();
    return {};
  }
  return S_->Picture.Standing->Restand(handed, 0, S_->Error) ? Result{}
                                                             : std::unexpected(S_->Error);
}

std::string Engine::writeScenario() const {
  return WriteScenario(S_->Session.Declared);
}

Result Engine::readScenario(std::string_view path) {
  Scenario::Document scenario;
  if (!readScenarioInto(path, scenario)) { return std::unexpected(S_->Error); }
  const std::vector<std::string> traced = S_->Session.LayerTrace;
  const Result stood = declare(scenario);
  if (!stood) { return stood; }
  S_->Session.Carried.insert(S_->Session.Carried.end(), traced.begin(), traced.end());
  return {};
}

Scene &Engine::scene() {
  return S_->Cast.Scene;
}

const Scene &Engine::scene() const {
  return S_->Cast.Scene;
}

const Scenario::Document &Engine::declaration() const {
  return S_->Session.Declared;
}

} // namespace outshine
