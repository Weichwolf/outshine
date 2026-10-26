#include "src/app/ProcessBoundary.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
struct UnwindWitness {
  bool &Destroyed;

  ~UnwindWitness() { Destroyed = true; }
};

int CheckBoundaries() {
  using namespace outshine::App;
  using namespace outshine::Test;
  constexpr int ordinaryStatus = 7;
  CHECK(RunAtProcessBoundary([] { return ordinaryStatus; }) == ordinaryStatus,
        "ordinary command status survives unchanged");
  const std::array<std::string_view, 3> expected{"outshine: fatal: memory allocation failed\n",
                                                 "outshine: fatal: injected failure\n",
                                                 "outshine: fatal: unknown exception\n"};
  for (size_t kind = 0; kind < expected.size(); ++kind) {
    const std::unique_ptr<std::FILE, decltype(&std::fclose)> stream(std::tmpfile(), &std::fclose);
    CHECK(stream != nullptr, "diagnostic stream opens");
    if (!stream) { continue; }
    bool destroyed = false;
    const auto entry = [&] -> int {
      const UnwindWitness witness{destroyed};
      if (kind == 0) { throw std::bad_alloc{}; }
      if (kind == 1) { throw std::runtime_error("injected failure"); }
      throw 7; // NOLINT(bugprone-std-exception-baseclass): verify the required non-standard
               // catch-all.
    };
    static_assert(noexcept(RunAtProcessBoundary(entry, stream.get())));
    CHECK(RunAtProcessBoundary(entry, stream.get()) == EXIT_FAILURE,
          "unexpected exception ends command with failure");
    CHECK(destroyed, "command resources unwind before returning");
    CHECK(std::fseek(stream.get(), 0, SEEK_SET) == 0, "diagnostic stream rewinds successfully");
    std::array<char, 256> text{};
    const auto count = std::fread(text.data(), 1, text.size(), stream.get());
    CHECK(!std::ferror(stream.get()), "diagnostics can be read");
    CHECK(std::string_view(text.data(), count) == expected[kind],
          "each exception category has an exact diagnostic");
  }
  return Report();
}
}

int main() {
  return outshine::App::RunAtProcessBoundary(CheckBoundaries);
}
