#include "Check.h"
#include "GroundLattice.h"
#include "test/harness/render/NativeCommandsProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include "StageSubmission.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct Recorded {
  uint32_t First = 0;
  uint32_t Count = 0;
  const Buffer *Grid = nullptr;
};

struct CountingCommands final : NativeCommandsProxy {
  using NativeCommandsProxy::NativeCommandsProxy;
  std::vector<Recorded> RecordedDraws;
  bool Reject = false;

  Result<void> DrawIndexed(const IndexedDrawDescription &draw) override {
    if (Reject) {
      return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = "shadow draw rejected"});
    }
    CHECK(draw.Range.Count == GroundLattice::kIndices,
          "selected casters retain complete terrain topology");
    RecordedDraws.push_back({.First = draw.Range.FirstInstance,
                             .Count = draw.Range.Instances,
                             .Grid = draw.State.Vertices[0].Data});
    return {};
  }
};

GroundTile Tile(float east) {
  GroundTile tile;
  tile.Instance.Row[12] = east;
  tile.Instance.Corners = {{-0.25f, 0.25f, 0.25f, 0.25f, -0.25f, -0.25f, 0.25f, -0.25f}};
  tile.LowM = -0.5f;
  tile.HighM = 0.5f;
  return tile;
}

void CheckRuns(const CountingCommands &commands) {
  const std::array<uint32_t, 5> first{{0, 3, 5, 6, 8}};
  const std::array<uint32_t, 5> counts{{2, 1, 1, 1, 2}};
  CHECK(commands.RecordedDraws.size() == first.size(),
        "side-plane rejection leaves five ordered contiguous runs");
  if (commands.RecordedDraws.size() != first.size()) { return; }
  for (size_t at = 0; at < first.size(); ++at) {
    CHECK(commands.RecordedDraws[at].First == first[at] &&
              commands.RecordedDraws[at].Count == counts[at],
          "visible, boundary-intersecting and far-depth casters retain their original instances");
    CHECK((commands.RecordedDraws[at].Grid == commands.RecordedDraws[0].Grid) == (at < 3),
          "real and virtual casters retain their distinct terrain grids");
  }
}

void RoundedCentre(Device &device, GroundLattice &ground) {
  GroundTile tile;
  tile.Instance.Row = {{0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1000000, 0, 0, 1}};
  tile.Instance.Corners = {
      {-0.0001f, -0.0001f, 0.0001f, -0.0001f, -0.0001f, 0.0001f, 0.0001f, 0.0001f}};
  tile.LowM = tile.HighM = 0.02f;
  Require(
      RetryOffline([&] { return ground.SetInstances({&tile, 1}, {}); }, std::chrono::seconds(5)));
  CountingCommands commands(Require(device.Begin()));
  StageSubmission submission;
  const PassRecording into{.Commands = &commands, .Submission = submission};
  const Mat4f narrow{{1000, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -20, 0, 0, 1}};
  Require(ground.Cast(into, narrow, {{-1000000, 0, 0, 0}}, {}));
  CHECK(
      commands.RecordedDraws.size() == 1,
      "bounds retain a tiny caster when float centre rounding differs from shifted GPU placement");
}

void CastCases(Device &device, GroundLattice &ground) {
  auto real = std::array{Tile(0), Tile(0), Tile(100), Tile(0), Tile(100), Tile(1.5f)};
  const auto virtualTiles = std::array{Tile(0), Tile(100), Tile(0), Tile(0)};
  real[1].Instance.Row[14] = 1000000;
  Require(RetryOffline([&] { return ground.SetInstances(real, virtualTiles); },
                       std::chrono::seconds(5)));
  CountingCommands commands(Require(device.Begin()));
  StageSubmission submission;
  const PassRecording into{.Commands = &commands, .Submission = submission};
  const Mat4f identity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  Require(ground.Cast(into, identity, {}, {}));
  CheckRuns(commands);
  CHECK(ground.Drawn() == real.size() + virtualTiles.size(),
        "shadow selection does not overwrite camera visibility");
  commands.RecordedDraws.clear();
  auto translated = identity;
  translated[12] = -100;
  Require(ground.Cast(into, translated, {}, {}));
  CHECK(commands.RecordedDraws.size() == 3 && commands.RecordedDraws[0].First == 2 &&
            commands.RecordedDraws[1].First == 4 && commands.RecordedDraws[2].First == 7,
        "another light projection selects its own casters independently of camera selection");
  commands.RecordedDraws.clear();
  commands.Reject = true;
  const auto failed = ground.Cast(into, identity, {}, {});
  CHECK(!failed && failed.error().Message == "shadow draw rejected",
        "native recording failures propagate without publishing shadow success");
  commands.Reject = false;
  ground.SetDrawEnabled(false);
  Require(ground.Cast(into, identity, {}, {}));
  CHECK(commands.RecordedDraws.empty(), "disabled terrain submits no shadow draws");
  ground.SetDrawEnabled(true);
  for (auto &tile : real) {
    tile.Instance.Row[12] += 1000000;
    tile.Instance.Row[13] += 1000000;
  }
  auto displacedVirtual = virtualTiles;
  for (auto &tile : displacedVirtual) {
    tile.Instance.Row[12] += 1000000;
    tile.Instance.Row[13] += 1000000;
  }
  Require(RetryOffline([&] { return ground.SetInstances(real, displacedVirtual); },
                       std::chrono::seconds(5)));
  Require(ground.Cast(into, identity, {{-1000000, -1000000, 0, 0}}, {}));
  CheckRuns(commands);
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  GroundLattice ground;
  GroundPipelineBinding pipelines;
  Require(RetryOffline([&] { return ground.AttachPipelines(pipelines, *device); },
                       std::chrono::seconds(5)));
  Require(GroundLattice::ConfigureDepth(pipelines, *device));
  const std::vector<float> nodes(GroundLattice::kPageNodes);
  static_cast<void>(
      Require(RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5))));
  CastCases(*device, ground);
  RoundedCentre(*device, ground);
  return Report();
}
