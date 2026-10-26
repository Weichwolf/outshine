#include "Species.h"
#include "Check.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-species-XXXXXX").string();
  if (mkdtemp(directory.data()) == nullptr) {
    CHECK(false, "temporary species directory created");
    return Report();
  }
  const std::string path = directory + "/species.json";
  const std::string valid = R"({"name":"retained","height_m":10,"seed":17})";
  {
    std::ofstream file(path);
    file << valid;
  }
  TreeSpecies species;
  CHECK(ReadSpecies(path.c_str(), &species) && species.Definition() == valid,
        "complete source definition reaches the parser unchanged");
  CHECK(!ReadSpecies(nullptr, &species) && !ReadSpecies(path.c_str(), nullptr),
        "absent path or destination is refused");
  for (const auto &failed : {directory, directory + "/missing"}) {
    CHECK(!ReadSpecies(failed.c_str(), &species), "unreadable input cannot publish a species");
    CHECK(species.Definition() == valid, "failed IO preserves the previous definition");
  }
  for (const auto *bad : {"", "{\"name\":\"truncated"}) {
    {
      std::ofstream file(path);
      file << bad;
    }
    CHECK(!ReadSpecies(path.c_str(), &species), "empty and truncated definitions are refused");
    CHECK(species.Definition() == valid && species.Name() == "retained",
          "rejected replacement preserves the valid species");
  }
  std::filesystem::remove_all(directory);
  return Report();
}
