#include "io/Log.h"
#include "Check.h"
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

namespace {
using namespace outshine;

struct RecordingSink final : LogSink {
  std::vector<std::string> Units;

  void Write(double, LogLevel, Saying who, std::span<const LogField>) override {
    Units.emplace_back(who.Unit != nullptr ? who.Unit : "");
  }
};

void Say() {
  Log::Info(LogTag::World, "scope-test");
}

void Nested(RecordingSink &sink) {
  const LogThreadSinkScope routing(&sink);
  const LogUnitScope unit("inner");
  Say();
  return;
}
}

int main() {
  using namespace outshine::Test;
  static_assert(!std::is_copy_constructible_v<LogUnitScope>);
  static_assert(!std::is_move_constructible_v<LogUnitScope>);
  static_assert(!std::is_copy_constructible_v<LogThreadSinkScope>);
  static_assert(!std::is_move_constructible_v<LogThreadSinkScope>);
  RecordingSink fallback, outer, inner;
  Log::SetLevel(LogLevel::Info);
  {
    const LogThreadSinkScope fallbackRoute(&fallback);
    const LogThreadSinkScope routing(&outer);
    std::string label = "outer";
    const LogUnitScope unit(label);
    label.assign("changed");
    Say();
    Nested(inner);
    Say();
    {
      const LogThreadSinkScope useFallback(&fallback);
      const LogUnitScope longLabel(std::string(64, 'x'));
      Say();
    }
    Say();
  }
  {
    const LogThreadSinkScope fallbackRoute(&fallback);
    Say();
  }
  CHECK(outer.Units == std::vector<std::string>({"outer", "outer", "outer"}),
        "nested scopes restore owned outer unit and borrowed sink");
  CHECK(inner.Units == std::vector<std::string>({"inner"}), "inner context is isolated");
  CHECK(fallback.Units == std::vector<std::string>({std::string(31, 'x'), ""}),
        "explicit routes receive bounded unit labels");
  {
    const LogThreadSinkScope route(&fallback);
    const char label[] = {'v', 'i', 'e', 'w', 'x'};
    const LogUnitScope unit(std::string_view(label, 4));
    Say();
  }
  {
    const LogThreadSinkScope silence(nullptr);
    Say();
  }
  CHECK(fallback.Units.back() == "view", "label copy respects a non-terminated view extent");
  RecordingSink first, second;
  std::thread a([&] {
    const LogThreadSinkScope routing(&first);
    const LogUnitScope unit("first");
    Say();
  });
  std::thread b([&] {
    const LogThreadSinkScope routing(&second);
    const LogUnitScope unit("second");
    Say();
  });
  a.join();
  b.join();
  CHECK(first.Units == std::vector<std::string>({"first"}) &&
            second.Units == std::vector<std::string>({"second"}),
        "thread-local sinks work independently without a global sink");
  Say();
  return Report();
}
