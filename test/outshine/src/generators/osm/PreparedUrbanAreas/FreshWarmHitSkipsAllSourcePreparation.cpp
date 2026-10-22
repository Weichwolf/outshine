#include "UrbanAssetFixture.h"
#include "UrbanAreaCodec.h"
#include "Check.h"
#include <cstdint>
#include <ios>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

namespace {
int FreshProcess(const std::filesystem::path &directory) {
  using namespace UrbanAssetFixture;
  using namespace outshine::Test;
  Fixture fixture(directory, Availability::Unavailable);
  if (!fixture.Urban) { return Report(); }
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  const auto result = Await(fixture, request);
  CHECK(result && result->Status == PreparedUrbanAreas::State::Ready && result->Product,
        "a new OS process receives the complete native asset with unavailable source input");
  if (!result || !result->Product) { return Report(); }
  std::ifstream in(directory / "expected.bin", std::ios::binary);
  const std::vector<uint8_t> expected{std::istreambuf_iterator<char>(in), {}};
  const auto bytes = EncodeUrbanArea(*result->Product);
  CHECK(bytes && !expected.empty() && *bytes == expected,
        "fresh process preserves every encoded field and provenance byte");
  const auto costs = fixture.Urban->Costs();
  CHECK(costs.Hits == 1 && costs.Generations == 0 && costs.SourcePolls == 0 && fixture.Calls == 0 &&
            fixture.Tiles->Costs().Hits == 0,
        "fresh OS process never invokes the provider, source service or urban factory");
  return Report();
}
}

int main(int argc, char **argv) {
  using namespace UrbanAssetFixture;
  using namespace outshine::Test;
  if (argc == 2) { return FreshProcess(argv[1]); }
  if (argc != 1) { return 2; }
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-urban-cache-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  std::vector<uint8_t> original;
  {
    Fixture fixture(directory, Availability::Ready);
    if (!fixture.Urban) { return Report(); }
    const auto result = Await(fixture, request);
    CHECK(result && result->Status == PreparedUrbanAreas::State::Ready && result->Product,
          "miss obtains known owner and parent products then publishes a complete urban asset");
    if (!result || !result->Product) { return Report(); }
    const auto &area = *result->Product;
    CHECK(area.HasUrbanEvidence && area.Occupied > 0 && area.Inputs.size() == 2,
          "owner and coarse evidence both retain their provenance and nonempty native content");
    CHECK(area.Inputs[0].Identity.Tile == kOwner && area.Inputs[1].Identity.Tile.Zoom == 5,
          "intrinsic ownership and parent source zoom remain separate");
    const auto bytes = EncodeUrbanArea(area);
    CHECK(bytes, "complete result encodes without losing provenance");
    if (bytes) { original = *bytes; }
    const auto costs = fixture.Urban->Costs();
    CHECK(costs.Misses == 1 && costs.Generations == 1 && costs.Writes == 1 && costs.Hits == 0 &&
              costs.GeneratedCells == area.Tiers.size(),
          "cold work counts one lookup miss, generation and publication");
    CHECK(fixture.Calls == 2 && fixture.Tiles->Costs().Writes == 2,
          "exactly the required owner and parent sources are prepared");
  }
  {
    Fixture fixture(directory, Availability::Unavailable);
    if (!fixture.Urban) { return Report(); }
    const auto result = Await(fixture, request);
    CHECK(result && result->Status == PreparedUrbanAreas::State::Ready && result->Product,
          "fresh producer loads the finished asset with unavailable source bytes");
    if (!result || !result->Product) { return Report(); }
    const auto bytes = EncodeUrbanArea(*result->Product);
    CHECK(bytes && *bytes == original,
          "fresh native hit preserves every field and provenance byte");
    const auto costs = fixture.Urban->Costs();
    CHECK(
        costs.Hits == 1 && costs.Misses == 0 && costs.Generations == 0 && costs.Writes == 0 &&
            costs.SourcePolls == 0 && costs.GeneratedCells == 0 && costs.ReadBytes > 0,
        "warm hit performs only native asset reading, without source layout, factory or GEOS work");
    CHECK(fixture.Calls == 0 && fixture.Tiles->Costs().Hits == 0 &&
              fixture.Tiles->Costs().Misses == 0,
          "warm producer never even asks the source-product service");
  }
  {
    std::ofstream out(directory / "expected.bin", std::ios::binary);
    out.write(reinterpret_cast<const char *>(original.data()),
              static_cast<std::streamsize>(original.size()));
    CHECK(out.good(), "cold snapshot is available to the independent child");
  }
  const auto child = fork();
  CHECK(child >= 0, "fresh-process check starts without carrying fixture threads");
  if (child == 0) {
    auto *log = std::fopen((directory / "child.log").c_str(), "w");
    if (log == nullptr || dup2(fileno(log), STDOUT_FILENO) < 0 ||
        dup2(fileno(log), STDERR_FILENO) < 0) {
      constexpr int redirectFailure = 126;
      _exit(redirectFailure);
    }
    execl(argv[0], argv[0], directory.c_str(), static_cast<char *>(nullptr));
    constexpr int execFailure = 127;
    _exit(execFailure);
  }
  if (child > 0) {
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "independent fresh-process native hit completes successfully");
  }
  std::error_code error;
  if (Failures.Value() == 0) { std::filesystem::remove_all(directory, error); }
  CHECK(!error, "isolated producer artifacts are removed");
  return Report();
}
