#include <Logging.h>
#include <Engine.h>
#include "TestEngine.h"
#include "Log.h"
#include "Device.h"
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include "Check.h"

namespace {

class RecordingSink final : public outshine::LogSink {
public:
  void Write([[maybe_unused]] double simTimeS,
             [[maybe_unused]] outshine::LogLevel level,
             Saying who,
             [[maybe_unused]] std::span<const outshine::LogField> fields) noexcept override {
    Events.emplace_back(who.Event == nullptr ? "" : who.Event);
  }

  std::vector<std::string> Events;
};

struct DeviceFailure {
  unsigned Calls = 0;
};

outshine::Render::Native::Result<std::unique_ptr<outshine::Render::Native::Device>>
RefuseDevice(void *context, [[maybe_unused]] outshine::Render::Native::DeviceOptions options) {
  ++static_cast<DeviceFailure *>(context)->Calls;
  outshine::Log::Error(outshine::LogTag::Render, "device_fixture_refused");
  return std::unexpected(outshine::Render::Native::Error{
      .Kind = outshine::Render::Native::ErrorKind::Driver, .Message = "injected device failure"});
}

bool RefusesInjectedDevice(outshine::Engine &engine) {
  return !engine.setRenderTarget({.WidthPx = 32, .HeightPx = 32});
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  RecordingSink first;
  RecordingSink second;
  DeviceFailure firstDevice;
  DeviceFailure secondDevice;
  auto left = TestEngine::WithGpu({.Context = &firstDevice, .Create = RefuseDevice});
  auto right = TestEngine::WithGpu({.Context = &secondDevice, .Create = RefuseDevice});
  left.logsTo(&first);
  right.logsTo(&second);
  CHECK(RefusesInjectedDevice(left), "the first engine reaches its renderer diagnostic boundary");
  CHECK(first.Events == std::vector<std::string>{"device_fixture_refused"} && second.Events.empty(),
        "the first engine emits only to its registered sink");
  CHECK(RefusesInjectedDevice(right), "the second engine reaches its renderer diagnostic boundary");
  CHECK(first.Events == std::vector<std::string>{"device_fixture_refused"} &&
            second.Events == std::vector<std::string>{"device_fixture_refused"},
        "separate engines keep their diagnostic routes isolated");
  left.logsTo(nullptr);
  CHECK(RefusesInjectedDevice(left), "a detached engine still reports its owned operation failure");
  CHECK(first.Events.size() == 1 && second.Events.size() == 1,
        "a detached sink receives no later callback");
  CHECK(RefusesInjectedDevice(right),
        "the retained route remains usable after another engine detaches");
  CHECK(second.Events.size() == 2, "the second engine retains its own route");
  CHECK(firstDevice.Calls == 2 && secondDevice.Calls == 2,
        "each public engine operation reaches its own injected device factory");
  return Report();
}
