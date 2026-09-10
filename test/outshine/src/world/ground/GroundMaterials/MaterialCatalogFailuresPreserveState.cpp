#include "GroundMaterials.h"
#include "Check.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  auto temporary = (std::filesystem::temp_directory_path() / "outshine-materials-XXXXXX").string();
  const char *created = mkdtemp(temporary.data());
  CHECK(created != nullptr, "temporary directory created");
  if (!created) { return Report(); }
  const auto path = std::filesystem::path(created) / "materials.json";
  const std::string row =
      R"({"name":"reference","peakFriction":0.5,"surface":"coherent","slope":{"plausibleDeg":[0,90]}})";
  const auto write = [&](const std::string &text) {
    std::ofstream file(path);
    file << text;
  };
  write("{\"frictionModel\":{\"reference\":\"reference\"},\"classes\":[" + row + "]}");
  GroundMaterials materials;
  CHECK(materials.Load(path.c_str()), "valid catalog loads");
  CHECK(materials.Ready() && materials.Count() == 1 && materials.At(0).FrictionFactor == 1,
        "reference friction resolves analytically");
  for (const std::string &invalid :
       {"{\"frictionModel\":{\"reference\":\"reference\"},\"classes\":[" + row + ",{}]}",
        "{\"frictionModel\":{\"reference\":\"missing\"},\"classes\":[" + row + "]}",
        "{\"frictionModel\":{\"reference\":\"reference\"},\"classes\":[" + row + "," + row + "]}",
        "{\"frictionModel\":{\"reference\":\"reference\"},\"classes\":[" + row +
            ",{\"peakFriction\":1,\"surface\":\"coherent\",\"slope\":{\"plausibleDeg\":[0,90]}}]}",
        "{\"frictionModel\":{\"reference\":\"reference\"},\"classes\":[" +
            row.substr(0, row.size() - 1) + ",\"litter\":{\"class\":\"missing\"}}]}",
        std::string("not json"),
        std::string(1024 * 1024 + 1, ' ')}) {
    write(invalid);
    CHECK(!materials.Load(path.c_str()) && !materials.Error().empty(), "bad catalog rejected");
    CHECK(materials.Ready() && materials.Count() == 1 && materials.Find("reference") == 0 &&
              materials.At(0).PeakFriction == 0.5f && materials.At(0).FrictionFactor == 1,
          "failed replacement preserves complete previous catalog");
  }
  const std::string forward = row.substr(0, row.size() - 1) + ",\"litter\":{\"class\":\"later\"}}";
  const std::string later =
      R"({"name":"later","peakFriction":1,"surface":"coherent","slope":{"plausibleDeg":[0,90]},"litter":{"class":"later"}})";
  write("{\"frictionModel\":{\"reference\":\"reference\"},\"classes\":[" + forward + "," + later +
        "]}");
  CHECK(materials.Load(path.c_str()), "forward and self references accepted");
  CHECK(materials.Count() == 2 && materials.Find("reference") == 0 &&
            materials.Find("later") == 1 && materials.At(0).LitterClass == 1 &&
            materials.At(1).LitterClass == 1 && materials.At(1).FrictionFactor == 2,
        "reference resolution preserves material ordering and ratios");
  CHECK(!materials.Load(nullptr) && materials.Ready(), "null path preserves catalog");
  std::filesystem::remove(path);
  CHECK(!materials.Load(path.c_str()) && materials.Ready(), "missing file preserves catalog");
  CHECK(materials.Load("src/assets/world/ground-materials.json") && materials.Error().empty(),
        "shipped catalog loads after failures");
  std::error_code cleanup;
  std::filesystem::remove_all(temporary, cleanup);
  CHECK(!cleanup, "temporary files removed");
  return Report();
}
