#include "src/client/CommandLine.h"
#include "Check.h"
#include <array>
#include <type_traits>

static_assert(noexcept(outshine::Client::ReadCommandLine({})));
static_assert(std::is_trivially_copyable_v<outshine::Client::CommandLine>);

int main() {
  using namespace outshine::Client;
  using namespace outshine::Test;
  const auto empty = ReadCommandLine({});
  CHECK(empty && empty->Verb == "help" && empty->Arguments.empty(), "empty input selects help");
  const std::array<const char *, 1> program{"client"};
  const auto help = ReadCommandLine(program);
  CHECK(help && help->Verb == "help", "program alone selects help");
  const std::array<const char *, 6> args{
      "client", "benchmark", "--scenario", "world.scenario", "--frames", "60"};
  const auto command = ReadCommandLine(args);
  CHECK(command && command->Verb == "benchmark", "command is selected without content knowledge");
  if (command) {
    CHECK(command->Verb.data() == args[1], "command borrows its process string");
    CHECK(command->Arguments.data() == args.data() + 2 && command->Arguments.size() == 4,
          "command arguments borrow the original pointer table");
  }
  const std::array<const char *, 3> nullArg{"client", "help", nullptr};
  CHECK(!ReadCommandLine(nullArg), "null trailing argument is rejected");
  const std::array<const char *, 3> plain{"client", "run", "asset.scenario"};
  const auto render = ReadCommandLine(plain);
  CHECK(render && render->Verb == "run" && render->Arguments.size() == 1 &&
            render->Arguments[0] == plain[2],
        "ordinary command preserves its argument");
  return Report();
}
