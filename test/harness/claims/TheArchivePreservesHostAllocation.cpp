#include <algorithm>
#include <filesystem>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include "Check.h"
#include "Shell.h"

namespace {
std::string Quote(std::string_view text) {
  std::string result = "'";
  for (const char character : text) {
    if (character == '\'') {
      result += "'\\''";
    } else {
      result += character;
    }
  }
  return result + "'";
}
}

int main() {
  using namespace outshine::Test;
  namespace fs = std::filesystem;
  const fs::path root = PlantedPath("external-host");
  CHECK(root.is_absolute(), "the external host has absolute temporary build storage");
  if (!root.is_absolute()) { return Report(); }
  std::error_code error;
  fs::create_directories(root, error);
  CHECK(!error, "external build directory created");
  fs::copy("include",
           root / "include",
           fs::copy_options::recursive | fs::copy_options::overwrite_existing,
           error);
  CHECK(!error, "public headers staged without internal sources");
  fs::copy_file(
      "build/liboutshine.a", root / "liboutshine.a", fs::copy_options::overwrite_existing, error);
  CHECK(!error, "built library staged");
  fs::copy_file("test/harness/shared/graph/HostAllocator.cpp",
                root / "host.cpp",
                fs::copy_options::overwrite_existing,
                error);
  CHECK(!error, "external host source staged");
  if (error) { return Report(); }
  std::string nativeLink;
  const int nativeFlags = Run("bash test/run.sh --native-link-flags", nativeLink);
  CHECK(nativeFlags == 0 && !nativeLink.empty(), "native platform linkage is available");
  if (nativeFlags != 0 || nativeLink.empty()) { return Report(); }
  std::ranges::replace(nativeLink, '\n', ' ');
  const std::string command = std::string(OUTSHINE_COMPILE) + " -Wall -Wextra -Werror -I" +
                              Quote((root / "include").string()) + " $(pkg-config --cflags sdl3) " +
                              Quote((root / "host.cpp").string());
  const std::string linkage = " " + Quote((root / "liboutshine.a").string()) +
                              " $(pkg-config --libs sdl3 sdl3-image sdl3-ttf "
                              "liblz4 libtiff-4 libwebp geos sqlite3 libzstd sfcgal) -lcurl -lz " +
                              nativeLink;
  std::string said;
  const int built =
      Run(command + linkage + " -o " + Quote((root / "host").string()) + " 2>&1", said);
  if (built != 0) { std::print("{}", said); }
  CHECK(built == 0, "public-only client links actual archive with its own allocator");
  if (built != 0) { return Report(); }
  said.clear();
  CHECK(Run("cd " + Quote(root.string()) + " && ./host 2>&1", said) == 0 &&
            said.contains("host allocator retained"),
        "engine creation and destruction preserve host allocator and new_handler outside checkout");
  said.clear();
  const int contaminated =
      Run(command + " -Isrc/base/io src/diagnostics/ProcessHeap.cpp " + linkage + " -o " +
              Quote((root / "conflicting-host").string()) + " 2>&1",
          said);
  CHECK(contaminated != 0 &&
            (said.contains("duplicate symbol") || said.contains("multiple definition")),
        "negative control rejects reintroduced process allocator overrides");
  return Report();
}
