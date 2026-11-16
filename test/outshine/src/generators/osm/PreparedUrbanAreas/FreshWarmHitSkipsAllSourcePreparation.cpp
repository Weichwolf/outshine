#include "UrbanAssetFixture.h"
#include "Check.h"
#include <ios>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <sys/wait.h>
#include <unistd.h>

namespace {
int FreshProcess(const std::filesystem::path &directory) {
  using namespace UrbanAssetFixture;
  using namespace outshine::Test;
  const Fixture fixture(directory, Availability::Unavailable);
  if (!fixture.Urban) { return Report(); }
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  const auto result = Await(fixture, request);
  CHECK(result && result->Status == PreparedUrbanAreas::State::Ready && result->Product,
        "a new OS process receives the complete native asset with unavailable source input");
  if (!result || !result->Product) { return Report(); }
  CHECK(CompleteArea(*result->Product),
        "the fresh-process cache hit supplies usable geometry and source provenance");
  std::ifstream keyInput(directory / "expected.key");
  std::string expectedKey;
  keyInput >> expectedKey;
  CHECK(!expectedKey.empty() && result->AssetKey == expectedKey,
        "fresh OS process delivers the persisted content key without recooking");
  CHECK(fixture.Calls == 0, "the fresh process does not acquire unavailable sources");
  return Report();
}

void CheckFreshProcess(const char *program, const std::filesystem::path &directory) {
  using namespace outshine::Test;
  std::error_code error;
  const auto executable = std::filesystem::canonical(program, error);
  CHECK(!error && std::filesystem::is_regular_file(executable),
        "fresh-process executable resolves");
  if (error || !std::filesystem::is_regular_file(executable)) { return; }
  const auto child = fork();
  CHECK(child >= 0, "fresh-process check starts without carrying fixture threads");
  if (child == 0) {
    auto *log = std::fopen((directory / "child.log").c_str(), "w");
    if (log == nullptr || dup2(fileno(log), STDOUT_FILENO) < 0 ||
        dup2(fileno(log), STDERR_FILENO) < 0) {
      constexpr int redirectFailure = 126;
      _exit(redirectFailure);
    }
    execl(executable.c_str(), executable.c_str(), directory.c_str(), static_cast<char *>(nullptr));
    constexpr int execFailure = 127;
    _exit(execFailure);
  }
  if (child > 0) {
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "independent fresh-process native hit completes successfully");
  }
}
}

int main(int argc, char *const *argv) {
  using namespace UrbanAssetFixture;
  using namespace outshine::Test;
  if (argc == 2) { return FreshProcess(argv[1]); }
  if (argc != 1) { return 2; }
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-urban-cache-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  std::string originalKey;
  {
    const Fixture fixture(directory, Availability::Ready);
    if (!fixture.Urban) { return Report(); }
    CHECK(fixture.Urban->AwaitProgress(1),
          "without an admitted job the caller must keep advancing");
    const auto result = Await(fixture, request);
    CHECK(result && result->Status == PreparedUrbanAreas::State::Ready && result->Product,
          "miss obtains known owner and parent products then publishes a complete urban asset");
    if (!result || !result->Product) { return Report(); }
    originalKey = result->AssetKey;
    CHECK(originalKey.size() == 64 &&
              originalKey.find_first_not_of("0123456789abcdef") == std::string::npos,
          "cold generation exposes the persisted native content identity");
    const auto &area = *result->Product;
    CHECK(area.HasUrbanEvidence && area.Occupied > 0 && area.Inputs.size() == 2,
          "owner and coarse evidence both retain their provenance and nonempty native content");
    CHECK(area.Inputs[0].Identity.Tile == kOwner && area.Inputs[1].Identity.Tile.Zoom == 5,
          "intrinsic ownership and parent source zoom remain separate");
    CHECK(fixture.Calls == 2, "exactly the required owner and parent sources are acquired");
  }
  {
    const Fixture fixture(directory, Availability::Unavailable);
    if (!fixture.Urban) { return Report(); }
    const auto result = Await(fixture, request);
    CHECK(result && result->Status == PreparedUrbanAreas::State::Ready && result->Product,
          "fresh producer loads the finished asset with unavailable source bytes");
    if (!result || !result->Product) { return Report(); }
    CHECK(result->AssetKey == originalKey,
          "warm hit delivers the same content key as cold publication");
    CHECK(CompleteArea(*result->Product),
          "the warm native asset retains usable geometry and source provenance");
    CHECK(fixture.Calls == 0, "the warm producer does not acquire unavailable sources");
  }
  {
    std::ofstream keyOutput(directory / "expected.key");
    keyOutput << originalKey;
    CHECK(keyOutput.good(), "content identity is available to the independent child");
  }
  CheckFreshProcess(argv[0], directory);
  std::error_code error;
  if (Failures.Value() == 0) { std::filesystem::remove_all(directory, error); }
  CHECK(!error, "isolated producer artifacts are removed");
  return Report();
}
