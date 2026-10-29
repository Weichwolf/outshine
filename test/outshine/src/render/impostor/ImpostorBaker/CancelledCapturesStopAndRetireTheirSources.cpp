#include "ImpostorBaker.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

ImpostorCapture Source() {
  Geometry mesh;
  CHECK(mesh.addSurface("opaque", Material{}).has_value(), "opaque capture material exists");
  const auto part = mesh.addPart("plane", MaterialInstance(0));
  CHECK(part.has_value(), "the source plane exists");
  if (part) {
    CHECK(mesh.setPositions(*part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}) &&
              mesh.setNormals(*part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
              mesh.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
          "the capture source owns complete geometry");
  }
  ImpostorCapture result;
  result.LeastM = {{-1, -1, -.1}};
  result.MostM = {{1, 1, .1}};
  result.ViewDirections = {{{0, 0, 1}}, {{0, 0, -1}}};
  result.Sources.push_back({.Mesh = std::move(mesh), .Instances = {Mat4{}}});
  return result;
}

enum class StopAt { Before, Device, Submit, Wait, Never };

struct Hooks {
  StopAt At;
  std::stop_source Stop;
  size_t Devices = 0, Submissions = 0, Waits = 0;

  void StopIf(StopAt at) {
    if (At == at) { Stop.request_stop(); }
  }

  GpuSubmission Submission() {
    return {
        .Context = this,
        .Create =
            [](void *context, Native::DeviceOptions options) {
              auto &self = *static_cast<Hooks *>(context);
              ++self.Devices;
              self.StopIf(StopAt::Device);
              return Native::CreateDevice(options);
            },
        .Submit =
            [](void *context, Native::Device *device, std::unique_ptr<Native::Commands> commands) {
              auto &self = *static_cast<Hooks *>(context);
              ++self.Submissions;
              auto result = device->Submit(std::move(commands));
              self.StopIf(StopAt::Submit);
              return result;
            },
        .WaitFence =
            [](void *context,
               Native::Device *device,
               Native::Completion completion,
               std::chrono::nanoseconds budget) {
              auto &self = *static_cast<Hooks *>(context);
              ++self.Waits;
              auto result = device->Wait(completion, budget);
              self.StopIf(StopAt::Wait);
              return result;
            }};
  }
};

std::optional<std::vector<uint8_t>> Encode(const std::optional<ImpostorAtlas> &atlas) {
  if (!atlas) { return std::nullopt; }
  std::string error;
  return atlas->Encode("cancellation-reference", error);
}

void Cancel(StopAt at, const std::vector<uint8_t> &reference) {
  Hooks hooks{.At = at, .Stop = {}};
  hooks.StopIf(StopAt::Before);
  ImpostorBaker baker(hooks.Submission());
  std::string error;
  size_t fullSubmissions = 0;
  size_t fullWaits = 0;
  if (at == StopAt::Submit || at == StopAt::Wait) {
    hooks.At = StopAt::Never;
    CHECK(Encode(baker.Capture(Source(), {.Pixels = 16, .Views = 2}, error)) == reference,
          "submission and wait controls start after actual device initialization");
    const auto submissions = hooks.Submissions;
    const auto waits = hooks.Waits;
    CHECK(Encode(baker.Capture(Source(), {.Pixels = 16, .Views = 2}, error)) == reference,
          "the same warmed source establishes actual submission and wait costs");
    fullSubmissions = hooks.Submissions - submissions;
    fullWaits = hooks.Waits - waits;
    hooks.At = at;
  }
  const auto submissions = hooks.Submissions;
  const auto waits = hooks.Waits;
  const auto result =
      baker.Capture(Source(), {.Pixels = 16, .Views = 2}, error, hooks.Stop.get_token());
  CHECK(hooks.Stop.stop_requested() && !result && error == "impostor capture cancelled",
        "a deterministic phase stop refuses the complete atlas");
  CHECK(hooks.Devices == (at == StopAt::Before ? size_t{0} : size_t{1}),
        "pre-admission cancellation creates no device; admitted work owns exactly one");
  if (at == StopAt::Submit) {
    CHECK(hooks.Submissions - submissions < fullSubmissions,
          "a submitted-source stop avoids the remaining native submissions");
  }
  if (at == StopAt::Wait) {
    CHECK(hooks.Waits - waits < fullWaits,
          "a completed-wait stop avoids waiting for the remaining views");
  }
  hooks.At = StopAt::Never;
  CHECK(Encode(baker.Capture(Source(), {.Pixels = 16, .Views = 2}, error)) == reference &&
            hooks.Devices == 1,
        "the same native device remains usable with no cancelled source or material residue");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  std::string error;
  const auto reference = Encode(ImpostorBaker::Bake(Source(), {.Pixels = 16, .Views = 2}, error));
  CHECK(reference.has_value(), "a fresh uncancelled capture is an independent reference");
  if (reference) {
    for (const auto at : {StopAt::Before, StopAt::Device, StopAt::Submit, StopAt::Wait}) {
      Cancel(at, *reference);
    }
  }
  std::stop_source stop;
  stop.request_stop();
  CHECK(!ImpostorBaker::Bake(Source(), {.Pixels = 16, .Views = 2}, error, stop.get_token()) &&
            error == "impostor capture cancelled",
        "the one-shot producer forwards cancellation instead of returning a product");
  SDL_Quit();
  return Report();
}
