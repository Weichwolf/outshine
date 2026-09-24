#include <Logging.h>
#include <Outshine.h>
#include <string>
#include <vector>
#include "Check.h"

namespace {

class RecordingSink final : public outshine::LogSink {
public:
  void Write(double,
             outshine::LogLevel,
             Saying who,
             std::span<const outshine::LogField>) noexcept override {
    Events.emplace_back(who.Event == nullptr ? "" : who.Event);
  }

  std::vector<std::string> Events;
};

bool RefusesWithoutVideo(outshine::Engine &engine) {
  return !engine.setRenderTarget({32, 32});
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  RecordingSink first;
  RecordingSink second;
  Engine left;
  Engine right;
  left.logsTo(&first);
  right.logsTo(&second);
  CHECK(RefusesWithoutVideo(left), "the first engine reaches its renderer diagnostic boundary");
  CHECK(first.Events == std::vector<std::string>{"no_video"} && second.Events.empty(),
        "the first engine emits only to its registered sink");
  CHECK(RefusesWithoutVideo(right), "the second engine reaches its renderer diagnostic boundary");
  CHECK(first.Events == std::vector<std::string>{"no_video"} &&
            second.Events == std::vector<std::string>{"no_video"},
        "separate engines keep their diagnostic routes isolated");
  left.logsTo(nullptr);
  CHECK(RefusesWithoutVideo(left), "a detached engine still reports its owned operation failure");
  CHECK(first.Events.size() == 1 && second.Events.size() == 1,
        "a detached sink receives no later callback");
  CHECK(RefusesWithoutVideo(right),
        "the retained route remains usable after another engine detaches");
  CHECK(second.Events.size() == 2, "the second engine retains its own route");
  return Report();
}
