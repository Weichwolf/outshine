#include "EngineHeld.h"

namespace outshine {




namespace {


}

Engine::Engine() : S_(std::make_unique<State>()) {}

Result Engine::assemble() {
  if (!S_->Session.Taken) {
    S_->Error = "a declaration was read and never DECLARED, so this engine holds a scenario it "
                "was not asked to stand -- Read fills the declaration, Declare hands it over, "
                "and Assemble builds what Declare stood";
    return std::unexpected(S_->Error);
  }
  const Scenario &declared = S_->Session.Declared;
  const size_t named = AssembledCapacity(declared);
  if (named == 0) {
    S_->Ticking.Drove = false;
    if (!S_->Routes()) { return std::unexpected(S_->Error); }
    if (!S_->Composes()) { return std::unexpected(S_->Error); }
    const bool standsOnAWorld = S_->Ticking.Drove;
    return (!standsOnAWorld || S_->Rides()) ? Result{} : std::unexpected(S_->Error);
  }
  if (!S_->Cast.Scene.open(named) || !S_->Cast.Bodies.Open(S_->Cast.Scene) ||
      !S_->Cast.Drives.Open(S_->Cast.Scene) || !S_->Cast.Kinds.Open(S_->Cast.Scene)) {
    S_->Error = "the scene did not open for the " + std::to_string(named) +
                " entities the declaration names";
    return std::unexpected(S_->Error);
  }
  if (!outshine::Assemble(declared, S_->Cast.Scene, S_->Cast.Bodies, S_->Cast.Drives, S_->Cast.Kinds, S_->Cast.Stood,
                          S_->Error)) {
    return std::unexpected(S_->Error);
  }
  if (!declared.Tables.empty()) {
    auto book = TableBook::Stand(declared.Tables);
    if (!book) {
      S_->Error = std::move(book).error();
      return std::unexpected(S_->Error);
    }
    S_->Session.Tabled.emplace(*std::move(book));
  }
  if (!declared.Buses.empty() || !declared.Sounds.empty()) {
    S_->Session.Mixing = false;
  }

  return (S_->Routes()) ? Result{} : std::unexpected(S_->Error);
}

namespace {

using Assembler = bool (*)(const Scene &, const Assembled &, const Column<Body> &,
                           const Column<Journey> &, const WorldSettings &, Ground::GroundStack &,
                           Data::Transport &, const Sim::Provision &, Sink &, Sim::DriveProduct &);

struct Travelling_ {
  Travels By;
  const char *Named;
  Assembler How;
};

constexpr size_t kTravels = 4;

const Travelling_ kAssemblers[kTravels] = {
    {Travels::Walk, "foot", nullptr},
    {Travels::Drive, "road", &Sim::AssembleDrive},
    {Travels::Fly, "air", nullptr},
    {Travels::Rail, "rail", nullptr},
};

[[nodiscard]] Assembler Assembles(Travels by) {
  for (const Travelling_ &one : kAssemblers) {
    if (one.By == by) { return one.How; }
  }
  return nullptr;
}

[[nodiscard]] const char *Travelling(Travels by) {
  for (const Travelling_ &one : kAssemblers) {
    if (one.By == by) { return one.Named; }
  }
  return "an unnamed way";
}

}

bool Engine::State::Routes(void) {
  const Scenario &declared = Session.Declared;
  Ticking.Drove = false;
  Ticking.Freestanding.clear();
  for (const Body &stands : declared.Bodies) {
    if (!stands.Placed) { continue; }
    Physics::Rigid held;
    held.MassKg = stands.MassKg;
    for (int axis = 0; axis < 3; ++axis) {
      held.PositionM[axis] = stands.Stands.AtM[axis];
      held.InertiaKgM2[axis] = stands.InertiaKgM2[axis];
    }
    held.OrientationQ[0] = stands.Stands.FacingXyzw[3];
    held.OrientationQ[1] = stands.Stands.FacingXyzw[0];
    held.OrientationQ[2] = stands.Stands.FacingXyzw[1];
    held.OrientationQ[3] = stands.Stands.FacingXyzw[2];
    Ticking.Freestanding.push_back(held);
  }
  if (!declared.Routed.Declared) { return true; }
  if (Assembles(declared.Routed.By) == nullptr) {
    Error = std::string("the scenario declares a journey travelling by ") +
            Travelling(declared.Routed.By) +
            ", and nothing assembles that -- a mode the engine cannot lay a corridor for is a "
            "refusal, never a journey that quietly does not happen";
    return false;
  }
  if (!World.Wire) {
    if (Session.Under.Offline) {
      World.Wire = std::make_unique<Unwired>();
    } else {
      World.Wire = std::make_unique<Fetching>(Fetching::Config{});
    }
  }
  Collecting say;
  const Sim::Provision kept{Session.Under.Cache, Session.Under.Shipped,
                            {Data::ShippedProviders().begin(), Data::ShippedProviders().end()}};
  const bool routed = Assembles(declared.Routed.By)(Cast.Scene, Cast.Stood, Cast.Bodies, Cast.Drives, declared.Ground,
                                                    World.Stack, *World.Wire, kept, say, Ticking.Drive);
  if (routed) {
    World.Stack.Restand(Ticking.Drive.Way.FrameLat, Ticking.Drive.Way.FrameLon);
    Ticking.Surface = std::make_unique<Sim::GroundSupport>(World.Stack, Ticking.Drive.Surfaces);
    Ticking.Surface->Restand();
  }
  Session.Carried.insert(Session.Carried.end(), std::make_move_iterator(say.Lines().begin()),
                     std::make_move_iterator(say.Lines().end()));
  Published.Stands(std::move(say.Numbers()));
  if (!routed) {
    Error = say.WhyNot();
    return false;
  }
  Published.Places("how long the corridor is", Ticking.Drive.Way.Line.LengthM(), "m");
  Published.Places("how far along it the body has come", 0.0, "m");
  if (Picture.Standing && Ticking.Drive.Stood.MetresPerAssetUnit > 0.0) {
    Picture.Standing->ScaledBy(Ticking.Drive.Stood.MetresPerAssetUnit);
  }
  Ticking.Drove = true;
  {
    const double slowestMs = Ticking.Drive.Way.Profile.Quantile(0.01);
    if (!(slowestMs > 0.0)) {
      Error = "a hundredth of this speed plan stands still, so the drive has no pace to be "
              "bounded by and would never arrive -- p01 is " + Said(slowestMs) + " m/s";
      return false;
    }
    const double stepS = Session.Declared.Motion.StepS > 0.0 ? Session.Declared.Motion.StepS : 1.0;
    Ticking.MostSteps = (size_t)(Ticking.Drive.Way.Line.LengthM() / slowestMs / stepS) + 1u;
    Published.Places("the steps the plan allows at its slowest station", (double)Ticking.MostSteps, "steps");
  }
  if (!Composes()) { return false; }
  const bool aWorldStands = Ticking.Drove;
  if (aWorldStands && !Rides()) { return false; }
  return true;
}

Engine::~Engine() = default;
Engine::Engine(Engine &&) noexcept = default;
Engine &Engine::operator=(Engine &&) noexcept = default;

Result Engine::drawsInto(SDL_Window *presents) {
  if (presents == nullptr) {
    S_->Error = "a window is what DrawsInto presents on, and this one is none -- an engine that "
                "draws nowhere is declared with an Extent instead";
    return std::unexpected(S_->Error);
  }
  int widthPx = 0, heightPx = 0;
  SDL_GetWindowSizeInPixels(presents, &widthPx, &heightPx);
  S_->Picture.Targeted = true;
  const auto standing = S_->Picture.Device.DrawsInto(widthPx, heightPx, presents);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return std::unexpected(S_->Error);
  }
  S_->Picture.Frame = Extent{widthPx, heightPx};
  return {};
}

