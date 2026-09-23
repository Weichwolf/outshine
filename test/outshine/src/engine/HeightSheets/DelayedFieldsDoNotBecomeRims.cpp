#include "TerrainLoader.h"
#include "HeightSheets.h"
#include "GroundLattice.h"
#include "SourceSet.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace {
class DelayedSource final : public outshine::Data::Source {
public:
  enum class Answer { Height, Absent, Refused };
  outshine::Data::SourceDecl Decl{
      .Id = "delayed", .Revision = "r1", .Keeps = outshine::Data::Cacheability::Never};
  std::thread::id Caller = std::this_thread::get_id();
  mutable std::atomic<bool> CalledOnCaller{false};
  mutable std::atomic<int> Collections{0};
  std::atomic<bool> Released{false};
  Answer Response = Answer::Height;

  const outshine::Data::SourceDecl &Declaration() const noexcept override { return Decl; }

  outshine::Data::Coverage Covers(const outshine::Data::Fetch &) const noexcept override {
    return outshine::Data::Coverage::Inside;
  }

  outshine::Data::Address Serves(const outshine::Data::Fetch &request) const noexcept override {
    return request.Where();
  }

  outshine::Data::Ticket Begin(const outshine::Data::Address &,
                               outshine::Data::Transport &) const override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::Fetched Collect(const outshine::Data::Address &,
                                  outshine::Data::Ticket,
                                  outshine::Data::Transport &) const override {
    ++Collections;
    if (std::this_thread::get_id() == Caller) { CalledOnCaller = true; }
    if (!Released) { return outshine::Data::Fetched::Working(); }
    if (Response == Answer::Absent) {
      return outshine::Data::Fetched::Meant(outshine::Data::Meaning::Absent);
    }
    if (Response == Answer::Refused) {
      return outshine::Data::Fetched::Meant(outshine::Data::Meaning::Refused);
    }
    return outshine::Data::Fetched::Delivered(
        {137, 80,  78,  71,  13,  10, 26,  10,  0,  0,   0,  13,  73,  72, 68, 82, 0,   0,  0,
         4,   0,   0,   0,   4,   8,  2,   0,   0,  0,   38, 147, 9,   41, 0,  0,  0,   16, 73,
         68,  65,  84,  120, 156, 99, 104, 208, 98, 128, 35, 6,   226, 56, 0,  28, 131, 10, 161,
         92,  158, 195, 228, 0,   0,  0,   0,   73, 69,  78, 68,  174, 66, 96, 130});
  }
};

