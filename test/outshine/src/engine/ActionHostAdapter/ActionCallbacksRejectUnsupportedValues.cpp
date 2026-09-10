#include <array>
#include <string>
#include "ActionHostAdapter.h"
#include "Check.h"

namespace {
class Receiver final : public outshine::Host {
public:
  size_t Calls = 0;
  size_t Count = 0;
  double Number = 0;
  std::string Text;
  bool Accept = true;

  bool calls(std::string_view name, std::span<const outshine::Argument> args) override {
    ++Calls;
    Count = args.size();
    if (name != "record") { return false; }
    for (const auto &arg : args) {
      if (arg.Is == outshine::Argument::Kind::Number) {
        Number = arg.Number;
      } else {
        Text = arg.Text;
      }
    }
    return Accept;
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Receiver receiver;
  ActionHostAdapter adapter(&receiver);
  const auto record = adapter.Global("record");
  Script::Value result = Script::Value::OfNumber(99);
  const std::array valid{Script::Value::OfNumber(7.5), Script::Value::OfText("copied text")};
  CHECK(adapter.Call(record, valid, result), "number and text reach the receiver");
  CHECK(receiver.Count == 2 && receiver.Number == 7.5 && receiver.Text == "copied text",
        "argument payload and order survive the callback boundary");
  CHECK(result.What == Script::Kind::Nothing, "action does not invent a script return value");
  for (const auto &bad : {Script::Value{}, Script::Value::OfRef(1)}) {
    const std::array args{valid[0], bad};
    const size_t before = receiver.Calls;
    CHECK(!adapter.Call(record, args, result), "unsupported value is rejected instead of zero");
    CHECK(receiver.Calls == before, "validation completes before calling application code");
  }
  std::array<Script::Value, Script::kMaxArgs + 1> args;
  args.fill(Script::Value::OfNumber(3));
  CHECK(!adapter.Call(record, args, result), "excess arguments cannot cross the boundary");
  CHECK(adapter.Call(record, std::span(args).first(Script::kMaxArgs), result),
        "the complete argument budget is usable");
  CHECK(receiver.Count == Script::kMaxArgs, "bounded storage does not truncate arguments");
  receiver.Accept = false;
  CHECK(!adapter.Call(record, {}, result), "receiver rejection propagates");
  ActionHostAdapter absent(nullptr);
  CHECK(!absent.Call(absent.Global("record"), {}, result) && !absent.Fired(),
        "no receiver means no callback");
  for (size_t i = 1; i < Script::kMaxNames; ++i) {
    CHECK(adapter.Global(std::to_string(i)).What == Script::Kind::Ref,
          "every permitted name gets a reference");
  }
  CHECK(adapter.Global("excess").What == Script::Kind::Nothing, "name accumulation is bounded");
  CHECK(adapter.Global("record").Ref == record.Ref, "existing names survive a full table");
  CHECK(
      !adapter.Call(Script::Value::OfRef(0), {}, result) &&
          !adapter.Call(Script::Value::OfRef(-1), {}, result) &&
          !adapter.Call(Script::Value::OfRef(static_cast<int>(Script::kMaxNames) + 1), {}, result),
      "invalid callees are rejected");
  Script::Program script;
  std::string error;
  receiver.Accept = true;
  ActionHostAdapter scripted(&receiver);
  CHECK(script.Read("record(12, 'script text');", error) && script.Run(scripted, error),
        "script execution uses the same typed callback boundary");
  CHECK(receiver.Number == 12 && receiver.Text == "script text", "script arguments are preserved");
  return Report();
}
