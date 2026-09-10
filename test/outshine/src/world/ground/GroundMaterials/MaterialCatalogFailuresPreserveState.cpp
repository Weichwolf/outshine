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
  for (const auto *friction : {"1e300", "1e-300", "0", "-1"}) {
    auto invalid = row;
    invalid.replace(invalid.find("0.5"), 3, friction);
    write("{\"frictionModel\":{\"reference\":\"reference\"},\"classes\":[" + invalid + "]}");
    CHECK(!materials.Load(path.c_str()), "nonrepresentable positive friction rejected");
    CHECK(materials.Count() == 1 && materials.At(0).PeakFriction == 0.5f &&
              materials.At(0).FrictionFactor == 1,
          "friction input failure preserves catalog");
  }
  for (const auto *reference : {"reference", "extreme"}) {
    auto small = row;
    small.replace(small.find("0.5"), 3, "1e-30");
    const std::string large =
        R"({"name":"extreme","peakFriction":1e30,"surface":"coherent","slope":{"plausibleDeg":[0,90]}})";
    write("{\"frictionModel\":{\"reference\":\"" + std::string(reference) + "\"},\"classes\":[" +
          small + "," + large + "]}");
    CHECK(!materials.Load(path.c_str()), "unrepresentable friction quotient rejected");
    CHECK(materials.Count() == 1 && materials.At(0).PeakFriction == 0.5f &&
              materials.At(0).FrictionFactor == 1,
          "friction quotient failure preserves catalog");
  }
  for (const auto *model : {R"("moistureModel":{"kWet":1e300})",
                            R"("moistureModel":{"kWet":-0.1})",
                            R"("moistureModel":{"kWet":"0.5"})",
                            R"("moistureModel":null)",
                            R"("specularModel":false)",
                            R"("specularModel":{"edges":[0.5,0.5]})",
                            R"("specularModel":{"edges":[0.8,0.2]})",
                            R"("specularModel":{"edges":[0.5,0.500000001]})",
                            R"("specularModel":{"edges":[0,1e300]})",
                            R"("specularModel":{"edges":[-1,1]})",
                            R"("specularModel":{"edges":[0,"1"]})",
                            R"("specularModel":{"edges":[0]})"}) {
    write("{" + std::string(model) +
          ",\"frictionModel\":{\"reference\":\"reference\"},\"classes\":[" + row + "]}");
    CHECK(!materials.Load(path.c_str()), "invalid moisture model rejected");
    CHECK(materials.Count() == 1 && materials.At(0).PeakFriction == 0.5f,
          "model failure preserves previous catalog");
  }
  for (const auto *moisture : {"-0.1", "1.1", "1e300", "null", "\"0.5\""}) {
    write("{\"frictionModel\":{\"reference\":\"reference\"},\"classes\":[" +
          row.substr(0, row.size() - 1) + ",\"moisture\":" + moisture + "}]}");
    CHECK(!materials.Load(path.c_str()), "invalid material moisture rejected");
    CHECK(materials.Count() == 1 && materials.At(0).PeakFriction == 0.5f,
          "moisture failure preserves previous catalog");
  }
  for (const auto *moisture : {"0", "0.25", "0.5", "0.75", "1"}) {
    write(
        std::string(
            R"({"moistureModel":{"kWet":0.5},"specularModel":{"edges":[0.25,0.75]},"frictionModel":{"reference":"sample"},"classes":[{"name":"sample","peakFriction":1,"surface":"particulate","albedo":[1,1,1],"slope":{"plausibleDeg":[0,90]},"moisture":)") +
        moisture + "}]}");
    CHECK(materials.Load(path.c_str()), "valid moisture model loads after failures");
    const float amount = std::stof(moisture);
    const float expected = amount <= 0.25f ? 0.0f : (amount >= 0.75f ? 1.0f : 0.5f);
    CHECK(materials.At(0).SpecularScale == expected,
          "smoothstep has exact dry, middle and saturated values");
    CHECK(materials.At(0).Albedo[0] == 1.0f - 0.5f * amount,
          "wet albedo follows declared attenuation");
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
