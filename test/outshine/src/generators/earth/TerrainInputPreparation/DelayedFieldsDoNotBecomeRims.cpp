#include "TilePool.h"
#include "TerrainLoader.h"
#include "TerrainInputPreparation.h"
#include "TerrainSheetTask.h"
#include "Tasks.h"
#include "GroundMesher.h"
#include "SourceSet.h"
#include "DelayedTerrainSource.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kMaximumPatches = 65536;
constexpr size_t kInteriorNodes = static_cast<size_t>(kPatchGrid + 1) * (kPatchGrid + 1);
constexpr size_t kPageNodes = static_cast<size_t>(kPatchGrid + 3) * (kPatchGrid + 3);
constexpr size_t kDecodedBudget = size_t{1024} * 1024u;
constexpr int kPollAttempts = 300;
constexpr int kAdmissionPollAttempts = 3000;
constexpr size_t kHaloTurns = 100000;
constexpr size_t kHaloSliceNodes = 257;
constexpr long long kMostPosts = 100;

void CheckSheetWorker(const Generators::TerrainInputPreparation &prepared,
                      const Patchwork &unhaloed,
                      const Patchwork &expected) {
  constexpr Generators::TerrainPageLayout layout{.Side = kPatchGrid + 1, .Halo = 1};
  constexpr size_t stride = 16;
  const auto frame = TangentFrame::At({});
  Generators::TerrainSurvey survey;
  for (const auto &sheet : expected.Sheets) {
    Generators::SurveyTerrainSheet(survey, sheet, frame, layout, stride);
  }
  auto inputs = prepared;
  auto candidate = unhaloed;
  Tasks workers(1);
  Generators::TerrainSheetTask worker(
      workers,
      inputs,
      candidate,
      {.Frame = frame, .Layout = layout, .FinestZoom = 4, .ProbeStride = stride});
  CHECK(candidate.Sheets.empty() && inputs.FieldAt(expected.Sheets.front().Tile) == nullptr,
        "background work owns the pending candidate and its immutable source fields");
  Ground::HeightField::Block retained;
  CHECK(worker.SnapshotSourceFields().CopySourcedField(expected.Sheets.front().Tile, retained) &&
            !retained.Nodes.empty() && retained.Nodes.front() == DelayedTerrainSource::HeightM,
        "a camera replacement can retain source heights while the old worker owns the candidate");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  auto result = worker.Take(inputs, candidate);
  while (!result && std::chrono::steady_clock::now() < deadline) {
    (void)worker.AwaitSlice(0.01);
    result = worker.Take(inputs, candidate);
  }
  CHECK(result && *result, "the background worker returns a complete product");
  if (!result || !*result) { return; }
  CHECK(candidate.Sheets == expected.Sheets &&
            inputs.FieldAt(expected.Sheets.front().Tile) != nullptr && inputs.RimsMissing() == 0 &&
            !worker.Take(inputs, candidate),
        "completed heights and their source owners transfer back once");
  CHECK((*result)->Survey.ProbePositionsM == survey.ProbePositionsM &&
            (*result)->Survey.Vertices == survey.Vertices,
        "the worker samples lighting probes from the completed height pages");
}

