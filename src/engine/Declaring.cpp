#include "EngineHeld.h"
#include "Ephemeris.h"
#include "CivilTime.h"

#include <ctime>

namespace outshine {


bool Engine::handleEvent(const SDL_Event &event) {
  if (!S_->Picture.Standing) { return false; }
  if (event.type == SDL_EVENT_MOUSE_WHEEL) {
    float xPx = 0.0f, yPx = 0.0f;
    SDL_GetMouseState(&xPx, &yPx);
    return S_->Picture.Standing->Wheeled((double)xPx, (double)yPx,
                                 -(double)event.wheel.y * S_->Session.Declared.WheelStepPx, S_->Error);
  }
  if (S_->Session.Pumping && (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)) {
    Core::InputPump::Fired fired[2];
    const size_t many = S_->Session.Pump.Translate(event, fired);
    if (S_->Offered == nullptr) { return false; }
    bool acted = false;
    for (size_t at = 0; at < many; ++at) {
      const std::string *const named = S_->Session.Bound.ActionNamed(fired[at].Action);
      if (named == nullptr) { continue; }
      const Argument value{Argument::Kind::Number, (double)fired[at].Value, {}};
      acted = S_->Offered->Calls(*named, std::span<const Argument>(&value, 1)) || acted;
    }
    return acted;
  }
  if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN) { return false; }

  size_t surface = 0;
  const Ui::Touched found =
      S_->Picture.Standing->Under((double)event.button.x, (double)event.button.y, surface);
  if (!found.Held() || found.Action.empty()) { return false; }
  const std::string &action = found.Action;
  if (S_->Offered == nullptr) {
    S_->Error = "a surface declares the call '" + action +
                "' and no host was offered to answer it -- the client calls Offers before it "
                "hands an event in";
    return false;
  }

  Script::Program programme;
  const std::string text = S_->Picture.Standing->ProgrammeOf(surface) + "\n" + action + ";\n";
  if (!programme.Read(text, S_->Error)) { return false; }
  Forwarding answering(S_->Offered);
  if (!programme.Run(answering, S_->Error)) { return false; }
  return answering.Fired();
}

bool Engine::setSurfaces(const std::vector<Surface> &surfaces) {
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands, so there is no picture for a surface to be laid over -- a "
                "scenario is declared before its surfaces are exchanged";
    return false;
  }
  if (!surfaces.empty() && !S_->Picture.Face.Opens(S_->Session.Under.Shipped + "/fonts", S_->Error)) {
    return false;
  }
  std::vector<const Surface *> ordered;
  ordered.reserve(surfaces.size());
  for (const Surface &surface : surfaces) { ordered.push_back(&surface); }
  std::stable_sort(ordered.begin(), ordered.end(),
                   [](const Surface *a, const Surface *b) { return a->Z < b->Z; });
  std::vector<Core::Shows> laid;
  laid.reserve(ordered.size());
  for (const Surface *surface : ordered) {
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
  return S_->Picture.Standing->Redeclare(std::move(laid), S_->Error);
}

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
  return a.SurfaceWidthPx == b.SurfaceWidthPx &&
         a.SurfaceHeightPx == b.SurfaceHeightPx && a.Built == b.Built &&
         a.MetresPerUnit == b.MetresPerUnit && a.Fps == b.Fps &&
         a.Fill == b.Fill && a.OrbitDegPerFrame == b.OrbitDegPerFrame &&
         a.PictureLeftFrac == b.PictureLeftFrac && a.PictureTopFrac == b.PictureTopFrac &&
         a.PictureWidthFrac == b.PictureWidthFrac && a.PictureHeightFrac == b.PictureHeightFrac &&
         a.IndirectLight[0] == b.IndirectLight[0] && a.IndirectLight[1] == b.IndirectLight[1] &&
         a.IndirectLight[2] == b.IndirectLight[2] && a.KeyLux == b.KeyLux &&
         a.Exposure == b.Exposure && a.DrawsSky == b.DrawsSky && a.Stages == b.Stages &&
         a.ShadowRadiusM == b.ShadowRadiusM && a.KeyElevationDeg == b.KeyElevationDeg &&
         a.KeyBearingDeg == b.KeyBearingDeg;
}

