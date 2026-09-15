#include "EngineHeld.h"
#include "geo/Mercator.h"
#include "math/Units.h"
#include <array>
#include <memory>
#include <expected>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <iterator>
#include <vector>
#include <string_view>
#include <cstdint>
#include <functional>
#include <chrono>
#include <cmath>

namespace outshine {

namespace Says {
constexpr auto kNoTerrainRequests = "terrain requests absent";
constexpr auto kPendingTerrain = "terrain downloads pending";
constexpr auto kMissingTerrain = "terrain coverage missing";
constexpr auto kMissingNeighbours = "terrain neighbours missing";
constexpr auto kPendingSnapshot = "generator snapshot pending";
constexpr auto kPendingIngestion = "world ingestion pending";
constexpr auto kPendingClassification = "terrain classification pending";
constexpr auto kPendingVectors = "vector tiles pending";
constexpr auto kPendingVegetation = "vegetation prototypes pending";

constexpr auto kInvalidHeightCoordinate =
    "height query requires finite longitude in [-180,180] and latitude in [-90,90] degrees";
constexpr auto kHeightOutsideCoverage = "height query is outside Mercator terrain coverage";
constexpr auto kInvalidPreloadBudget = "preload requires finite nonnegative seconds";
constexpr auto kForeignSwapChain = "the swap chain belongs to another engine";
constexpr auto kTargetInsideFrame = "end the open frame before changing its target";
constexpr auto kNullWindow = "the target window is null";
constexpr auto kWindowExtentFailed = "could not read the target window extent: ";
constexpr auto kEmptyGeneratorKind = "generator registration needs a nonempty kind";
constexpr auto kDuplicateGeneratorKind = "generator kind is already registered";
}

constexpr double kBitsPerByte = 8.0;

Engine::Engine() : S_(std::make_unique<State>()) {}

Result Engine::assemble() {
  if (!S_->Session.Taken) {
    S_->Error = "declare content before assembling simulation";
    return std::unexpected(S_->Error);
  }
  const Scenario::Document &declared = S_->Session.Declared;
  const auto capacity = RequiredEntityCapacity(declared);
  if (!capacity) {
    S_->Error = capacity.error();
    return std::unexpected(S_->Error);
  }
  const size_t named = *capacity;
  auto candidate = std::make_unique<SimulationState>();
  if (named != 0) {
    if (!candidate->Entities.open(named) || !candidate->Bodies.Open(candidate->Entities) ||
        !candidate->Kinds.Open(candidate->Entities)) {
      S_->Error = "could not allocate simulation entity storage";
      return std::unexpected(S_->Error);
    }
    if (!outshine::Assemble(declared,
                            candidate->Entities,
                            candidate->Bodies,
                            candidate->Kinds,
                            candidate->Stood,
                            S_->Error)) {
      return std::unexpected(S_->Error);
    }
  }
  if (!declared.Tables.empty()) {
    auto book = TableBook::Stand(declared.Tables);
    if (!book) {
      S_->Error = std::move(book).error();
      return std::unexpected(S_->Error);
    }
    candidate->Tables.emplace(*std::move(book));
  }
  if (!declared.Volumes.empty() || !declared.Events.empty()) {
    auto triggers = TriggerField::Stand(declared.Volumes, declared.Events);
    if (!triggers) {
      S_->Error = triggers.error();
      return std::unexpected(S_->Error);
    }
    candidate->Triggers.emplace(std::move(*triggers));
  }
  candidate->DeclarationRevision = S_->Session.DeclarationRevision;
  candidate->PrepareBodies();
  std::vector<std::string_view> followed;
  followed.reserve(declared.Views.size());
  for (const auto &view : declared.Views) { followed.push_back(view.Follows); }
  auto viewBodies = candidate->BindBodies(followed);
  if (!viewBodies) {
    S_->Error = viewBodies.error();
    return std::unexpected(S_->Error);
  }
  candidate->ViewBodies = std::move(*viewBodies);
  auto previous = std::exchange(S_->Simulation, std::move(candidate));
  if (S_->Picture.Targeted && !S_->Composes()) {
    S_->Simulation = std::move(previous);
    return std::unexpected(S_->Error);
  }
  S_->Session.Sounding.reset();
  S_->Session.AudioBodies.clear();
  S_->Error.clear();
  return {};
}

Engine::~Engine() = default;

Result Engine::drawsInto(SDL_Window *presents) {
  if (S_->Picture.Scope != FrameScope::Closed) {
    S_->Error = Says::kTargetInsideFrame;
    return std::unexpected(S_->Error);
  }
  if (presents == nullptr) {
    S_->Error = Says::kNullWindow;
    return std::unexpected(S_->Error);
  }
  int widthPx = 0;
  int heightPx = 0;
  if (!SDL_GetWindowSizeInPixels(presents, &widthPx, &heightPx)) {
    S_->Error = std::string(Says::kWindowExtentFailed) + SDL_GetError();
    return std::unexpected(S_->Error);
  }
  const auto standing = S_->Picture.Device.DrawsInto(widthPx, heightPx, presents);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return std::unexpected(S_->Error);
  }
  S_->Picture.Frame = Extent{.WidthPx = widthPx, .HeightPx = heightPx};
  S_->Picture.Targeted = true;
  S_->Error.clear();
  return {};
}