Result Engine::drawsInto(Extent offscreen) {
  S_->Picture.Targeted = true;
  const auto standing = S_->Picture.Device.DrawsInto(offscreen.WidthPx, offscreen.HeightPx, nullptr);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return std::unexpected(S_->Error);
  }
  S_->Picture.Frame = offscreen;
  return {};
}

void Engine::setRoots(Roots roots) { S_->Session.Under = std::move(roots); }








namespace {


}

void Engine::offers(Host *host) { S_->Offered = host; }



namespace {





}


void Engine::offers(const Generates &maker) { (void)S_->World.Offering.offers(maker); }



namespace {


}







const std::vector<std::string> &Engine::unacted() const { return S_->Session.Carried; }


const std::vector<Measure> &Engine::measures() const { return S_->Published.Numbers(); }

bool Engine::settled(void) const {
  // SETTLED MEANS THE WHOLE PICTURE, NOT ONLY ITS GROUND. Terrain tiles are one half; the OSM
  // fields the generators grow from are the other, and they arrive on their own schedule. A client
  // that took its picture when the last tile landed got correct terrain with no buildings and no
  // streets on it -- measured, six places at `0 instanced` with the snapshot answering Waiting.
  return S_->World.AskedWanted > 0 && S_->World.AskedPending == 0 && S_->World.Grown;
}

