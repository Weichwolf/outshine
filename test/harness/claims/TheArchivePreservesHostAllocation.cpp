#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include "Check.h"
#include "Shell.h"

namespace {
std::string Quote(std::string_view text) {
  std::string result = "'";
  for (char character : text) {
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
  const char *nest = std::getenv("OUTSHINE_NEST");
  CHECK(nest != nullptr, "runner provides temporary build storage");
  if (nest == nullptr) { return Report(); }
  const fs::path root = fs::path(nest) / "external-host";
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
  const std::string command = std::string(OUTSHINE_COMPILE) + " -Wall -Wextra -Werror -I" +
                              Quote((root / "include").string()) + " $(pkg-config --cflags sdl3) " +
                              Quote((root / "host.cpp").string());
  const std::string linkage =
      " " + Quote((root / "liboutshine.a").string()) +
      " $(pkg-config --libs sdl3 sdl3-image sdl3-ttf sdl3-shadercross) -lcurl -lz "
      "-Wl,-rpath,$(pkg-config --variable=libdir sdl3-shadercross)";
  std::string said;
  const int built =
      Run(command + linkage + " -o " + Quote((root / "host").string()) + " 2>&1", said);
  if (built != 0) { std::printf("%s", said.c_str()); }
  CHECK(built == 0, "public-only client links actual archive with its own allocator");
  if (built != 0) { return Report(); }
  said.clear();
  CHECK(Run("cd " + Quote(root.string()) + " && ./host 2>&1", said) == 0 &&
            said.find("host allocator retained") != std::string::npos,
        "engine creation and destruction preserve host allocator and new_handler outside checkout");
  said.clear();
  const int contaminated =
      Run(command + " -Isrc/base/io src/diagnostics/ProcessHeap.cpp " + linkage + " -o " +
              Quote((root / "conflicting-host").string()) + " 2>&1",
          said);
  CHECK(contaminated != 0 && (said.find("duplicate symbol") != std::string::npos ||
                              said.find("multiple definition") != std::string::npos),
        "negative control rejects reintroduced process allocator overrides");
  return Report();
}
