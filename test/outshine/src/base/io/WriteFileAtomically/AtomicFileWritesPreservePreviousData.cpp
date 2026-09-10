#include "io/WriteFileAtomically.h"
#include "io/ReadTextFile.h"
#include "Check.h"
#include "Outshine.h"
#include <array>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <span>
#include <thread>
#include <sys/resource.h>
#include <unistd.h>

namespace {
auto WriteText(std::string_view path, std::string_view text) {
  return outshine::WriteFileAtomically(path, std::as_bytes(std::span(text)));
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-write-XXXXXX").string();
  if (mkdtemp(directory.data()) == nullptr) {
    CHECK(false, "temporary directory created");
    return Report();
  }
  const std::string path = directory + "/state";
  const std::string before = "previous complete save";
  CHECK(WriteText(path, before).has_value(), "initial complete file published");
  Scenario::Document scene;
  scene.Room = 4;
  Scenario::Kind kind;
  kind.Name = "actor";
  kind.Attributes = {{"health", "23"}};
  scene.Kinds.push_back(kind);
  scene.Instances.push_back({.Of = "actor", .Id = "kept"});
  scene.State.push_back({.What = "kept.health"});
  Engine engine;
  const std::string savePath = directory + "/engine-save";
  const bool ready = engine.declare(scene).has_value() && engine.assemble().has_value() &&
                     engine.save(savePath).has_value();
  CHECK(ready, "public API creates a complete save");
  const auto previousSave = ReadTextFile(savePath, 4096);
  struct rlimit saved{};
  const bool queried = getrlimit(RLIMIT_FSIZE, &saved) == 0;
  CHECK(queried, "file size limit can be inspected");
  if (queried) {
    const auto previousSignal = std::signal(SIGXFSZ, SIG_IGN);
    auto limited = saved;
    limited.rlim_cur = 1;
    const bool changed = setrlimit(RLIMIT_FSIZE, &limited) == 0;
    CHECK(changed, "isolated test process applies file size limit");
    if (changed) {
      for (const size_t size : {size_t{128}, size_t{65536}}) {
        const auto failed = WriteText(path, std::string(size, 'x'));
        CHECK(!failed, "write or buffered close failure is reported");
        if (!failed) { std::printf("injected IO failure: %s\n", failed.error().c_str()); }
        const auto kept = ReadTextFile(path, 1024);
        CHECK(kept && *kept == before, "IO failure preserves the complete previous file");
      }
      if (ready) {
        CHECK(!engine.save(savePath), "public save propagates the publication failure");
        const auto preserved = ReadTextFile(savePath, 4096);
        CHECK(previousSave && preserved && *preserved == *previousSave,
              "failed public save preserves the previous complete snapshot bytes");
      }
      CHECK(setrlimit(RLIMIT_FSIZE, &saved) == 0, "file size limit restored");
    }
    std::signal(SIGXFSZ, previousSignal);
  }
  const auto refused = WriteText(directory, "cannot replace a directory");
  CHECK(!refused && std::filesystem::is_directory(directory),
        "rename failure preserves target directory");
  CHECK(!WriteText(path + std::string("\0suffix", 7), "bad"), "NUL path rejected");
  std::array<std::string, 4> products;
  std::array<std::thread, 4> writers;
  std::array<bool, 4> success{};
  std::atomic<size_t> active{writers.size()};
  for (size_t i = 0; i < writers.size(); ++i) {
    products[i] = std::string(65536, static_cast<char>('a' + i));
  }
  for (size_t i = 0; i < writers.size(); ++i) {
    writers[i] = std::thread([&, i] {
      success[i] = true;
      for (size_t count = 0; count < 8; ++count) {
        if (!WriteText(path, products[i])) { success[i] = false; }
      }
      --active;
    });
  }
  bool complete = true;
  for (size_t read = 0; read < 10000 && active.load() != 0; ++read) {
    const auto seen = ReadTextFile(path, 65536);
    bool recognized = seen && *seen == before;
    for (const auto &product : products) { recognized = recognized || (seen && *seen == product); }
    complete = complete && recognized;
  }
  for (auto &writer : writers) { writer.join(); }
  CHECK(complete && success[0] && success[1] && success[2] && success[3],
        "concurrent replacements expose only complete products");
  size_t remaining = 0;
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    (void)entry;
    ++remaining;
  }
  CHECK(remaining == 2, "success and failure leave no owned temporary files");
  std::filesystem::remove_all(directory);
  return Report();
}
