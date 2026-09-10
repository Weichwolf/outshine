#include "src/client/ScenarioRoundTrip.h"
#include "src/client/PlaceCamera.h"
#include "Check.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

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
  const auto held = Client::RoundTripScenario(declaration, path);
  CHECK(held && *held > 0, "valid declaration survives checked file roundtrip");
  std::ifstream beforeFile(path);
  const std::string before((std::istreambuf_iterator<char>(beforeFile)), {});
  auto invalid = declaration;
  invalid.Tables[0].Types.push_back(true);
  CHECK(!Client::RoundTripScenario(invalid, path), "export failure is propagated");
  std::ifstream afterFile(path);
  const std::string after((std::istreambuf_iterator<char>(afterFile)), {});
  CHECK(after == before, "export failure preserves previous scratch contents");
  CHECK(!Client::RoundTripScenario(declaration, directory), "destination directory is refused");
  CHECK(!Client::RoundTripScenario(declaration, directory + "/missing/state.scn"),
        "missing output parent is refused");
  const std::array places{Shots::Place{.Name = "one", .Declaration = declaration}};
  CHECK(Client::RoundTripPlaces(places, directory) == 1, "CLI aggregate reports output failure");
  CHECK(Client::RoundTripPlaces(places, path) == 0, "valid retry succeeds");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "scratch directory removed");
  return Report();
}
