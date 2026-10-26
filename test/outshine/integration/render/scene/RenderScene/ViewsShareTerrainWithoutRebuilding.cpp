#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeTerrainRendererProbe.h"
#include "terrain/TerrainResidency.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace {
constexpr auto kHiddenShiftM = 1000;

using namespace outshine::Test::GroundProbe;

class ResourceProbe final : public NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label = {}) override {
    ++Buffers;
    return Backing().CreateBuffer(description, label);
  }

  Result<std::unique_ptr<Texture>> CreateTexture(TextureDescription description,
                                                 std::string_view label = {}) override {
    ++Textures;
    return Backing().CreateTexture(description, label);
  }

  Result<std::span<std::byte>> MapWrite(Buffer &buffer, ByteRange range) override {
    ++Writes;
    return Backing().MapWrite(buffer, range);
  }

  uint32_t Buffers = 0;
  uint32_t Textures = 0;
  uint32_t Writes = 0;
};

void SelectView(Device &device, TerrainRenderer &view, const FrameContext &frame) {
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  Require(view.Cull(frame, {}, *commands));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "each view completes its own terrain selection");
}

void Configure(Device &device, TerrainRenderer &view, TerrainPipelines &pipelines) {
  std::array<ColourTarget, kTargetFormats.size()> targets{};
  for (size_t i = 0; i < targets.size(); ++i) { targets[i].Texels = kTargetFormats[i]; }
  Require(RetryOffline(
      [&] {
        return view.Configure(
            pipelines,
            device,
            {.NormalIndex = 1, .IdentityIndex = 2, .BaseIndex = 3, .MetalRoughIndex = 4},
            targets);
      },
      std::chrono::seconds(5)));
}
}

int main() {
  using namespace outshine::Test::GroundProbe;
  auto backing = Require(CreateDevice({.Validation = true}));
  ResourceProbe device{*backing};
  outshine::Render::TerrainResidency resident;
  TerrainPipelines pipelines;
  TerrainRenderer first{resident};
  Configure(device, first, pipelines);
  HeightPattern pattern;
  for (size_t i = 0; i < pattern.Fractions.size(); ++i) {
    pattern.Fractions[i] = static_cast<float>(i) / (pattern.Fractions.size() - 1);
  }
  pattern.Nodes.fill(0.5f);
  const auto page = Require(
      RetryOffline([&] { return resident.PlacePage(pattern.Nodes); }, std::chrono::seconds(5)));
  pattern.Tiles = {Tile(page, {kLeft, -kInner}), Tile(page, {kInner, -kLeft})};
  Require(RetryOffline([&] { return resident.SetInstances(pattern.Tiles, {}); },
                       std::chrono::seconds(5)));
  const auto buffers = device.Buffers;
  const auto textures = device.Textures;
  const auto writes = device.Writes;
  const auto held = resident.HeldBytes();
  const auto generation = resident.Generation();
  TerrainRenderer second{resident};
  Require(second.AttachPipelines(pipelines, device));
  CHECK(device.Buffers == buffers && device.Textures == textures && device.Writes == writes,
        "a second view neither allocates terrain resources nor uploads their data");
  CHECK(resident.HeldBytes() == held && resident.Generation() == generation,
        "attaching a view preserves the existing canonical terrain");

  Lit(device, first, "", &pattern, true);
  FrameContext hidden;
  hidden.Mvp = kIdentity;
  hidden.Mvp[12] = kHiddenShiftM;
  SelectView(device, second, hidden);
  CHECK(first.Drawn() == 2 && second.Drawn() == 0,
        "one view cannot overwrite another view's selected instances");
  Lit(device, second, "", &pattern, true);
  first.SetDrawEnabled(false);
  CHECK(first.Drawn() == 0 && second.Drawn() == 2,
        "draw policy belongs to the view rather than the shared terrain");
  first.SetDrawEnabled(true);
  CHECK(resident.HeldBytes() == held,
        "per-view patch selection does not grow canonical terrain storage");

  Require(RetryOffline([&] { return resident.SetInstances(pattern.Tiles, {}); },
                       std::chrono::seconds(5)));
  CHECK(first.SelectedPatches() == nullptr && second.SelectedPatches() == nullptr,
        "a canonical mutation invalidates both old GPU patch selections");
  StageSubmission submission;
  const auto stale = second.Encode({.Submission = submission, .Area = {}, .Clip = {}}, {});
  CHECK(!stale && stale.error().Kind == ErrorKind::Busy,
        "old visibility cannot draw a changed canonical instance buffer");
  Lit(device, first, "", &pattern, true);
  CHECK(second.SelectedPatches() == nullptr,
        "refreshing one view does not validate its stale sibling");
  Lit(device, second, "", &pattern, true);
  CHECK(first.Drawn() == 2 && second.Drawn() == 2 && resident.PagesStanding() == 1,
        "both refreshed views draw the same surviving height page");
  return Report();
}