void CheckSourceOwnership(const Generators::TerrainInputPreparation &sheets) {
  const std::array<Data::TileId, 1> routeTiles{{{.Zoom = 4, .X = 0, .Y = 0}}};
  Ground::HeightField::Block exact;
  Ground::HeightField::Block child;
  Ground::HeightField::Block route;
  Ground::HeightField::Block absent;
  const auto snapshot = sheets.SnapshotSourceFields();
  CHECK(sheets.ShareExactSourceField({.Zoom = 4, .X = 8, .Y = 8}, exact) &&
            !exact.Sources.empty() && exact.Raster.Side == 4 && exact.Terrain &&
            exact.Nodes.empty(),
        "the exact source keeps its native raster and provenance");
  CHECK(snapshot.CopySourcedField({.Zoom = 5, .X = 16, .Y = 16}, child) &&
            child.Sources == exact.Sources &&
            std::ranges::all_of(
                child.Nodes, [](float height) { return height == DelayedTerrainSource::HeightM; }),
        "a child height tile resamples its sourced ancestor without inventing provenance");
  CHECK(sheets.ShareExactSourceField(routeTiles.front(), route) && !route.Sources.empty() &&
            route.Terrain && route.Terrain->AtM(0u, 0u) == DelayedTerrainSource::HeightM,
        "an off-camera route tile is prepared as a sourced field");
  CHECK(!snapshot.CopySourcedField({.Zoom = 5, .X = 2, .Y = 2}, absent),
        "a tile outside sourced coverage is not qualified as fine terrain");
  Generators::TerrainInputPreparation cleared = sheets;
  cleared.Reset();
  CHECK(sheets.HeapBytes() >=
            cleared.HeapBytes() +
                static_cast<size_t>(exact.Raster.Side * exact.Raster.Side) * sizeof(float),
        "retained source rasters enter the height-sheet memory ledger");
  Generators::TerrainInputPreparation inputOwner = sheets;
  const size_t beforeMove = inputOwner.HeapBytes();
  const Generators::TerrainInputPreparation transferred = std::move(inputOwner);
  CHECK(transferred.HeapBytes() == beforeMove,
        "moving sourced fields transfers their complete memory accounting");
  inputOwner = {};
  CHECK(inputOwner.HeapBytes() == 0,
        "a reinitialized source owner counts no payload retained by its destination");
  inputOwner = sheets;
  CHECK(inputOwner.HeapBytes() == transferred.HeapBytes(),
        "copy assignment restores the same immutable field ownership and accounting");
  inputOwner.Reset();
  inputOwner = sheets;
  CHECK(inputOwner.HeapBytes() == transferred.HeapBytes(),
        "reset and reacquisition do not accumulate earlier field memory");
  inputOwner = {};
  Ground::HeightField::Block publishedChild;
  CHECK(transferred.SnapshotSourceFields().CopySourcedField({.Zoom = 5, .X = 16, .Y = 16},
                                                            publishedChild) &&
            publishedChild.Sources == child.Sources && publishedChild.Nodes == child.Nodes,
        "source preparation ownership survives transfer and preserves ancestor resampling");
  Ground::HeightField::Block pinned;
  CHECK(snapshot.CopySourcedField({.Zoom = 4, .X = 8, .Y = 8}, pinned) &&
            pinned.Sources == exact.Sources && !pinned.Nodes.empty() &&
            pinned.Nodes.front() == DelayedTerrainSource::HeightM,
        "a worker snapshot keeps immutable sourced fields after the candidate releases them");
}

void CheckHalos(Generators::TerrainInputPreparation &sheets,
                Patchwork &candidate,
                const Patchwork &original,
                const Ground::GroundStream &ground) {
  std::string error;
  CHECK(sheets.RefineByError(
            candidate, {.Side = (kPatchGrid + 1), .Halo = 1}, {}, kMaximumPatches, error),
        "resolved field refines the native candidate");
  const Patchwork unhaloed = candidate;
  Patchwork interrupted = candidate;
  Generators::TerrainInputPreparation interruptedSheets = sheets;
  Generators::TerrainInputPreparation::HaloBuildJob haloJob(interruptedSheets, interrupted, 4);
  for (size_t turn = 0;
       turn < kHaloTurns && !haloJob.Advance(turn % 3 == 2 ? kHaloSliceNodes : turn % 3 + 1u);
       ++turn) {}
  CHECK(haloJob.Haloed() == interrupted.Sheets.size(),
        "bounded halo preparation completes every eligible sheet");
  CHECK(sheets.Halos(candidate, 4) == candidate.Sheets.size() && sheets.RimsMissing() == 0,
        "neighbour fields complete every halo without missing rims");
  CHECK(interrupted.Sheets == candidate.Sheets &&
            interruptedSheets.RimsMissing() == sheets.RimsMissing(),
        "interrupted and one-shot halo preparation produce identical pages and rim counts");
  CheckSheetWorker(sheets, unhaloed, candidate);
  for (const Sheet &sheet : candidate.Sheets) {
    CHECK(sheet.Nodes.size() == kPageNodes,
          "complete height page has all interior and rim samples");
    CHECK(std::ranges::all_of(sheet.Nodes,
                              [](float height) { return height == DelayedTerrainSource::HeightM; }),
          "completed page agrees with the delivered source height");
  }
  Patchwork resident = original;
  Generators::TerrainInputPreparation residentSheets;
  residentSheets.Framed(TangentFrame::At({}));
  const auto residentReady = residentSheets.PrepareFields(
      resident, ground, {.FinestZoom = 4, .RequestsMost = 9, .AdditionalTiles = {}});
  CHECK(residentReady.has_value() && *residentReady,
        "all fields are resident for the independent one-step preparation");
  CHECK(residentSheets.RefineByError(
            resident, {.Side = (kPatchGrid + 1), .Halo = 1}, {}, kMaximumPatches, error),
        "resident source refines");
  CHECK(residentSheets.Halos(resident, 4) == resident.Sheets.size(), "resident halos complete");
  CHECK(resident.Sheets.size() == candidate.Sheets.size(),
        "paced and resident runs select the same tile count");
  if (resident.Sheets.size() == candidate.Sheets.size()) {
    for (size_t at = 0; at < candidate.Sheets.size(); ++at) {
      CHECK(resident.Sheets[at].Tile == candidate.Sheets[at].Tile &&
                resident.Sheets[at].Nodes == candidate.Sheets[at].Nodes,
            "paced and resident runs own identical height pages");
    }
  }
}

