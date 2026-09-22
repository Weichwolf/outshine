#include "GroundMaterials.h"
#include "VegetationTemplates.h"
#include "Check.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  auto temporary = (std::filesystem::temp_directory_path() / "outshine-metrics-XXXXXX").string();
  const char *created = mkdtemp(temporary.data());
  CHECK(created != nullptr, "temporary directory created");
  if (!created) { return Report(); }
  const auto path = std::filesystem::path(created) / "materials.json";
  const auto write = [&](const std::string &fields) {
    std::ofstream file(path);
    file
        << R"({"frictionModel":{"reference":"sample"},"classes":[{"name":"sample","peakFriction":1,"surface":"coherent",)"
        << fields << "}]}";
  };
  GroundMaterials materials;
  const std::string slope = R"("slope":{"plausibleDeg":[0,90]})";
  write(slope + R"(,"grainSizeM":0,"heightAmplitudeM":0,"detailScaleM":[2,0.5])");
  CHECK(materials.Load(path.c_str()), "zero grain and relief are valid for smooth surfaces");
  const auto preserved = [&] {
    return materials.Count() == 1 && materials.At(0).GrainSizeM == 0 &&
           materials.At(0).HeightAmplitudeM == 0 && materials.At(0).DetailCoarseM == 2 &&
           materials.At(0).DetailFineM == 0.5f && materials.At(0).SlopeMaxDeg == 90 &&
           !materials.At(0).SlopeExposesRock;
  };
  CHECK(preserved(), "metric values retain declared units");
  for (const auto *field : {R"("grainSizeM":-1)",
                            R"("grainSizeM":1e300)",
                            R"("grainSizeM":"1")",
                            R"("heightAmplitudeM":-1)",
                            R"("heightAmplitudeM":1e300)",
                            R"("heightAmplitudeM":null)",
                            R"("detailScaleM":[0,1])",
                            R"("detailScaleM":[1,-1])",
                            R"("detailScaleM":[1,1e300])",
                            R"("detailScaleM":[1e-300,1])",
                            R"("detailScaleM":[1,"1"])",
                            R"("detailScaleM":[1])",
                            R"("detailScaleM":[1,2,3])",
                            R"("detailScaleM":null)"}) {
    write(slope + "," + field);
    CHECK(!materials.Load(path.c_str()) && !materials.Error().empty(), "invalid metric rejected");
    CHECK(preserved(), "metric failure preserves catalog");
  }
  for (const auto *interval : {"[-1,90]",
                               "[0,91]",
                               "[80,20]",
                               "[0,1e300]",
                               "[null,90]",
                               "[0,\"90\"]",
                               "[0]",
                               "{\"a\":0,\"b\":90}"}) {
    write(std::string(R"("slope":{"plausibleDeg":)") + interval + "}");
    CHECK(!materials.Load(path.c_str()) && !materials.Error().empty(), "invalid slope rejected");
    CHECK(preserved(), "slope failure preserves catalog");
  }
  write(R"("slope":{"plausibleDeg":[90,90]},"detailScaleM":[0.5,0.5])");
  CHECK(materials.Load(path.c_str()) && materials.At(0).SlopeMaxDeg == 90 &&
            materials.At(0).DetailCoarseM == 0.5f && materials.At(0).DetailFineM == 0.5f,
        "equal finite boundaries remain valid on retry");
  write(slope + R"(,"slopeExposesRock":true)");
  CHECK(materials.Load(path.c_str()) && materials.At(0).SlopeExposesRock &&
            materials.At(0).SlopeMaxDeg == 90,
        "declared rock exposure preserves the separate plausible-slope limit");
  write(slope + R"(,"slopeExposesRock":"true")");
  CHECK(!materials.Load(path.c_str()) && materials.At(0).SlopeExposesRock &&
            materials.Error().find("slopeExposesRock") != std::string::npos,
        "invalid exposure flag cannot replace the last valid catalog");
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "shipped catalog remains valid");
  const int asphalt = materials.Find("asphalt");
  const int limestone = materials.Find("limestone");
  const int water = materials.Find("water");
  CHECK(asphalt >= 0 && limestone >= 0 && water >= 0,
        "built, rocky and water-cover comparison materials are declared");
  if (asphalt >= 0 && limestone >= 0 && water >= 0) {
    CHECK(!materials.At(static_cast<size_t>(asphalt)).SlopeExposesRock &&
              materials.At(static_cast<size_t>(limestone)).SlopeExposesRock &&
              materials.At(static_cast<size_t>(water)).SlopeExposesRock,
          "pavement remains built while rocky and water-covered slopes expose bedrock");
  }
  VegetationTemplates templates;
  CHECK(templates.Load("src/assets/world/vegetation.json", materials),
        "shipped vegetation templates accept the material contract");
  const int sand = materials.Find("sand");
  bool sawAsphalt = false;
  bool sawSand = false;
  bool sawWater = false;
  for (size_t row = 0; row < templates.TemplateCount(); ++row) {
    const auto &one = templates.Rows()[row];
    if (one.GroundClass == asphalt) {
      sawAsphalt = true;
      CHECK(one.Edge[3] == 90.0f, "steep asphalt never exposes procedural rock");
    }
    if (one.GroundClass == sand && sand >= 0) {
      sawSand = true;
      CHECK(one.Edge[3] == materials.At(static_cast<size_t>(sand)).SlopeMaxDeg,
            "natural sand retains its declared rock-exposure slope");
    }
    if (one.GroundClass == water && water >= 0) {
      sawWater = true;
      CHECK(one.Edge[3] == materials.At(static_cast<size_t>(water)).SlopeMaxDeg,
            "water-covered steep terrain exposes its bedrock rather than a vertical blue wall");
    }
  }
  CHECK(sawAsphalt && sawSand && sawWater, "all exposure policies reach template rows");
  std::error_code cleanup;
  std::filesystem::remove_all(temporary, cleanup);
  CHECK(!cleanup, "temporary files removed");
  return Report();
}