class NoNetwork final : public outshine::Data::Transport {
public:
  outshine::Data::Ticket Begin(const std::string &) override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Never();
  }

  void Cancel(outshine::Data::Ticket) override {}
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
  Data::SourceSet sources(store);
  NoNetwork transport;
  auto source = std::make_unique<DelayedSource>();
  auto *probe = source.get();
  CHECK(sources.Add(std::move(source)) == Data::SourceSet::Registration::Accepted,
        "delayed source registered");
  Ground::TilePool pool({.DecodedBytes = 1u << 20u, .PollAttempts = 300}, sources, transport);
  Ground::GroundStream ground(pool, {.Z = 4, .Grid = 4});
  Patchwork candidate;
  candidate.Sheets.push_back({.Tile = {.Zoom = 4, .X = 8, .Y = 8},
                              .Nodes = std::vector<float>(Render::GroundLattice::kNodes, 42.0f),
                              .Side = Render::GroundLattice::kSide,
                              .Postings = Render::GroundLattice::kSide});
  const Patchwork original = candidate;
  HeightSheets sheets;
  sheets.Framed(TangentFrame::At({}));
  auto prepared = sheets.PrepareFields(candidate, ground, {.FinestZoom = 4, .RequestsMost = 9});
  CHECK(prepared.has_value() && !*prepared && !probe->CalledOnCaller,
        "unreleased source leaves a complete candidate pending without caller-side collection");
  CHECK(candidate.Sheets.front().Nodes.size() == Render::GroundLattice::kNodes,
        "pending fields do not finalize missing height nodes");
  HeightSheets abandoned = sheets;
  abandoned.ForgetsFields();
  const auto restarted =
      abandoned.PrepareFields(original, ground, {.FinestZoom = 4, .RequestsMost = 1});
  CHECK(restarted.has_value() && !*restarted,
        "cancelled field preparation starts again without publishing incomplete data");
  probe->Released = true;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (prepared && !*prepared && std::chrono::steady_clock::now() < deadline) {
    prepared = sheets.PrepareFields(candidate, ground, {.FinestZoom = 4, .RequestsMost = 9});
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(prepared.has_value() && *prepared && !probe->CalledOnCaller,
        "every source tile and halo neighbour resolves without blocking the caller");
  if (!prepared || !*prepared) { return Report(); }
  std::string error;
  CHECK(sheets.RefineByError(candidate,
                             {.Side = Render::GroundLattice::kSide, .Halo = 1},
                             {},
                             Render::GroundLattice::kPages,
                             error),
        "resolved field refines the native candidate");
  Patchwork interrupted = candidate;
  HeightSheets interruptedSheets = sheets;
  HeightSheets::HaloBuildJob haloJob(interruptedSheets, interrupted, 4);
  for (size_t turn = 0; turn < 100000 && !haloJob.Advance(turn % 3 == 2 ? 257u : turn % 3 + 1u);
       ++turn) {}
  CHECK(haloJob.Haloed() == interrupted.Sheets.size(),
        "bounded halo preparation completes every eligible sheet");
  CHECK(sheets.Halos(candidate, 4) == candidate.Sheets.size() && sheets.RimsMissing() == 0,
        "neighbour fields complete every halo without missing rims");
  CHECK(interrupted.Sheets == candidate.Sheets &&
            interruptedSheets.RimsMissing() == sheets.RimsMissing(),
        "interrupted and one-shot halo preparation produce identical pages and rim counts");
  for (const Sheet &sheet : candidate.Sheets) {
    CHECK(sheet.Nodes.size() == Render::GroundLattice::kPageNodes,
          "complete height page has all interior and rim samples");
    CHECK(std::ranges::all_of(sheet.Nodes, [](float height) { return height == 42.0f; }),
          "completed page agrees with the delivered source height");
  }
  Patchwork resident = original;
  HeightSheets residentSheets;
  residentSheets.Framed(TangentFrame::At({}));
  const auto residentReady =
      residentSheets.PrepareFields(resident, ground, {.FinestZoom = 4, .RequestsMost = 9});
  CHECK(residentReady.has_value() && *residentReady,
        "all fields are resident for the independent one-step preparation");
  CHECK(residentSheets.RefineByError(resident,
                                     {.Side = Render::GroundLattice::kSide, .Halo = 1},
                                     {},
                                     Render::GroundLattice::kPages,
                                     error),
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
  Patchwork boundary;
  boundary.Sheets.push_back({.Tile = {.Zoom = 4, .X = 0, .Y = 0},
                             .Nodes = std::vector<float>(Render::GroundLattice::kNodes, 42.0f),
                             .Side = Render::GroundLattice::kSide,
                             .Postings = Render::GroundLattice::kSide});
  HeightSheets boundarySheets;
  boundarySheets.Framed(TangentFrame::At({}));
  auto boundaryReady =
      boundarySheets.PrepareFields(boundary, ground, {.FinestZoom = 4, .RequestsMost = 9});
  const auto boundaryDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (boundaryReady && !*boundaryReady && std::chrono::steady_clock::now() < boundaryDeadline) {
    boundaryReady =
        boundarySheets.PrepareFields(boundary, ground, {.FinestZoom = 4, .RequestsMost = 9});
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(boundaryReady.has_value() && *boundaryReady,
        "source requests wrap longitude and stop at the northern Mercator boundary");
  if (boundaryReady && *boundaryReady) {
    CHECK(boundarySheets.Halos(boundary, 4) == 1 && boundarySheets.RimsMissing() == 1,
          "a true world boundary copies only the unavailable northern rim");
  }
  for (size_t limit : {1u, 2u}) {
    probe->Released = false;
    const int before = probe->Collections;
    Ground::TilePool tightPool(
        {.DecodedBytes = 1u << 20u, .PollAttempts = 3000, .OutstandingMost = limit},
        sources,
        transport);
    Ground::GroundStream tightGround(tightPool, {.Z = 4, .Grid = 4});
    HeightSheets tightSheets;
    auto deferred =
        tightSheets.PrepareFields(original, tightGround, {.FinestZoom = 4, .RequestsMost = 9});
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
      deferred =
          tightSheets.PrepareFields(original, tightGround, {.FinestZoom = 4, .RequestsMost = 9});
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(deferred.has_value() && *deferred,
          "a released dependency lets the field complete even at one admitted slot");
    CHECK(tightPool.Counters().FieldDropped == 0 && tightPool.Counters().Posts < 100,
          "dependency admission completes without repeated field jobs or a posting storm");
  }
  probe->Released = false;
  {
    Ground::TilePool stoppingPool(
        {.DecodedBytes = 1u << 20u, .PollAttempts = 300, .OutstandingMost = 1}, sources, transport);
    Ground::GroundStream stoppingGround(stoppingPool, {.Z = 4, .Grid = 4});
    HeightSheets stoppingSheets;
    const auto waiting = stoppingSheets.PrepareFields(
        original, stoppingGround, {.FinestZoom = 4, .RequestsMost = 1});
    CHECK(waiting.has_value() && !*waiting, "a field is pending before pool shutdown");
    const auto parkedBy = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (stoppingPool.Counters().ParkedJobs == 0 && std::chrono::steady_clock::now() < parkedBy) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(stoppingPool.Counters().ParkedJobs == 1,
          "the dependent field is parked while its fetch is withheld");
  }
  probe->Released = true;
  for (DelayedSource::Answer answer :
       {DelayedSource::Answer::Absent, DelayedSource::Answer::Refused}) {
    Data::ContentStore otherStore({.Using = Data::ContentStore::Use::Off});
    Data::SourceSet otherSources(otherStore);
    NoNetwork otherTransport;
    auto otherSource = std::make_unique<DelayedSource>();
    otherSource->Response = answer;
    otherSource->Released = true;
    CHECK(otherSources.Add(std::move(otherSource)) == Data::SourceSet::Registration::Accepted,
          "terminal source registered");
    Ground::TilePool otherPool(
        {.DecodedBytes = 1u << 20u, .PollAttempts = 300}, otherSources, otherTransport);
    Ground::GroundStream otherGround(otherPool, {.Z = 4, .Grid = 4});
    Patchwork other = original;
    HeightSheets otherSheets;
    otherSheets.Framed(TangentFrame::At({}));
    auto settled =
        otherSheets.PrepareFields(other, otherGround, {.FinestZoom = 4, .RequestsMost = 9});
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (settled && !*settled && std::chrono::steady_clock::now() < until) {
      settled = otherSheets.PrepareFields(other, otherGround, {.FinestZoom = 4, .RequestsMost = 9});
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (answer == DelayedSource::Answer::Absent) {
      CHECK(settled.has_value() && *settled, "terminal absence finishes field preparation");
      if (settled && *settled) {
        CHECK(otherSheets.Halos(other, 4) == 1 && otherSheets.RimsMissing() == 1,
              "terminal absence alone may copy an interior edge into its missing rim");
      }
    } else {
      CHECK(!settled.has_value() && !settled.error().empty(),
            "source refusal aborts the private candidate with a concrete error");
    }
    CHECK(other.Sheets.front().Nodes.size() == Render::GroundLattice::kNodes ||
              answer == DelayedSource::Answer::Absent,
          "failure leaves the original candidate geometry untouched");
  }
  return Report();
}