void CheckBoundary(const Ground::GroundStream &ground) {
  Patchwork boundary;
  boundary.Sheets.push_back(
      {.Tile = {.Zoom = 4, .X = 0, .Y = 0},
       .Nodes = std::vector<float>(kInteriorNodes, DelayedTerrainSource::HeightM),
       .Side = (kPatchGrid + 1),
       .Postings = (kPatchGrid + 1)});
  Generators::TerrainInputPreparation boundarySheets;
  boundarySheets.Framed(TangentFrame::At({}));
  auto boundaryReady = boundarySheets.PrepareFields(
      boundary, ground, {.FinestZoom = 4, .RequestsMost = 9, .AdditionalTiles = {}});
  const auto boundaryDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (boundaryReady && !*boundaryReady && std::chrono::steady_clock::now() < boundaryDeadline) {
    boundaryReady = boundarySheets.PrepareFields(
        boundary, ground, {.FinestZoom = 4, .RequestsMost = 9, .AdditionalTiles = {}});
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(boundaryReady.has_value() && *boundaryReady,
        "source requests wrap longitude and stop at the northern Mercator boundary");
  if (boundaryReady && *boundaryReady) {
    CHECK(boundarySheets.Halos(boundary, 4) == 1 && boundarySheets.RimsMissing() == 1,
          "a true world boundary copies only the unavailable northern rim");
  }
}

void CheckAdmission(Data::SourceSet &sources,
                    NoNetwork &transport,
                    DelayedTerrainSource *probe,
                    const Patchwork &original) {
  for (const size_t limit : {1u, 2u}) {
    probe->Released = false;
    const int before = probe->Collections;
    Ground::TilePool tightPool({.DecodedBytes = kDecodedBudget,
                                .PollAttempts = kAdmissionPollAttempts,
                                .OutstandingMost = limit,
                                .PreparedFields = {}},
                               sources,
                               transport);
    const Ground::GroundStream tightGround(tightPool, {.Z = 4, .Grid = 4});
    Generators::TerrainInputPreparation tightSheets;
    auto deferred = tightSheets.PrepareFields(
        original, tightGround, {.FinestZoom = 4, .RequestsMost = 9, .AdditionalTiles = {}});
    CHECK(deferred.has_value() && !*deferred && tightPool.Counters().AdmissionDeferred > 0,
          "deferred admission remains pending and never becomes a missing field");
    const auto startedBy = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (probe->Collections == before && std::chrono::steady_clock::now() < startedBy) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(probe->Collections > before && !probe->CalledOnCaller,
          "a dependent fetch starts on a carrier under the tight admission limit");
    probe->Released = true;
    const auto finishesBy = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (deferred && !*deferred && std::chrono::steady_clock::now() < finishesBy) {
      deferred = tightSheets.PrepareFields(
          original, tightGround, {.FinestZoom = 4, .RequestsMost = 9, .AdditionalTiles = {}});
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(deferred.has_value() && *deferred,
          "a released dependency lets the field complete even at one admitted slot");
    CHECK(tightPool.Counters().FieldDropped == 0 && tightPool.Counters().Posts < kMostPosts,
          "dependency admission completes without repeated field jobs or a posting storm");
  }
}

void CheckShutdown(Data::SourceSet &sources,
                   NoNetwork &transport,
                   DelayedTerrainSource *probe,
                   const Patchwork &original) {
  probe->Released = false;
  {
    Ground::TilePool stoppingPool({.DecodedBytes = kDecodedBudget,
                                   .PollAttempts = kPollAttempts,
                                   .OutstandingMost = 1,
                                   .PreparedFields = {}},
                                  sources,
                                  transport);
    const Ground::GroundStream stoppingGround(stoppingPool, {.Z = 4, .Grid = 4});
    Generators::TerrainInputPreparation stoppingSheets;
    const auto waiting = stoppingSheets.PrepareFields(
        original, stoppingGround, {.FinestZoom = 4, .RequestsMost = 1, .AdditionalTiles = {}});
    CHECK(waiting.has_value() && !*waiting, "a field is pending before pool shutdown");
    const auto parkedBy = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (stoppingPool.Counters().ParkedJobs == 0 && std::chrono::steady_clock::now() < parkedBy) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(stoppingPool.Counters().ParkedJobs == 1,
          "the dependent field is parked while its fetch is withheld");
  }
  probe->Released = true;
}

void CheckTerminalSources(const Patchwork &original) {
  for (const DelayedTerrainSource::Answer answer :
       {DelayedTerrainSource::Answer::Absent, DelayedTerrainSource::Answer::Refused}) {
    Data::RawSourceCache otherStore({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
    Data::SourceSet otherSources(otherStore);
    NoNetwork otherTransport;
    auto otherSource = std::make_unique<DelayedTerrainSource>();
    otherSource->Response = answer;
    otherSource->Released = true;
    CHECK(otherSources.Add(std::move(otherSource)) == Data::SourceSet::Registration::Accepted,
          "terminal source registered");
    Ground::TilePool otherPool(
        {.DecodedBytes = kDecodedBudget, .PollAttempts = kPollAttempts, .PreparedFields = {}},
        otherSources,
        otherTransport);
    const Ground::GroundStream otherGround(otherPool, {.Z = 4, .Grid = 4});
    Patchwork other = original;
    Generators::TerrainInputPreparation otherSheets;
    otherSheets.Framed(TangentFrame::At({}));
    auto settled = otherSheets.PrepareFields(
        other, otherGround, {.FinestZoom = 4, .RequestsMost = 9, .AdditionalTiles = {}});
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (settled && !*settled && std::chrono::steady_clock::now() < until) {
      settled = otherSheets.PrepareFields(
          other, otherGround, {.FinestZoom = 4, .RequestsMost = 9, .AdditionalTiles = {}});
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (answer == DelayedTerrainSource::Answer::Absent) {
      CHECK(settled.has_value() && *settled, "terminal absence finishes field preparation");
      if (settled && *settled) {
        CHECK(otherSheets.Halos(other, 4) == 1 && otherSheets.RimsMissing() == 1,
              "terminal absence alone may copy an interior edge into its missing rim");
      }
    } else {
      CHECK(!settled.has_value() && !settled.error().empty(),
            "source refusal aborts the private candidate with a concrete error");
    }
    CHECK(other.Sheets.front().Nodes.size() == kInteriorNodes ||
              answer == DelayedTerrainSource::Answer::Absent,
          "failure leaves the original candidate geometry untouched");
  }
}

void CheckSourceReuse(const Generators::TerrainInputPreparation &prepared,
                      Patchwork requested,
                      Ground::TilePool &pool,
                      const Ground::GroundStream &ground) {
  const auto shared = requested.Sheets.front().Tile;
  const Data::TileId departed{.Zoom = 4, .X = 7, .Y = 8};
  const Data::TileId entered{.Zoom = 4, .X = 10, .Y = 8};
  auto prior = prepared.SnapshotSourceFields();
  ++requested.Sheets.front().Tile.X;
  const Generators::TerrainInputPreparation::FieldPreparation budget{
      .FinestZoom = 4, .RequestsMost = 0, .AdditionalTiles = {}};
  Generators::TerrainInputPreparation next;
  const auto pending = next.PrepareFields(
      requested, ground, budget, std::make_unique<Generators::TerrainFieldSet>(prior));
  const auto *const field = next.FieldAt(shared);
  CHECK(pending && !*pending && field && field->AtM(0u, 0u) == DelayedTerrainSource::HeightM &&
            next.FieldAt(departed) == nullptr && next.FieldAt(entered) == nullptr,
        "moving the camera retains overlapping source heights while the new edge awaits its DEM");

  pool.Shapes({.Kind = "sineRidge", .AmplitudeM = 5.0, .WavelengthM = 1000.0});
  Generators::TerrainInputPreparation reshaped;
  const auto replaced = reshaped.PrepareFields(
      requested, ground, budget, std::make_unique<Generators::TerrainFieldSet>(std::move(prior)));
  CHECK(replaced && !*replaced && reshaped.FieldAt(shared) == nullptr,
        "a changed terrain declaration requests new heights for a previously overlapping tile");
}

}

int main() {
  Data::RawSourceCache store({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  Data::SourceSet sources(store);
  NoNetwork transport;
  auto source = std::make_unique<DelayedTerrainSource>();
  auto *probe = source.get();
  CHECK(sources.Add(std::move(source)) == Data::SourceSet::Registration::Accepted,
        "delayed source registered");
  Ground::TilePool pool(
      {.DecodedBytes = kDecodedBudget, .PollAttempts = kPollAttempts, .PreparedFields = {}},
      sources,
      transport);
  const Ground::GroundStream ground(pool, {.Z = 4, .Grid = 4});
  Patchwork candidate;
  candidate.Sheets.push_back(
      {.Tile = {.Zoom = 4, .X = 8, .Y = 8},
       .Nodes = std::vector<float>(kInteriorNodes, DelayedTerrainSource::HeightM),
       .Side = (kPatchGrid + 1),
       .Postings = (kPatchGrid + 1)});
  const Patchwork original = candidate;
  Generators::TerrainInputPreparation invalid;
  const std::array<Data::TileId, 1> wrongZoom{{{.Zoom = 5, .X = 0, .Y = 0}}};
  CHECK(!invalid.PrepareFields(
            candidate, ground, {.FinestZoom = 4, .RequestsMost = 1, .AdditionalTiles = wrongZoom}),
        "an additional tile from another source zoom cannot enter the candidate");
  const std::vector<Data::TileId> tooMany(257, {.Zoom = 4, .X = 0, .Y = 0});
  CHECK(!invalid.PrepareFields(
            candidate, ground, {.FinestZoom = 4, .RequestsMost = 1, .AdditionalTiles = tooMany}),
        "additional route fields have an explicit candidate budget");
  Generators::TerrainInputPreparation sheets;
  sheets.Framed(TangentFrame::At({}));
  const std::array<Data::TileId, 1> routeTiles{{{.Zoom = 4, .X = 0, .Y = 0}}};
  auto prepared = sheets.PrepareFields(
      candidate, ground, {.FinestZoom = 4, .RequestsMost = 9, .AdditionalTiles = routeTiles});
  CHECK(prepared.has_value() && !*prepared && !probe->CalledOnCaller,
        "unreleased source leaves a complete candidate pending without caller-side collection");
  CHECK(candidate.Sheets.front().Nodes.size() == kInteriorNodes,
        "pending fields do not finalize missing height nodes");
  Generators::TerrainInputPreparation abandoned = sheets;
  abandoned.Reset();
  const auto restarted = abandoned.PrepareFields(
      original, ground, {.FinestZoom = 4, .RequestsMost = 1, .AdditionalTiles = {}});
  CHECK(restarted.has_value() && !*restarted,
        "cancelled field preparation starts again without publishing incomplete data");
  probe->Released = true;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (prepared && !*prepared && std::chrono::steady_clock::now() < deadline) {
    prepared = sheets.PrepareFields(
        candidate, ground, {.FinestZoom = 4, .RequestsMost = 9, .AdditionalTiles = routeTiles});
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(prepared.has_value() && *prepared && !probe->CalledOnCaller,
        "every source tile and halo neighbour resolves without blocking the caller");
  if (!prepared || !*prepared) { return Report(); }
  CheckSourceOwnership(sheets);
  CheckHalos(sheets, candidate, original, ground);
  CheckBoundary(ground);
  CheckAdmission(sources, transport, probe, original);
  CheckShutdown(sources, transport, probe, original);
  CheckTerminalSources(original);
  CheckSourceReuse(sheets, original, pool, ground);
  return Report();
}
