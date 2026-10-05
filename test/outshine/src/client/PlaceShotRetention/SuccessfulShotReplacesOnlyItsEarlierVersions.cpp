#include "src/client/PlaceShotRetention.h"
#include "Check.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

int main() {
  using namespace outshine::Test;
  namespace fs = std::filesystem;
  auto scratch = (fs::temp_directory_path() / "outshine-shot-retention-XXXXXX").string();
  CHECK(mkdtemp(scratch.data()) != nullptr, "owned scratch directory created");
  if (!fs::is_directory(scratch)) { return Report(); }
  const fs::path directory(scratch);
  const auto latest = directory / "Wien-22222222.png";
  const auto old = directory / "Wien-11111111.png";
  std::ofstream(old) << "old";
  CHECK(!outshine::Shots::KeepLatestPlaceShot(latest, "Wien") && fs::exists(old),
        "missing new output preserves earlier successful shot");
  std::ofstream(latest) << "new";
  constexpr std::array foreign{"WienWest-11111111.png",
                               "Wien-11111111-reference.png",
                               "Wien-zzzzzzzz.png",
                               "Wien.writing",
                               "Flensburg-11111111.png"};
  for (const auto *file : foreign) { std::ofstream(directory / file) << "foreign"; }
  fs::create_directory(directory / "Wien-33333333.png");
  fs::create_symlink(latest, directory / "Wien-44444444.png");
  CHECK(outshine::Shots::KeepLatestPlaceShot(latest, "Wien").has_value(), "retention succeeds");
  CHECK(fs::exists(latest) && !fs::exists(old), "only earlier matching file is removed");
  for (const auto *file : foreign) {
    CHECK(fs::exists(directory / file), "foreign file preserved");
  }
  CHECK(fs::is_directory(directory / "Wien-33333333.png") &&
            fs::is_symlink(directory / "Wien-44444444.png"),
        "directories and symlinks preserved");
  CHECK(outshine::Shots::KeepLatestPlaceShot(latest, "Wien").has_value(), "repeated save is safe");
  CHECK(!outshine::Shots::KeepLatestPlaceShot(latest, "Flensburg"), "mismatched place is refused");
  std::error_code error;
  fs::remove_all(directory, error);
  CHECK(!error, "owned scratch removed");
  return Report();
}