// A LOADING BAR IS A NUMBER, and every game has one. Cesium's tileset answers
// `ComputeLoadProgress()` and Unreal answers `GetAsyncLoadPercentage`; both let the client draw the
// bar rather than guessing. This is the same question in this engine's own terms: of the terrain
// the current view wants, what share has actually arrived. A place with nothing wanted is loaded.
Result Renderer::render(Extent frame) {
  return Of_->render(frame) ? Result{} : std::unexpected(Of_->error());
}
Result Renderer::saveScreenshot(std::string_view path) {
  return Of_->saveScreenshot(path) ? Result{} : std::unexpected(Of_->error());
}
Result Renderer::readPixels(std::vector<uint8_t> &rgba) {
  return Of_->readPixels(rgba) ? Result{} : std::unexpected(Of_->error());
}

Renderer Engine::renderer(void) { return Renderer(*this); }

bool Engine::sampleHeight(double latitudeDeg, double longitudeDeg, double &heightM) const {
  if (!S_->World.Stack.Opened()) {
    S_->Error = "a height was asked for at " + std::to_string(latitudeDeg) + ", " +
                std::to_string(longitudeDeg) +
                " and no world stands -- a scenario declares one before anything can be placed on it";
    return false;
  }
  double aslM = 0.0;
  if (!S_->World.Stack.Ground().At(latitudeDeg, longitudeDeg).TryAslM(&aslM)) {
    S_->Error = "the terrain at " + std::to_string(latitudeDeg) + ", " +
                std::to_string(longitudeDeg) +
                " is not resident, so the height there is not a number this engine may invent";
    return false;
  }
  heightM = aslM;
  return true;
}

double Engine::loadProgress(void) const {
  // THIS READS THE ASK, and it only became truthful when the pool started HOLDING what it built.
  // While `Done_` was a one-shot mailbox, a tile read "pending" again the moment it had been used,
  // so this answered 0 per cent with 88 of 128 tiles carrying ground. It must not read the BUILD
  // walk's numbers either: that walk does not count bare tiles at all, so `Bare == 0` there is a
  // fact about the walk rather than about the world, and reading it made `settled()` answer yes
  // with 35 tiles still pending.
  const size_t wanted = S_->World.AskedWanted;
  if (wanted == 0) { return 1.0; }
  const size_t missing = S_->World.AskedPending;
  if (missing >= wanted) { return 0.0; }
  return (double)(wanted - missing) / (double)wanted;
}

// PRELOAD IS THE CLIENT'S WAIT, NOT THE ENGINE'S. The frame path never blocks -- that is the
// invariant -- so a client that wants a finished picture rather than a progressively refining one
// asks for it HERE, once, bounded in seconds. Filament spells the same distinction
// `SceneRenderer::flushAndWait`; Cesium's tileset reports load progress and the caller decides whether
// to wait on it. Nothing inside advance() or render() ever calls this.
Result Engine::preload(double patienceS) {
  const auto began = std::chrono::steady_clock::now();
  const double bound = patienceS > 0.0 ? patienceS : 0.0;
  for (;;) {
    if (!S_->Asks()) { return std::unexpected(S_->Error); }
    // THE OSM SIDE HAS TO BE DRIVEN TOO. `GroundStack::Restand` is what builds the vector ring and
    // ingests streets, water and footprints from it, and preload never called it -- so through the
    // whole wait the terrain arrived and the OSM fields did not move at all. Measured: land classes
    // and a patch of ground both present, `OSM features` answering 0, one vector tile settled and
    // it was not the one the region asked for, and every place at `0 instanced`.
    const double atLat = S_->Session.Declared.Ground.Origin.LatitudeDeg;
    const double atLon = S_->Session.Declared.Ground.Origin.LongitudeDeg;
    S_->World.Stack.Restand(atLat, atLon);
    (void)S_->Grows(atLat, atLon);
    if (settled()) { return (S_->Grounds(true)) ? Result{} : std::unexpected(S_->Error); }
    if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >= bound) {
      // PATIENCE RUNNING OUT IS NOT A REASON TO SHOW NOTHING. Whatever arrived is built and drawn;
      // the refusal says what is still missing. Building only on a full settle meant a place that
      // loaded 73 per cent of its tiles rendered the bare ellipsoid, because the one call that
      // turns tiles into geometry never ran.
      const bool built = S_->Grounds(true);
      S_->Error = "the world at this place did not become resident within " +
                  std::to_string(bound) + " s -- " + std::to_string(S_->World.Pending) +
                  " of " + std::to_string(S_->World.Wanted) + " tile(s) still pending" +
                  (built ? "" : ", and what did arrive would not build");
      return std::unexpected(S_->Error);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

Result Engine::setView(std::string_view view) {
  if (!S_->Session.Views) {
    S_->Error = "the scenario declares no views, so there is none to take";
    return std::unexpected(S_->Error);
  }
  if (!S_->Session.Views->Take(view)) {
    S_->Error = "the scenario declares no view by that name";
    return std::unexpected(S_->Error);
  }
  return {};
}













bool Engine::standing() const { return S_->Picture.Standing != nullptr; }
const std::string &Engine::error() const { return S_->Error; }

}