Result Engine::drawsInto(Extent offscreen) {
  if (S_->Picture.Scope != FrameScope::Closed) {
    S_->Error = Says::kTargetInsideFrame;
    return std::unexpected(S_->Error);
  }
  const auto standing =
      S_->Picture.Device.DrawsInto(offscreen.WidthPx, offscreen.HeightPx, nullptr);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return std::unexpected(S_->Error);
  }
  S_->Picture.Frame = offscreen;
  S_->Picture.Targeted = true;
  S_->Error.clear();
  return {};
}

void Engine::setRoots(Roots roots) {
  S_->Session.Under = std::move(roots);
}

void Engine::offers(Host *host) {
  S_->Offered = host;
}

Result Engine::offers(const Generators::Generator &maker) {
  const auto offered = S_->World.Offering.offers(maker);
  if (offered) { return {}; }
  return std::unexpected(offered.error() == Generators::Registry::RegistrationError::EmptyKind
                             ? Says::kEmptyGeneratorKind
                             : Says::kDuplicateGeneratorKind);
}

const std::vector<std::string> &Engine::unacted() const {
  return S_->Session.Carried;
}

const std::vector<Measure> &Engine::measures() const {
  return S_->Published.Numbers();
}

WorldReadiness Engine::State::Readiness() const {
  const auto classes = World.Stack.Classes().Read();
  const uint64_t version = classes ? classes->Version() : 0;
  const auto *vectors = World.Stack.Vectors();
  return {
      {World.AskedWanted > 0 ? "" : Says::kNoTerrainRequests,
       World.AskedPending == 0 ? "" : Says::kPendingTerrain,
       World.Bare == 0 ? "" : Says::kMissingTerrain,
       World.RimsMissing == 0 ? "" : Says::kMissingNeighbours,
       World.Grown ? "" : Says::kPendingSnapshot,
       World.Stack.Ingested() && World.LaidFootprintsRevision == World.Stack.Footprints().Revision()
           ? ""
           : Says::kPendingIngestion,
       World.Stack.Classes().Complete() && World.LaidClasses == version
           ? ""
           : Says::kPendingClassification,
       vectors != nullptr && vectors->PendingTiles() == 0 ? "" : Says::kPendingVectors,
       !Picture.Standing || !Session.Declared.Ground.VegetationEnabled ||
               (World.Crowns && World.Crowns->Ready())
           ? ""
           : Says::kPendingVegetation}};
}

