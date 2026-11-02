#include "SpirvFile.h"
#include "Check.h"
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_process.h>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
constexpr std::array<uint32_t, 5> kWords{0x07230203, 0x00010000, 0, 1, 0};
constexpr std::string_view kShader = "calibrationFma.comp.spv";

bool ChildPassed(const std::string &executable, const char *mode) {
  const std::array arguments{executable.c_str(), mode, static_cast<const char *>(nullptr)};
  const std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process(
      SDL_CreateProcess(arguments.data(), false), &SDL_DestroyProcess);
  int status = -1;
  return process && SDL_WaitProcess(process.get(), true, &status) && status == 0;
}
}

int main(int argc, char *const *argv) {
  using namespace outshine::Render;
  using namespace outshine::Test;
  if (argc == 2) {
    const auto loaded = ReadShaderSpirv(kShader);
    if (std::string_view(argv[1]) == "present") {
      return loaded && *loaded == std::vector<uint32_t>(kWords.begin(), kWords.end()) ? 0 : 1;
    }
    return !loaded ? 0 : 1;
  }
  const char *const base = SDL_GetBasePath();
  CHECK(base != nullptr, "executable directory is available");
  if (base == nullptr) { return Report(); }
  const auto original = ReadShaderSpirv(kShader);
  CHECK(original && *original != std::vector<uint32_t>(kWords.begin(), kWords.end()),
        "original shader package exists with different words for the fallback counterexample");
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-shader-package-XXXXXX").string();
  if (mkdtemp(directory.data()) == nullptr) {
    CHECK(false, "temporary application package created");
    return Report();
  }
  const auto source = std::filesystem::path(base) / std::filesystem::path(argv[0]).filename();
  const auto executable = std::filesystem::path(directory) / "relocated";
  std::error_code error;
  std::filesystem::copy_file(source, executable, error);
  CHECK(!error, "relocated executable copied without changing the original package");
  if (!error) {
    const auto shaders = std::filesystem::path(directory) / "shaders";
    std::filesystem::create_directory(shaders, error);
    CHECK(!error, "relocated shader directory created");
    {
      std::ofstream file(shaders / kShader, std::ios::binary);
      file.write(reinterpret_cast<const char *>(kWords.data()),
                 static_cast<std::streamsize>(sizeof(kWords)));
      CHECK(file.good(), "distinct local shader bytes written");
    }
    CHECK(ChildPassed(executable.string(), "present"),
          "relocated process reads its own exact shader words from another working directory");
    std::filesystem::remove(shaders / kShader, error);
    CHECK(!error, "local shader removed while the original package remains available");
    CHECK(ChildPassed(executable.string(), "absent"),
          "missing local shader never falls back to the original build package");
  }
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "temporary application package removed");
  return Report();
}
