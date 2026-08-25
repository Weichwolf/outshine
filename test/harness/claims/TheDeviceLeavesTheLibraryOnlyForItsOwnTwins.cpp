#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Slurp(const std::filesystem::path &path) {
  std::string out;
  std::FILE *const file = std::fopen(path.string().c_str(), "rb");
  if (file == nullptr) { return out; }
  char block[8192];
  size_t read = 0;
  while ((read = std::fread(block, 1, sizeof block, file)) > 0) { out.append(block, read); }
  std::fclose(file);
  return out;
}

constexpr const char *kDeviceWork[] = {
    "SDL_ClaimWindowForGPUDevice", "SDL_WaitAndAcquireGPUSwapchainTexture",
    "SDL_ReleaseWindowFromGPUDevice", "SDL_AcquireGPUCommandBuffer", "SDL_CreateGPUTexture",
};

[[nodiscard]] bool ProvesTheDevice(const std::string &path) {
  return path.find("test/outshine/shader/") != std::string::npos ||
         path.find("test/harness/") != std::string::npos;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::vector<std::string> holding;
  size_t walked = 0;
  for (const char *root : {"src", "include", "apps", "test"}) {
    for (const auto &entry : Sources(root)) {
      const std::string suffix = entry.extension().string();
      if (suffix != ".cpp" && suffix != ".h") { continue; }
      const std::string path = entry.string();
      if (path.rfind("src/render/", 0) == 0) { continue; }
      if (ProvesTheDevice(path)) { continue; }
      ++walked;
      const std::string text = Slurp(entry);
      for (const char *verb : kDeviceWork) {
        const std::string called = std::string(verb) + "(";
        for (size_t at = text.find(called); at != std::string::npos;
             at = text.find(called, at + 1)) {
          const size_t opens = text.rfind('\n', at);
          const size_t closes = text.find('\n', at);
          const std::string line =
              text.substr(opens == std::string::npos ? 0 : opens + 1,
                          closes == std::string::npos ? std::string::npos : closes - opens - 1);
          if (line.find('"') != std::string::npos) { continue; }
          size_t number = 1;
          for (size_t scan = 0; scan < at; ++scan) { number += text[scan] == '\n' ? 1 : 0; }
          holding.push_back(path + ":" + std::to_string(number) + " calls " + verb);
        }
      }
    }
  }

  Note("sources walked outside src/render/", (double)walked, "files");
  for (const std::string &one : holding) { std::printf("FOUND %s\n", one.c_str()); }

  CHECK(walked > 200, "the walk saw the tree, not a corner of it");
  CHECK(holding.empty(),
        "**A CLIENT DECLARES A SURFACE AND THE LIBRARY RENDERS INTO IT**: no source outside "
        "src/render/ claims a window for the device, acquires a swapchain texture or a command "
        "buffer, or makes a GPU texture -- because a client that does those things is written "
        "against SDL_GPU whether it wants to be or not, and the renderer it holds is then not "
        "swappable. The exception is the MSL-versus-C++ twins, which exist to prove the device "
        "and are not clients of it (board:1826)");

  Covers("IV.21 the device does not leave the render layer: a client hands in a window or an "
         "extent and gets back pixels, never a handle it must do GPU work with (board:1826)");
  return Report();
}
