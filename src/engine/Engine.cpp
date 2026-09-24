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
#include <limits>
#include <vector>
#include <string_view>
#include <cstdint>
#include <functional>
#include <chrono>
#include <cmath>
#include <ratio>
#include <span>

namespace outshine {

namespace Says {
constexpr auto kRootsAfterDeclaration =
    "roots cannot change after declaration because prepared assets and world sources retain them";
constexpr auto kNoTerrainRequests = "terrain requests absent";
constexpr auto kPendingTerrain = "terrain downloads pending";
constexpr auto kMissingTerrain = "terrain coverage missing";
constexpr auto kMissingNeighbours = "terrain neighbours missing";
constexpr auto kPendingSnapshot = "generator snapshot pending";
constexpr auto kPendingGroundRevision = "ground for current request pending";
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

Capture::~Capture() {
  release();
}

Capture::Capture(Capture &&other) noexcept : Engine_(std::exchange(other.Engine_, nullptr)) {}

void Capture::release() noexcept {
  if (Engine_ != nullptr) {
    Engine_->S_->World.GroundPublished.EndCapture();
    Engine_->S_->Capturing = false;
    Engine_ = nullptr;
  }
}

Capture &Capture::operator=(Capture &&other) noexcept {
  if (this != &other) {
    release();
    Engine_ = std::exchange(other.Engine_, nullptr);
  }
  return *this;
}

Result Engine::assemble() {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
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

Result Engine::setRenderTarget(SDL_Window *window) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  if (S_->Picture.Scope != FrameScope::Closed) {
    S_->Error = Says::kTargetInsideFrame;
    return std::unexpected(S_->Error);
  }
  if (window == nullptr) {
    S_->Error = Says::kNullWindow;
    return std::unexpected(S_->Error);
  }
  int widthPx = 0;
  int heightPx = 0;
  if (!SDL_GetWindowSizeInPixels(window, &widthPx, &heightPx)) {
    S_->Error = std::string(Says::kWindowExtentFailed) + SDL_GetError();
    return std::unexpected(S_->Error);
  }
  const auto standing = S_->Picture.Device.DrawsInto(widthPx, heightPx, window);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return std::unexpected(S_->Error);
  }
  S_->Picture.Frame = Extent{.WidthPx = widthPx, .HeightPx = heightPx};
  S_->Picture.Targeted = true;
  S_->Error.clear();
  return {};
}

Result Engine::setRenderTarget(Extent offscreen) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
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

Result Engine::setRoots(Roots roots) {
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  if (S_->Session.Taken) {
    S_->Error = Says::kRootsAfterDeclaration;
    return std::unexpected(S_->Error);
  }
  S_->Session.Under = std::move(roots);
  S_->Error.clear();
  return {};
}

void Engine::setInputHost(Host *host) {
  S_->Offered = host;
}

Result Engine::registerGenerator(const Generators::Generator &generator) {
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  const auto registered = S_->World.Offering.registerGenerator(generator);
  if (registered) { return {}; }
  return std::unexpected(registered.error() == Generators::Registry::RegistrationError::EmptyKind
                             ? Says::kEmptyGeneratorKind
                             : Says::kDuplicateGeneratorKind);
}

std::span<const std::string> Engine::unacted() const {
  return S_->Session.Carried;
}

std::span<const DiagnosticSample> Engine::measures() const {
  return S_->Published.Numbers();
}

bool Engine::State::StructuresReady(const Ground::BuildingField &footprints,
                                    const GroundRevision &revision) const {
  if (revision.Quality == GroundQuality::Refined) {
    return World.StructureBuilds.Complete(World.Stack, footprints);
  }
  const Ground::OsmField *const vectors = World.Stack.Vectors();
  if (vectors == nullptr) { return true; }
  const double tileSpanM = footprints.TileSpanM();
  const int rings =
      tileSpanM > 0.0 ? static_cast<int>(revision.Coverage.ContactRadiusM / tileSpanM) : 0;
  return footprints.IngestedWithin(*vectors, rings);
}

bool Engine::State::RefinedGroundIngested(const GroundRevision &revision) const {
  return World.Stack.Ingested() && StructuresReady(World.Stack.Footprints(), revision) &&
         revision.Footprints == World.Stack.Footprints().Revision();
}

bool Engine::State::RefinedGroundClassified(const GroundRevision &revision) const {
  const auto classes = World.Stack.Classes().Read();
  const uint64_t version = classes ? classes->Version() : 0;
  return World.Stack.Classes().Complete() && revision.Classes == version;
}

WorldReadiness Engine::State::Readiness(GroundQuality quality) const {
  const auto *vectors = World.Stack.Vectors();
  const auto &ground = World.GroundPublished.Current();
  const bool refined = quality == GroundQuality::Refined;
  const bool published = ground && ground->Quality >= quality;
  const bool currentRevision =
      World.RequestedRefinedGround &&
      !World.GroundPublished.NeedsRebuild(*World.RequestedRefinedGround, false, false);
  return {{(!refined || World.AskedWanted > 0) ? "" : Says::kNoTerrainRequests,
           (!refined || World.AskedPending == 0) ? "" : Says::kPendingTerrain,
           (!refined || World.Bare == 0) ? "" : Says::kMissingTerrain,
           (!refined || World.RimsMissing == 0) ? "" : Says::kMissingNeighbours,
           World.Grown ? "" : Says::kPendingSnapshot,
           !refined || currentRevision ? "" : Says::kPendingGroundRevision,
           published && (!refined || RefinedGroundIngested(*ground)) ? "" : Says::kPendingIngestion,
           published && (!refined || RefinedGroundClassified(*ground))
               ? ""
               : Says::kPendingClassification,
           vectors != nullptr && (!refined || vectors->PendingTiles() == 0) ? ""
                                                                            : Says::kPendingVectors,
           !Picture.Standing || !Session.Declared.Ground.VegetationEnabled ||
                   (World.Vegetation && World.Vegetation->Ready())
               ? ""
               : Says::kPendingVegetation}};
}

bool Engine::settled(WorldQuality required) const {
  const GroundQuality quality =
      required == WorldQuality::Refined ? GroundQuality::Refined : GroundQuality::Playable;
  return S_->Readiness(quality).Ready();
}

std::string Engine::unsettledReasons(WorldQuality required) const {
  const GroundQuality quality =
      required == WorldQuality::Refined ? GroundQuality::Refined : GroundQuality::Playable;
  std::string reasons = S_->Readiness(quality).Describe();
  if (!reasons.empty() && required == WorldQuality::Refined) {
    reasons += "; " + S_->World.Stack.IngestionStatus();
    reasons += "; " + S_->GroundBuildDiagnostic();
    reasons += "; structure queue=" + std::to_string(S_->World.StructureBuilds.Queued()) +
               ", landed=" + std::to_string(S_->World.StructureBuilds.Landed()) + "/" +
               std::to_string(S_->World.StructureBuilds.Posted());
  }
  return reasons;
}

Holds<Capture> Engine::beginCapture() {
  if (S_->Capturing) { return std::unexpected("a capture already holds this engine"); }
  if (!S_->Picture.Standing) {
    return std::unexpected("capture requires a declared and assembled render scene");
  }
  if (S_->Session.Declared.Ground.Declared && !settled()) {
    return std::unexpected("capture requires a settled published world");
  }
  if (S_->Session.Declared.Ground.Declared && !S_->World.GroundPublished.BeginCapture()) {
    return std::unexpected("capture could not pin the published ground");
  }
  S_->Capturing = true;
  S_->World.Pieces.ForEachDigest([this](TilePieces::DigestRecord piece) {
    const std::string name = "capture: structure tile " + std::to_string(piece.Tile) + " digest ";
    S_->Published.Places(name + "low half",
                         static_cast<double>(piece.Digest & std::numeric_limits<uint32_t>::max()),
                         "digest");
    S_->Published.Places(name + "high half", static_cast<double>(piece.Digest >> 32u), "digest");
    S_->Published.Places(name + "fallback heights", piece.FallbackHeights ? 1.0 : 0.0, "yes/no");
  });
  return Capture(*this);
}

Result Renderer::render(Extent frame) {
  return Of_->render(frame);
}

Result Renderer::saveScreenshot(std::string_view path) {
  return Of_->saveScreenshot(path);
}

int Renderer::settleFrames() const {
  return Of_->S_->Picture.Device.SettleFrames();
}

Result Renderer::readPixels(std::vector<uint8_t> &rgba) {
  return Of_->readPixels(rgba);
}

Result Renderer::readPixels(Buffer which, std::vector<float> &out) {
  return Of_->readPixels(which, out);
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
    return std::unexpected(
        std::string("cannot begin a frame without a render target with positive pixel dimensions"));
  }
  return Of_->beginFrame();
}

