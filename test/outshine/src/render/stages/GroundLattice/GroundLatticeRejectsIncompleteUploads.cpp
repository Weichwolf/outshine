#include "GroundLattice.h"
#include "NativeCommandsProxy.h"
#include "NativeDeviceProxy.h"
#include "NativeGpuCheck.h"
#include "NativeOffline.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
enum class Failure { None, Buffer, Transfer, Map, Acquire, Copy, Submit };
Failure Next = Failure::None;
constexpr std::string_view kCause = "injected ground upload failure";
constexpr size_t kMany = 65;

bool Reject(Failure point) {
  if (Next != point) { return false; }
  Next = Failure::None;
  return true;
}

Error Refused() {
  return {.Kind = ErrorKind::Driver, .Message = std::string(kCause)};
}

struct FaultCommands final : NativeCommandsProxy {
  using NativeCommandsProxy::Copy;
  using NativeCommandsProxy::NativeCommandsProxy;

  Result<void> Copy(const Buffer &source, Buffer &destination, BufferCopy region) override {
    if (Reject(Failure::Copy)) { return std::unexpected(Refused()); }
    return NativeCommandsProxy::Copy(source, destination, region);
  }

  Result<void> Copy(const Buffer &source, Texture &destination, TextureTransfer region) override {
    if (Reject(Failure::Copy)) { return std::unexpected(Refused()); }
    return NativeCommandsProxy::Copy(source, destination, region);
  }
};

struct FaultDevice final : NativeDeviceProxy {
  using NativeDeviceProxy::NativeDeviceProxy;

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label = {}) override {
    if (Reject(description.Placement == Memory::Upload ? Failure::Transfer : Failure::Buffer)) {
      return std::unexpected(Refused());
    }
    return NativeDeviceProxy::CreateBuffer(description, label);
  }

  Result<std::span<std::byte>> MapWrite(Buffer &buffer, ByteRange range) override {
    if (Reject(Failure::Map)) { return std::unexpected(Refused()); }
    return NativeDeviceProxy::MapWrite(buffer, range);
  }

  Result<std::unique_ptr<Commands>> Begin() override {
    if (Reject(Failure::Acquire)) { return std::unexpected(Refused()); }
    auto commands = NativeDeviceProxy::Begin();
    if (!commands) { return std::unexpected(commands.error()); }
    return std::make_unique<FaultCommands>(std::move(*commands));
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    auto actual = static_cast<FaultCommands &>(*commands).Take();
    if (Reject(Failure::Submit)) { return std::unexpected(Refused()); }
    return NativeDeviceProxy::Submit(std::move(actual));
  }
};

GroundTile Tile(float east) {
  GroundTile tile;
  tile.Instance.Row[12] = east;
  tile.Instance.Corners = {{-1, -1, 1, -1, -1, 1, 1, 1}};
  tile.LowM = -1;
  tile.HighM = 1;
  return tile;
}

void Pages(FaultDevice &device, GroundLattice &ground) {
  std::vector<float> nodes(GroundLattice::kPageNodes);
  CHECK(Require(RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5))) ==
            0,
        "first height page is placed");
  for (const Failure failure :
       {Failure::Transfer, Failure::Map, Failure::Acquire, Failure::Copy, Failure::Submit}) {
    Next = failure;
    const auto rejected =
        RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5));
    CHECK(!rejected && rejected.error().Message == kCause,
          "rejected page upload reports its native cause");
    CHECK(Next == Failure::None, "the requested page failure is reached");
    CHECK(ground.PagesStanding() == 1, "failed uploads publish no page");
    CHECK(Require(RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5))) ==
              1,
          "rejected page upload returns its reservation for retry");
    ground.ReleasePage(1);
  }
  ground.ReleasePage(1);
  CHECK(ground.PagesStanding() == 1, "releasing a page twice does not corrupt live accounting");
  CHECK(!ground.PlacePage(std::span(nodes).first(nodes.size() - 1)),
        "incomplete native height pages remain invalid");
  auto other = Require(CreateDevice({.Validation = true}));
  GroundPipelineBinding pipelines;
  CHECK(!ground.AttachPipelines(pipelines, *other),
        "ground resources cannot silently move between native devices");
  static_cast<void>(device);
}

void Instances(GroundLattice &ground) {
  const std::array one{Tile(0)};
  std::array<GroundTile, kMany> many{};
  for (size_t at = 0; at < many.size(); ++at) { many[at] = Tile(static_cast<float>(at)); }
  Require(RetryOffline([&] { return ground.SetInstances(one, {}); }, std::chrono::seconds(5)));
  const auto baseline = ground.Triangles();
  for (const Failure failure : {Failure::Buffer,
                                Failure::Transfer,
                                Failure::Map,
                                Failure::Acquire,
                                Failure::Copy,
                                Failure::Submit}) {
    Next = failure;
    const auto rejected =
        RetryOffline([&] { return ground.SetInstances(many, {}); }, std::chrono::seconds(5));
    CHECK(!rejected && rejected.error().Message == kCause,
          "rejected tile upload reports its native cause");
    CHECK(Next == Failure::None, "the requested tile failure is reached");
    CHECK(ground.Triangles() == baseline,
          "rejected tile upload preserves the published tile count");
  }
  Require(RetryOffline([&] { return ground.SetInstances(many, {}); }, std::chrono::seconds(5)));
  CHECK(ground.Triangles() == many.size() * GroundLattice::kIndices / 3u,
        "successful tile upload publishes every tile");
}
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  FaultDevice device(*actual);
  GroundLattice ground;
  GroundPipelineBinding pipelines;
  Require(RetryOffline([&] { return ground.AttachPipelines(pipelines, device); },
                       std::chrono::seconds(5)));
  Pages(device, ground);
  Instances(ground);
  return Report();
}
