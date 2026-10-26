#include "ScenarioLoading.h"
#include <Engine.h>
#include "WriteFileAtomically.h"
#include <cstddef>
#include <expected>
#include <span>
#include <string_view>
#include <utility>
#include "Check.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace {
std::expected<size_t, std::string>
RoundTripScenario(const outshine::Scenario::Document &declaration, std::string_view scratchPath) {
  outshine::Engine engine;
  if (auto declared = engine.declare(declaration); !declared) {
    return std::unexpected(std::move(declared.error()));
  }
  auto first = engine.writeScenario();
  if (!first) { return std::unexpected(std::move(first.error())); }
  if (auto written = outshine::WriteFileAtomically(scratchPath, std::as_bytes(std::span(*first)));
      !written) {
    return std::unexpected(std::move(written.error()));
  }
  outshine::Engine restored;
  if (auto read = outshine::Test::LoadAndDeclareScenario(restored, scratchPath); !read) {
    return std::unexpected(std::move(read.error()));
  }
  auto second = restored.writeScenario();
  if (!second) { return std::unexpected(std::move(second.error())); }
  if (*first != *second) {
    return std::unexpected("serialized declarations differ after reimport");
  }
  return first->size();
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-roundtrip-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "scratch directory created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const auto path = directory + "/state.scn";
  Scenario::Document declaration;
  declaration.Tables = {
      {.Id = "state", .Columns = {"key", "value"}, .Types = {false, true}, .Rows = {{"one", "2"}}}};
  const auto held = RoundTripScenario(declaration, path);
  CHECK(held && *held > 0, "valid declaration survives checked file roundtrip");
  std::ifstream beforeFile(path);
  const std::string before((std::istreambuf_iterator<char>(beforeFile)), {});
  auto invalid = declaration;
  invalid.Tables[0].Types.push_back(true);
  CHECK(!RoundTripScenario(invalid, path), "export failure is propagated");
  std::ifstream afterFile(path);
  const std::string after((std::istreambuf_iterator<char>(afterFile)), {});
  CHECK(after == before, "export failure preserves previous scratch contents");
  CHECK(!RoundTripScenario(declaration, directory), "destination directory is refused");
  CHECK(!RoundTripScenario(declaration, directory + "/missing/state.scn"),
        "missing output parent is refused");
  CHECK(RoundTripScenario(declaration, path).has_value(), "valid retry succeeds");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "scratch directory removed");
  return Report();
}
