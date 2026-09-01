#include "EngineHeld.h"
#include <memory>
#include <expected>
#include <cstddef>
#include <string>
#include <utility>
#include <iterator>
#include <vector>
#include <string_view>
#include <cstdint>
#include <functional>
#include <chrono>

namespace outshine {

namespace {}

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
  if (!outshine::Assemble(declared,
                          S_->Cast.Scene,
                          S_->Cast.Bodies,
                          S_->Cast.Drives,
                          S_->Cast.Kinds,
                          S_->Cast.Stood,
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
  if (!declared.Buses.empty() || !declared.Sounds.empty()) { S_->Session.Mixing = false; }

  return (S_->Routes()) ? Result{} : std::unexpected(S_->Error);
}

namespace {

using Assembler = bool (*)(const Scene &,
                           const Assembled &,
                           const Column<Body> &,
                           const Column<Journey> &,
                           const WorldSettings &,
                           Ground::GroundStack &,
                           Data::Transport &,
                           const Sim::Provision &,
                           Sink &,
                           Sim::DriveProduct &);

struct Travelling_ {
  Travels By;
  const char *Named;
  Assembler How;
};

constexpr size_t kTravels = 4;

const Travelling_ kAssemblers[kTravels] = {
    {.By = Travels::Walk, .Named = "foot", .How = nullptr},
    {.By = Travels::Drive, .Named = "road", .How = &Sim::AssembleDrive},
    {.By = Travels::Fly, .Named = "air", .How = nullptr},
    {.By = Travels::Rail, .Named = "rail", .How = nullptr},
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

} // namespace

bool Engine::State::Routes() {
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
  const Sim::Provision kept{
      .CacheDir = Session.Under.Cache,
      .AssetsDir = Session.Under.Shipped,
      .Providers = {Data::ShippedProviders().begin(), Data::ShippedProviders().end()}};
  const bool routed = Assembles(declared.Routed.By)(Cast.Scene,
                                                    Cast.Stood,
                                                    Cast.Bodies,
                                                    Cast.Drives,
                                                    declared.Ground,
                                                    World.Stack,
                                                    *World.Wire,
                                                    kept,
                                                    say,
                                                    Ticking.Drive);
  if (routed) {
    World.Stack.Restand(
        Ticking.Drive.Way.FrameLat, Ticking.Drive.Way.FrameLon, Ground::kStreamBudgetMs);
    Ticking.Surface = std::make_unique<Sim::GroundSupport>(World.Stack, Ticking.Drive.Surfaces);
    Ticking.Surface->Restand();
  }
  Session.Carried.insert(Session.Carried.end(),
                         std::make_move_iterator(say.Lines().begin()),
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
              "bounded by and would never arrive -- p01 is " +
              Said(slowestMs) + " m/s";
      return false;
    }
    const double stepS = Session.Declared.Motion.StepS > 0.0 ? Session.Declared.Motion.StepS : 1.0;
    Ticking.MostSteps =
        static_cast<size_t>(Ticking.Drive.Way.Line.LengthM() / slowestMs / stepS) + 1u;
    Published.Places("the steps the plan allows at its slowest station",
                     static_cast<double>(Ticking.MostSteps),
                     "steps");
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
  int widthPx = 0;
  int heightPx = 0;
  SDL_GetWindowSizeInPixels(presents, &widthPx, &heightPx);
  S_->Picture.Targeted = true;
  const auto standing = S_->Picture.Device.DrawsInto(widthPx, heightPx, presents);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return std::unexpected(S_->Error);
  }
  S_->Picture.Frame = Extent{.WidthPx = widthPx, .HeightPx = heightPx};
  return {};
}

Result Engine::drawsInto(Extent offscreen) {
  S_->Picture.Targeted = true;
  const auto standing =
      S_->Picture.Device.DrawsInto(offscreen.WidthPx, offscreen.HeightPx, nullptr);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return std::unexpected(S_->Error);
  }
  S_->Picture.Frame = offscreen;
  return {};
}

void Engine::setRoots(Roots roots) {
  S_->Session.Under = std::move(roots);
}

namespace {}

void Engine::offers(Host *host) {
  S_->Offered = host;
}

namespace {}

void Engine::offers(const Generates &maker) {
  (void)S_->World.Offering.offers(maker);
}

namespace {}

const std::vector<std::string> &Engine::unacted() const {
  return S_->Session.Carried;
}

const std::vector<Measure> &Engine::measures() const {
  return S_->Published.Numbers();
}

bool Engine::settled() const {
  return S_->World.AskedWanted > 0 && S_->World.AskedPending == 0 && S_->World.Bare == 0 &&
         S_->World.Grown && S_->World.Stack.Ingested();
}

Result Renderer::render(Extent frame) {
  return Of_->render(frame) ? Result{} : std::unexpected(Of_->error());
}

Result Renderer::saveScreenshot(std::string_view path) {
  return Of_->saveScreenshot(path) ? Result{} : std::unexpected(Of_->error());
}

int Renderer::settleFrames() const {
  return Of_->S_->Picture.Device.SettleFrames();
}

Result Renderer::readPixels(std::vector<uint8_t> &rgba) {
  return Of_->readPixels(rgba) ? Result{} : std::unexpected(Of_->error());
}

Result Renderer::readPixels(Buffer which, std::vector<float> &out) {
  return Of_->readPixels(which, out) ? Result{} : std::unexpected(Of_->error());
}

Renderer Engine::renderer() {
  return Renderer(*this);
}

SwapChain Engine::swapChain() {
  return SwapChain(*this);
}

Extent SwapChain::extent() const {
  return Of_->canvas();
}

bool SwapChain::presents() const {
  return Of_->presenting();
}

Result Renderer::beginFrame(SwapChain &into) {
  if (into.extent().WidthPx <= 0 || into.extent().HeightPx <= 0) {
    return std::unexpected(std::string("a frame is begun against a canvas and this one is "
                                       "0x0 -- drawsInto declares it before a frame opens"));
  }
  return Of_->beginFrame() ? Result{} : std::unexpected(Of_->error());
}

Result Renderer::endFrame() {
  return Of_->endFrame() ? Result{} : std::unexpected(Of_->error());
}

Result Renderer::flushAndWait() {
  return Of_->flushAndWait() ? Result{} : std::unexpected(Of_->error());
}

bool Engine::sampleHeight(double latitudeDeg, double longitudeDeg, double &heightM) const {
  if (!S_->World.Stack.Opened()) {
    S_->Error =
        "a height was asked for at " + std::to_string(latitudeDeg) + ", " +
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

double Engine::loadProgress() const {
  const size_t wanted = S_->World.AskedWanted;
  if (wanted == 0) { return 1.0; }
  const size_t missing = S_->World.AskedPending;
  if (missing >= wanted) { return 0.0; }
  return static_cast<double>(wanted - missing) / static_cast<double>(wanted);
}

constexpr double kMostWaitS = 0.05;

Loading Engine::loading() const {
  Loading said;
  said.GroundWanted = S_->World.AskedWanted;
  said.GroundArrived = S_->World.AskedWanted >= S_->World.AskedPending
                           ? S_->World.AskedWanted - S_->World.AskedPending
                           : 0;
  if (!S_->World.Stack.Opened()) { return said; }
  if (const Ground::OsmField *vectors = S_->World.Stack.Vectors()) {
    said.VectorArrived = vectors->Tiles().size();
    const int pending = vectors->PendingTiles();
    said.VectorWanted = said.VectorArrived + (pending > 0 ? static_cast<size_t>(pending) : 0);
  }
  const Ground::TilePool::Ledger counted = S_->World.Stack.Pool().Counters();
  said.Outstanding = counted.Outstanding > 0 ? static_cast<size_t>(counted.Outstanding) : 0;
  said.FetchedMB = counted.FetchedMB;
  said.MeanFetchMs = counted.Posts > 0 ? counted.FetchMs / static_cast<double>(counted.Posts) : 0.0;
  return said;
}

Result Engine::preload(double patienceS) {
  return preload(patienceS, {});
}

Result Engine::preload(double patienceS, const std::function<void(const Loading &)> &tell) {
  const auto began = std::chrono::steady_clock::now();
  const auto say = [&]() {
    if (!tell) { return; }
    Loading said = loading();
    said.ElapsedS = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    said.Megabits = said.ElapsedS > 0.0 ? said.FetchedMB * 8.0 / said.ElapsedS : 0.0;
    tell(said);
  };
  const double bound = patienceS > 0.0 ? patienceS : 0.0;
  if (!S_->Session.Declared.Ground.Declared) {
    say();
    return Result{};
  }
  for (;;) {
    S_->Published.Opens();
    if (!S_->Asks()) { return std::unexpected(S_->Error); }
    const double atLat = S_->Session.Declared.Ground.Origin.LatitudeDeg;
    const double atLon = S_->Session.Declared.Ground.Origin.LongitudeDeg;
    S_->World.Stack.Restand(atLat, atLon, 0.0);
    (void)S_->Grows(atLat, atLon);
    say();
    if (S_->World.AskedWanted > 0 && S_->World.AskedPending == 0 && S_->World.Grown &&
        S_->World.Stack.Ingested()) {
      if (!S_->Grounds(true)) { return std::unexpected(S_->Error); }
      if (settled()) { return Result{}; }
    }
    if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >= bound) {
      const bool built = S_->Grounds(true);
      S_->Error = "the world at this place did not become resident within " +
                  std::to_string(bound) + " s -- " + std::to_string(S_->World.Pending) + " of " +
                  std::to_string(S_->World.Wanted) + " tile(s) still pending" +
                  (built ? "" : ", and what did arrive would not build");
      return std::unexpected(S_->Error);
    }
    const double leftS =
        bound - std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    if (!S_->World.Stack.Opened()) { continue; }
    (void)S_->World.Stack.Pool().AwaitLanding(leftS < kMostWaitS ? leftS : kMostWaitS);
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

bool Engine::standing() const {
  return S_->Picture.Standing != nullptr;
}

const std::string &Engine::error() const {
  return S_->Error;
}

} // namespace outshine