bool Engine::settled() const {
  return S_->Readiness().Ready();
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
  if (into.Of_ != Of_) { return std::unexpected(std::string(Says::kForeignSwapChain)); }
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

Holds<double> Engine::sampleHeight(const LongitudeLatitudeHeight &at) const {
  constexpr double poleDeg = kDegPerHalfTurn / 2.0;
  if (!std::isfinite(at.LongitudeDeg) || !std::isfinite(at.LatitudeDeg) ||
      std::abs(at.LongitudeDeg) > kDegPerHalfTurn || std::abs(at.LatitudeDeg) > poleDeg) {
    return std::unexpected(Says::kInvalidHeightCoordinate);
  }
  if (std::abs(at.LatitudeDeg) > kMercatorLatMaxDeg) {
    return std::unexpected(Says::kHeightOutsideCoverage);
  }
  const std::string there = std::to_string(at.LatitudeDeg) + ", " + std::to_string(at.LongitudeDeg);
  if (!S_->World.Stack.Opened()) {
    S_->Error = "a height was asked for at " + there +
                " and no world stands -- a scenario declares one before anything can be placed on "
                "it";
    return std::unexpected(S_->Error);
  }
  const std::optional<double> aslM =
      S_->World.Stack.Ground()
          .At({.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg})
          .AslM();
  if (!aslM) {
    S_->Error = "the terrain at " + there +
                " is not resident, so the height there is not a number this engine may invent";
    return std::unexpected(S_->Error);
  }
  return *aslM;
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

namespace {
void ReportPreload(const Engine &engine,
                   std::chrono::steady_clock::time_point began,
                   const std::function<void(const Loading &)> &tell) {
  if (!tell) { return; }
  Loading said = engine.loading();
  said.ElapsedS = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  said.Megabits = said.ElapsedS > 0.0 ? said.FetchedMB * kBitsPerByte / said.ElapsedS : 0.0;
  tell(said);
}
}

bool Engine::State::CanFinishPreload() const {
  return World.AskedWanted > 0 && World.AskedPending == 0 && World.Grown && World.Stack.Ingested();
}

Result Engine::State::PumpPreload() {
  if (World.Stack.Overflowing()) { return PreloadOverflow(); }
  Published.Opens();
  if (!Asks()) { return std::unexpected(Error); }
  const LongitudeLatitude stands = WhereTheEyeStands();
  const double atLat = stands.LatitudeDeg;
  const double atLon = stands.LongitudeDeg;
  HandsPiecesOver();
  const auto streamed = World.Stack.Restand(stands);
  if (!streamed) {
    Error = streamed.error();
    return std::unexpected(Error);
  }
  if (World.Stack.Overflowing()) { return PreloadOverflow(); }
  if (!Bakes(kBakesLandedInPreload)) { return std::unexpected(Error); }
  (void)Grows(atLat, atLon);
  return {};
}

Result Engine::State::PreloadOverflow() {
  Error = "the world at this place holds " + std::to_string(World.Stack.HeapBytes()) +
          " bytes against a ceiling of " + std::to_string(Ground::GroundStack::kHoldsBytes) +
          ", so it stopped ingesting part-way. What it did take depends on which tiles had "
          "landed when the round ran, which is a picture the declaration does not name";
  return std::unexpected(Error);
}

Result Engine::State::PreloadTimeout(double bound) {
  Error = "the world did not become resident within " + std::to_string(bound) +
          " s: " + Readiness().Describe();
  return std::unexpected(Error);
}

Result Engine::preload(double patienceS) {
  return preload(patienceS, {});
}

Result Engine::preload(double patienceS, const std::function<void(const Loading &)> &tell) {
  if (!std::isfinite(patienceS) || patienceS < 0.0) {
    return std::unexpected(Says::kInvalidPreloadBudget);
  }
  const auto began = std::chrono::steady_clock::now();
  const double bound = patienceS;
  if (!S_->Session.Declared.Ground.Declared) {
    ReportPreload(*this, began, tell);
    return Result{};
  }
  for (;;) {
    if (const auto pumped = S_->PumpPreload(); !pumped) { return pumped; }
    ReportPreload(*this, began, tell);
    if (S_->CanFinishPreload()) {
      if (!S_->Grounds(true)) { return std::unexpected(S_->Error); }
      if (!S_->UpdateCrowns(true)) { return std::unexpected(S_->Error); }
      if (settled()) { return Result{}; }
    }
    if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >= bound) {
      return S_->PreloadTimeout(bound);
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

}
