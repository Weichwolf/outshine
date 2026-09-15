#include "src/client/ProcessBoundary.h"
#include "Check.h"
#include <array>
#include <memory>
#include <stdexcept>
#include <string_view>

struct UnwindWitness {
  bool &Destroyed;

  ~UnwindWitness() { Destroyed = true; }
};

int main() {
  using namespace outshine::Client;
  using namespace outshine::Test;
  constexpr int ordinaryStatus = 7;
  CHECK(RunAtProcessBoundary([] { return ordinaryStatus; }) == ordinaryStatus,
        "ordinary command status survives unchanged");
  const std::array<std::string_view, 3> expected{
      "outshine-client: fatal: memory allocation failed\n",
      "outshine-client: fatal: injected failure\n",
      "outshine-client: fatal: unknown exception\n"};
  for (size_t kind = 0; kind < expected.size(); ++kind) {
    std::unique_ptr<std::FILE, decltype(&std::fclose)> stream(std::tmpfile(), &std::fclose);
    CHECK(stream != nullptr, "diagnostic stream opens");
    if (!stream) { continue; }
    bool destroyed = false;
    auto entry = [&]() -> int {
      UnwindWitness witness{destroyed};
      if (kind == 0) { throw std::bad_alloc{}; }
      if (kind == 1) { throw std::runtime_error("injected failure"); }
      throw 7;
    };
    static_assert(noexcept(RunAtProcessBoundary(entry, stream.get())));
    CHECK(RunAtProcessBoundary(entry, stream.get()) == EXIT_FAILURE,
          "unexpected exception ends command with failure");
    CHECK(destroyed, "command resources unwind before returning");
    std::rewind(stream.get());
    std::array<char, 256> text{};
    const auto count = std::fread(text.data(), 1, text.size(), stream.get());
    CHECK(!std::ferror(stream.get()), "diagnostics can be read");
    CHECK(std::string_view(text.data(), count) == expected[kind],
          "each exception category has an exact diagnostic");
  }
  return Report();
}
