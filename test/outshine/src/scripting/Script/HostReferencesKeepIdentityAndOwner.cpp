#include "Script.h"
#include "ScriptHostFixture.h"
#include "Check.h"
#include <string>
#include <thread>
#include <utility>

int main() {
  using namespace outshine::Script;
  using namespace outshine::Test;
  ScriptHostFixture host;
  Program program;
  std::string error;
  CHECK(program.Read("object.value += 2; capture(object===object, capture===object.record); "
                     "capture.call(null, object.value); scratch=5; capture(scratch);",
                     error),
        "native references and ordinary global writes compile");
  CHECK(program.Run(host, error), error.c_str());
  CHECK(host.Calls.size() == 3, "host methods and Function.prototype.call execute");
  if (host.Calls.size() != 3) { return Report(); }
  CHECK(host.Calls[0][0].Truth() && host.Calls[0][1].Truth() && host.Calls[1][0].Number == 42 &&
            host.Calls[2][0].Number == 5,
        "references retain identity, members update and globals remain ordinary JavaScript");
  Program moved = std::move(program);
  CHECK(moved.Held(), "moving transfers the compiled runtime");
  bool foreignRan = true;
  std::thread foreign([&] {
    std::string threadError;
    foreignRan = moved.Run(host, threadError);
  });
  foreign.join();
  CHECK(!foreignRan && moved.Held(), "a foreign thread cannot enter or invalidate the runtime");
  ScriptHostFixture other;
  CHECK(!moved.Run(other, error) && other.Calls.empty(),
        "a new host cannot reinterpret held references");
  CHECK(moved.Read("capture('new')", error) && moved.Run(other, error),
        "a new compilation explicitly replaces the host binding");
  other.Accept = false;
  CHECK(!moved.Run(other, error) && !moved.Held(), "host refusal invalidates further execution");
  CHECK(!moved.Read("let = ;", error) && !moved.Held(), "syntax rejection leaves no old program");
  return Report();
}
