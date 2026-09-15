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
  CHECK(empty && empty->Verb == "help" && empty->Directory == "src/assets/places" &&
            empty->Arguments.empty(),
        "empty input selects help");
  const std::array<const char *, 1> program{"client"};
  const auto help = ReadCommandLine(program);
  CHECK(help && help->Verb == "help", "program alone selects help");
  const std::array<const char *, 6> args{
      "client", "--places", "custom/places", "shots", "--all", "--audit"};
  const auto command = ReadCommandLine(args);
  CHECK(command && command->Directory == "custom/places" && command->Verb == "shots",
        "override and command parse");
  if (command) {
    CHECK(command->Directory.data() == args[2] && command->Verb.data() == args[3],
          "text views borrow process strings");
    CHECK(command->Arguments.data() == args.data() + 4 && command->Arguments.size() == 2,
          "rest arguments borrow original pointer table");
  }
  CHECK(!ReadCommandLine(std::span(args).first(2)) && !ReadCommandLine(std::span(args).first(3)),
        "incomplete override is rejected");
  const std::array<const char *, 3> nullArg{"client", "help", nullptr};
  CHECK(!ReadCommandLine(nullArg), "null trailing argument is rejected");
  const std::array<const char *, 3> plain{"client", "render", "asset.glb"};
  const auto render = ReadCommandLine(plain);
  CHECK(render && render->Verb == "render" && render->Arguments.size() == 1 &&
            render->Arguments[0] == plain[2],
        "ordinary command preserves its argument");
  return Report();
}
