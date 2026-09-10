#include "GroundMaterials.h"
#include "Check.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  auto temporary = (std::filesystem::temp_directory_path() / "outshine-optics-XXXXXX").string();
  const char *created = mkdtemp(temporary.data());
  CHECK(created != nullptr, "temporary directory created");
  if (!created) { return Report(); }
  const auto path = std::filesystem::path(created) / "materials.json";
  const auto write = [&](const std::string &fields) {
    std::ofstream file(path);
    file
        << R"({"frictionModel":{"reference":"sample"},"classes":[{"name":"sample","peakFriction":1,"surface":"coherent","slope":{"plausibleDeg":[0,90]},)"
        << fields << "}]}";
  };
  GroundMaterials materials;
  const std::string valid =
      R"("roughness":0.5,"albedo":[0.25,0.5,0],"visibleBroadbandRatio":2,"litter":{"coverage":1})";
  write(valid);
  CHECK(materials.Load(path.c_str()), "spectral ratio above one can yield valid reflectance");
  CHECK(materials.Count() == 1 && materials.At(0).Albedo[0] == 0.5f &&
            materials.At(0).Albedo[1] == 1.0f && materials.At(0).Albedo[2] == 0.0f &&
            materials.At(0).Roughness == 0.5f && materials.At(0).LitterCoverage == 1.0f,
        "optical values retain exact known products and boundary values");
  for (const auto *invalid : {R"("roughness":-0.1)",
                              R"("roughness":1.1)",
                              R"("roughness":1e300)",
                              R"("roughness":"0.5")",
                              R"("roughness":null)",
                              R"("litter":{"coverage":-1})",
                              R"("litter":{"coverage":2})",
                              R"("litter":{"coverage":null})",
                              R"("litter":false)",
                              R"("albedo":[-0.1,0,0])",
                              R"("albedo":[0,1.1,0])",
                              R"("albedo":[0,0,1e300])",
                              R"("albedo":[0,0,"0.5"])",
                              R"("albedo":[0,0])",
                              R"("albedo":[0,0,0,0])",
                              R"("albedo":null)",
                              R"("visibleBroadbandRatio":-1)",
                              R"("visibleBroadbandRatio":1e300)",
                              R"("visibleBroadbandRatio":"1")",
                              R"("visibleBroadbandRatio":null)",
                              R"("albedo":[0.5,1,0],"visibleBroadbandRatio":2)"}) {
    write(invalid);
    CHECK(!materials.Load(path.c_str()) && !materials.Error().empty(),
          "invalid optical values fail with a diagnostic");
    CHECK(materials.Count() == 1 && materials.At(0).Albedo[0] == 0.5f &&
              materials.At(0).Albedo[1] == 1.0f && materials.At(0).Roughness == 0.5f &&
              materials.At(0).LitterCoverage == 1.0f,
          "rejected optical replacement preserves previous catalog");
  }
  write(R"("roughness":0,"albedo":[1,1,1],"visibleBroadbandRatio":0,"litter":{"coverage":0})");
  CHECK(materials.Load(path.c_str()) && materials.At(0).Albedo[0] == 0 &&
            materials.At(0).Roughness == 0 && materials.At(0).LitterCoverage == 0,
        "zero factors are valid on retry");
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "shipped catalog remains valid");
  std::error_code cleanup;
  std::filesystem::remove_all(temporary, cleanup);
  CHECK(!cleanup, "temporary files removed");
  return Report();
}