Result Renderer::endFrame() {
  return Of_->endFrame();
}

Result Renderer::flushAndWait() {
  return Of_->flushAndWait();
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
  if (!S_->Session.Declared.Ground.Declared || !S_->World.Stack.Opened()) {
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
  if (!S_->Session.Declared.Ground.Declared) { return 1.0; }
  const size_t wanted = S_->World.AskedWanted;
  if (wanted == 0) { return 1.0; }
  const size_t missing = S_->World.AskedPending;
  if (missing >= wanted) { return 0.0; }
  return static_cast<double>(wanted - missing) / static_cast<double>(wanted);
}

constexpr double kMostWaitS = 0.05;
constexpr size_t kPreloadGroundAdvancesMost = 16;

Loading Engine::loading() const {
  Loading said;
  if (!S_->Session.Declared.Ground.Declared) { return said; }
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

bool Engine::State::CanBeginGroundCandidate() const {
  return World.AskedWanted > 0 && World.AskedPlayablePending == 0 && World.Stack.IngestedWithin(0);
}

bool Engine::State::CanAdvanceGroundCandidate() const {
  return World.GroundBuild != nullptr && World.Stack.IngestedWithin(0);
}

Result Engine::State::FinishesPreload() {
  const auto &published = World.GroundPublished.Current();
  const bool structuresReady = published && StructuresReady(World.Stack.Footprints(), *published);
  if ((!published || structuresReady) && !Grounds(true, GroundQuality::Playable)) {
    return std::unexpected(Error);
  }
  if (structuresReady && !UpdateCrowns(true)) { return std::unexpected(Error); }
  return {};
}

std::expected<Engine::State::PreloadFlush, std::string>
Engine::State::FlushPreloadGround(std::chrono::steady_clock::time_point began, double bound) {
  for (size_t advance = 0; advance < kPreloadGroundAdvancesMost; ++advance) {
    if (const Result finished = FinishesPreload(); !finished) {
      return std::unexpected(finished.error());
    }
    if (Readiness(GroundQuality::Playable).Ready() &&
        std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() < bound) {
      return PreloadFlush::Ready;
    }
    if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >= bound) {
      return PreloadFlush::Pending;
    }
    if (!World.GroundBuild) { break; }
  }
  return PreloadFlush::Pending;
}

Result Engine::State::PumpPreload() {
  if (World.Stack.Overflowing()) { return PreloadOverflow(); }
  Published.Opens();
  if (!RequestTerrainCoverage()) { return std::unexpected(Error); }
  const LongitudeLatitude stands = WhereTheEyeStands();
  const double atLat = stands.LatitudeDeg;
  const double atLon = stands.LongitudeDeg;
  HandsPiecesOver();
  const int vectorRing = World.GroundPublished.Current() ? Ground::kVectorRing : 0;
  const auto streamed = World.Stack.Restand(
      stands, {.IngestTilesMost = Ground::kVectorTiles, .VectorRing = vectorRing});
  if (!streamed) {
    Error = streamed.error();
    return std::unexpected(Error);
  }
  if (World.Stack.Overflowing()) { return PreloadOverflow(); }
  if (!Bakes(kBakesLandedInPreload)) { return std::unexpected(Error); }
  const auto growthBegan = std::chrono::steady_clock::now();
  (void)Grows(atLat, atLon);
  Published.Places(
      "preload: generator work",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - growthBegan)
          .count(),
      "ms");
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
  const WorldReadiness readiness = Readiness();
  const std::string pendingGround = Error;
  Error = "the world did not become resident within " + std::to_string(bound) +
          " s: " + readiness.Describe();
  if (!World.Stack.Ingested()) { Error += " (" + World.Stack.IngestionStatus() + ")"; }
  if (!World.GroundPublished.Current() && !pendingGround.empty()) { Error += "; " + pendingGround; }
  Error += "; ground build=" + std::string(GroundBuildStatus());
  if (World.StructureBuilds.Posted() > 0) {
    Error += "; structure bakes=" + std::to_string(World.StructureBuilds.Landed()) + "/" +
             std::to_string(World.StructureBuilds.Posted()) +
             ", discarded=" + std::to_string(World.StructureBuilds.Discarded()) +
             ", queued=" + std::to_string(World.StructureBuilds.Queued()) +
             ", structures=" + std::to_string(World.StructureBuilds.QueuedStructures()) +
             ", deferred=" + std::to_string(World.StructureBuilds.Deferred()) +
             ", meanMs=" + std::to_string(World.StructureBuilds.MeanBakeMs()) +
             ", maxMs=" + std::to_string(World.StructureBuilds.SlowestBakeMs()) +
             ", ranges=" + std::to_string(World.StructureBuilds.CompletedRanges()) +
             ", structuresPerRange=" + std::to_string(StructureBuildTask::StructuresPerRange) +
             ", maxQueueMs=" + std::to_string(World.StructureBuilds.SlowestQueueMs()) +
             ", maxTaskMs=" + std::to_string(World.StructureBuilds.SlowestTaskMs()) +
             ", maxRangeMs=" + std::to_string(World.StructureBuilds.SlowestRangeMs()) +
             ", maxFinalizationMs=" + std::to_string(World.StructureBuilds.SlowestFinalizationMs());
  }
  if (const auto &ground = World.GroundPublished.Current();
      ground && ground->Footprints != World.Stack.Footprints().Revision()) {
    Error += "; footprint revision=" + std::to_string(ground->Footprints) + "/" +
             std::to_string(World.Stack.Footprints().Revision());
  }
  return std::unexpected(Error);
}

void Engine::State::AwaitPreloadProgress(double seconds) {
  if (World.StructureBuilds.AwaitSlice(seconds)) { return; }
  (void)World.Stack.AwaitProgress(seconds);
}

Result Engine::preload(double patienceS) {
  return preload(patienceS, {});
}

Result Engine::preload(double patienceS, const std::function<void(const Loading &)> &tell) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
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
    if (S_->CanBeginGroundCandidate() || S_->CanAdvanceGroundCandidate()) {
      const auto finished = S_->FlushPreloadGround(began, bound);
      if (!finished) { return std::unexpected(finished.error()); }
      if (*finished == State::PreloadFlush::Ready) { return Result{}; }
    }
    if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >= bound) {
      return S_->PreloadTimeout(bound);
    }
    const double leftS =
        bound - std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    if (!S_->World.Stack.Opened()) { continue; }
    const double waitS = leftS < kMostWaitS ? leftS : kMostWaitS;
    S_->AwaitPreloadProgress(waitS);
  }
}

Result Engine::setView(std::string_view view) {
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
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

}