[[nodiscard]] bool SameStand(const Core::Declaration &a, const Core::Declaration &b) {
  return SamePicture(a, b) && a.Stands == b.Stands && a.Variant == b.Variant &&
         a.Animation == b.Animation && a.Clip == b.Clip;
}

void Engine::ships(void) {
  if (S_->World.Offering.Count() > 0) { return; }
  (void)S_->World.Offering.Offers(S_->World.Shipped);
}

bool Engine::declare(const Scenario &scenario) {
  ships();
  const auto offers = [this](const std::string &kind) {
    return S_->World.Offering.Named(kind) != nullptr;
  };
  for (const Generator &named : scenario.Generators) {
    if (offers(named.Kind)) { continue; }
    S_->Error = "the scenario declares a generator of kind '" + named.Kind +
                "' and nothing offers that kind -- a declaration nobody can act on is a refusal, "
                "never a line that is counted and dropped";
    return false;
  }
  for (const Asset &shown : scenario.Assets) {
    if (shown.Kind != "generated" || offers(shown.Uri)) { continue; }
    S_->Error = "the scenario stands the generated asset '" + shown.Uri +
                "' and nothing offers a generator of that kind -- an asset names a generator the "
                "way a scenario names anything, and a name nobody answers is a refusal";
    return false;
  }
  const Asset *const subject = scenario.Subject();

  Core::Declaration declared;
  declared.SurfaceWidthPx = S_->Picture.Frame.WidthPx;
  declared.SurfaceHeightPx = S_->Picture.Frame.HeightPx;
  if (subject != nullptr) {
    declared.Stands = Beneath(S_->Session.Under.Assets, subject->Uri);
    bool first = true;
    for (const Asset &shown : scenario.Assets) {
      if (shown.Kind != "gltf") { continue; }
      if (first) {
        first = false;
        continue;
      }
      declared.Joins.push_back(Beneath(S_->Session.Under.Assets, shown.Uri));
    }
    declared.Variant = subject->Variant;
    declared.Animation = subject->Animation;
    declared.Clip = subject->Clip;
  }
  declared.DrawsSky = scenario.Ground.Declared && scenario.Ground.AirDensityKgM3 > 0.0;
  const Patch whole;
  const Patch &picture = scenario.Render.Declared ? scenario.Render.Picture : whole;
  if (scenario.Render.Declared) {
    declared.Fps = scenario.Render.Fps;
    declared.Fill = scenario.Render.Fill;
    declared.OrbitDegPerFrame = scenario.Render.OrbitDegPerFrame;
    declared.Stages = scenario.Render.Stages;
  }
  declared.PictureLeftFrac = picture.LeftFrac;
  declared.PictureTopFrac = picture.TopFrac;
  declared.PictureWidthFrac = picture.WidthFrac;
  declared.PictureHeightFrac = picture.HeightFrac;
  if (scenario.Lit.Declared) {
    declared.KeyLux = scenario.Lit.Key.Lux;
    declared.KeyElevationDeg = scenario.Lit.Key.ElevationDeg;
    declared.KeyBearingDeg = scenario.Lit.Key.BearingDeg;
    const bool anglePut = scenario.Lit.Key.ElevationDeg != 0.0 || scenario.Lit.Key.BearingDeg != 0.0;
    if (scenario.Ground.Declared && anglePut && scenario.Time.Declared) {
      S_->Error = "this scenario declares a clock AND hand-sets the key light to " +
                  Said(scenario.Lit.Key.ElevationDeg) + " degrees up on bearing " +
                  Said(scenario.Lit.Key.BearingDeg) +
                  " -- over a place on Earth only one of the two can be true, and a sun that does "
                  "not follow the hour disagrees with its own shadows the moment the clock moves";
      return false;
    }
    if (scenario.Ground.Declared && !anglePut) {
      int64_t whenS = 0;
      if (!scenario.Time.Declared || scenario.Time.Start.empty() ||
          !ParseIsoUtc(scenario.Time.Start.c_str(), whenS)) {
        whenS = (int64_t)std::time(nullptr);
      }
      const Solar sun = SolarAt(scenario.Ground.Origin.LatitudeDeg, scenario.Ground.Origin.LongitudeDeg, (double)whenS);
      declared.KeyElevationDeg = (double)sun.SunElDeg;
      declared.KeyBearingDeg = (double)sun.SunAzDeg;
    }
    for (int at = 0; at < 3; ++at) { declared.IndirectLight[at] = scenario.Lit.IndirectLight[at]; }
    declared.ShadowRadiusM = scenario.Lit.ShadowRadiusM;
  }

  std::vector<const Surface *> ordered;
  ordered.reserve(scenario.Surfaces.size());
  for (const Surface &surface : scenario.Surfaces) { ordered.push_back(&surface); }
  std::stable_sort(ordered.begin(), ordered.end(),
                   [](const Surface *a, const Surface *b) { return a->Z < b->Z; });
  for (const Surface *surface : ordered) {
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
  if (!declared.Surfaces.empty() && !S_->Picture.Face.Opens(S_->Session.Under.Shipped + "/fonts", S_->Error)) {
    return false;
  }

  S_->Session.Pumping = false;
  if (!scenario.Input.empty()) {
    if (!S_->Session.Bound.Build(scenario.Input, S_->Error)) { return false; }
    if (!S_->Session.Pump.Open(S_->Session.Bound)) {
      S_->Error = "the declared bindings did not open a pump, so no event could reach an action";
      return false;
    }
    S_->Session.Pumping = true;
  }

  S_->Session.Volumes.reset();
  if (!scenario.Volumes.empty()) {
    auto stood = TriggerField::Stand(scenario.Volumes, scenario.Events);
    if (!stood) {
      S_->Error = stood.error();
      return false;
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
      return false;
    }
    S_->Session.Views.emplace(std::move(*stood));
  }

  if (S_->Picture.Standing && SamePicture(S_->Picture.Shown, declared)) {
    if (!SameStand(S_->Picture.Shown, declared) &&
        !S_->Picture.Standing->Restands(declared.Stands, declared.Variant, declared.Animation,
                                declared.Clip, S_->Error)) {
      return false;
    }
    if (!SameSurfaces(S_->Picture.Shown.Surfaces, declared.Surfaces) &&
        !S_->Picture.Standing->Redeclare(declared.Surfaces, S_->Error)) {
      return false;
    }
    S_->Picture.Shown = std::move(declared);
    S_->Session.Declared = scenario;
    S_->Session.Carried = Unacted(scenario);
    S_->Error.clear();
    return true;
  }

  std::vector<std::vector<Ui::Layout::Scrolled>> wasScrolled;
  if (S_->Picture.Standing) { wasScrolled = S_->Picture.Standing->Scrolled(); }
  S_->Picture.Standing.reset();
  S_->Picture.Shown = declared;
  if (!S_->Picture.Targeted) {
    S_->Session.Declared = scenario;
    S_->Session.Taken = true;
    S_->Session.Carried = Unacted(scenario);
    S_->Error.clear();
    return generated(scenario);
  }
  if (!Core::Live::Open(S_->Picture.Device, std::move(declared), &S_->Picture.Face, S_->Picture.Standing, S_->Error)) {
    S_->Picture.Standing.reset();
    return false;
  }
  if (!wasScrolled.empty() && !S_->Picture.Standing->Scrolled(std::move(wasScrolled), S_->Error)) {
    return false;
  }
  S_->Session.Declared = scenario;
  S_->Session.Taken = true;
  S_->Session.Carried = Unacted(scenario);
  S_->Error.clear();
  return generated(scenario);
}

bool Engine::generated(const Scenario &scenario) {
  Ask ask;
  ask.EastM = scenario.Ground.Origin.LongitudeDeg;
  ask.NorthM = scenario.Ground.Origin.LatitudeDeg;
  ask.ExtentM = scenario.Ground.Origin.RadiusM;

  Geometry made;
  const auto asked = [&](const std::string &kind) {
    const Generates *const stood = S_->World.Offering.Named(kind);
    if (stood == nullptr) { return true; }
    if (stood->Make(ask, made)) { return true; }
    S_->Error = "the generator of kind '" + kind + "' refused to make anything";
    return false;
  };

  for (const Asset &shown : scenario.Assets) {
    if (shown.Kind != "generated") { continue; }
    if (!asked(shown.Uri)) { return false; }
  }
  for (const Generator &named : scenario.Generators) {
    if (!asked(named.Kind)) { return false; }
  }
  return made.Parts() == 0 || setGeometry(made);
}

bool Engine::readScenarioInto(std::string_view path, Scenario &out) {
  const std::string held(path);
  std::string text;
  if (!SlurpFile(held, text, S_->Error)) { return false; }

  if (!ReadScenario(text.c_str(), text.size(), out, S_->Error)) {
    S_->Error = held + ": " + S_->Error;
    return false;
  }

  S_->Session.LayerTrace.clear();
  if (out.Layers.empty()) { return true; }
  const size_t cut = held.find_last_of('/');
  const std::string dir = cut == std::string::npos ? std::string() : held.substr(0, cut + 1);
  for (const Layer &layer : out.Layers) {
    const std::string named = layer.Id.empty() ? layer.Path : layer.Id;
    if (!LayerActive(layer, out.Named.Active)) {
      S_->Session.LayerTrace.push_back("layer '" + named + "' is inactive -- its set '" + layer.Set +
                               "' is not selected by active=\"" + out.Named.Active + "\"");
      continue;
    }
    const std::string at =
        (!layer.Path.empty() && layer.Path.front() == '/') ? layer.Path : dir + layer.Path;
    std::string fragmentText;
    if (!SlurpFile(at, fragmentText, S_->Error)) { return false; }
    if (!ApplyLayer(out, fragmentText.c_str(), fragmentText.size(), named, S_->Session.LayerTrace,
                    S_->Error)) {
      S_->Error = at + ": " + S_->Error;
      return false;
    }
  }
  return true;
}

bool Engine::setGeometry(const Geometry &geometry) {
  if (!geometry.Whole()) {
    S_->Error = "the geometry stands no whole part, and a subject of nothing is a refusal rather "
                "than an empty picture";
    return false;
  }
  Gltf::Subject handed;
  if (!handed.Assemble(geometry)) {
    S_->Error = handed.Error();
    return false;
  }
  S_->Blocks(handed);
  if (!S_->Picture.Standing) {
    S_->Picture.Handed = std::move(handed);
    S_->Picture.Carrying = true;
    S_->Error.clear();
    return true;
  }
  return S_->Picture.Standing->Restand(handed, 0, S_->Error);
}

bool Engine::readScenario(std::string_view path) {
  Scenario scenario;
  if (!readScenarioInto(path, scenario)) { return false; }
  S_->Session.Declared = scenario;
  S_->Session.Taken = false;
  S_->Session.Carried = Unacted(scenario);
  S_->Session.Carried.insert(S_->Session.Carried.end(), S_->Session.LayerTrace.begin(), S_->Session.LayerTrace.end());
  S_->Error.clear();
  return true;
}

Scene &Engine::scene(void) { return S_->Cast.Scene; }

const Scene &Engine::scene(void) const { return S_->Cast.Scene; }

const Scenario &Engine::declaration() const { return S_->Session.Declared; }

}
