#include "ByteArchive.h"
#include "GroupImpostorComposer.h"
#include "GroupImpostorProbeFields.h"
#include "GroupImpostorView.h"
#include "ImpostorAtlas.h"
#include "Material.h"
#include "ReadTextFile.h"
#include "Vec3.h"
#include "WriteFileAtomically.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using Json = nlohmann::json;
constexpr size_t kManifestLimit = 128 * size_t{1024};
constexpr int kSourceSideLimit = 512;

Json ReadManifest(const std::filesystem::path &path) {
  const auto bytes = ReadTextFile(path.string(), kManifestLimit);
  return bytes ? Json::parse(*bytes, nullptr, false) : Json{};
}

std::optional<Vec3> ReadVector(const Json &value) {
  if (!value.is_array() || value.size() != 3 || !std::ranges::all_of(value, &Json::is_number)) {
    return std::nullopt;
  }
  return Vec3{{value[0].get<double>(), value[1].get<double>(), value[2].get<double>()}};
}

struct AtlasSize {
  Vec3 CentreM;
  double HalfExtentM;
  int Pixels;
};

std::optional<AtlasSize> ReadAtlasSize(const Json &manifest) {
  if (!manifest.contains("pixels") || !manifest["pixels"].is_number_unsigned() ||
      !manifest.contains("centre_m") || !manifest.contains("half_extent_m") ||
      !manifest["half_extent_m"].is_number()) {
    return std::nullopt;
  }
  const auto centre = ReadVector(manifest["centre_m"]);
  const auto side = manifest["pixels"].get<unsigned>();
  if (!centre || side < 3 || side > kSourceSideLimit) { return std::nullopt; }
  return AtlasSize{.CentreM = *centre,
                   .HalfExtentM = manifest["half_extent_m"].get<double>(),
                   .Pixels = static_cast<int>(side)};
}

bool ReadSample(ByteReader &reader,
                ImpostorAtlas::Texel &texel,
                ImpostorAtlas::SurfaceSample &material) {
  if (!reader.Number(texel.Surface) || !reader.Number(texel.Depth)) { return false; }
  for (float &component : texel.Normal) {
    if (!reader.Number(component)) { return false; }
  }
  for (float &component : material.BaseColour) {
    if (!reader.Number(component)) { return false; }
  }
  return reader.Number(material.Roughness) && reader.Number(material.Metalness);
}

std::optional<ImpostorAtlas::View>
ReadView(const std::filesystem::path &path, const Vec3 &axis, size_t count) {
  constexpr size_t kTexelBytes = sizeof(uint32_t) + 9 * sizeof(float);
  const auto bytes = ReadTextFile(path.string(), count * kTexelBytes);
  if (!bytes || bytes->size() != count * kTexelBytes) { return std::nullopt; }
  ByteReader reader({reinterpret_cast<const uint8_t *>(bytes->data()), bytes->size()});
  ImpostorAtlas::View result{.TowardEye = axis,
                             .Texels = std::vector<ImpostorAtlas::Texel>(count),
                             .Materials = std::vector<ImpostorAtlas::SurfaceSample>(count)};
  for (size_t at = 0; at < count; ++at) {
    if (!ReadSample(reader, result.Texels[at], result.Materials[at])) { return std::nullopt; }
  }
  return result;
}

std::optional<ImpostorAtlas> ReadPrototype(const std::filesystem::path &folder,
                                           std::string &error) {
  const auto manifest = ReadManifest(folder / "group.json");
  const auto size = ReadAtlasSize(manifest);
  constexpr size_t kMaximumDirections = 64;
  if (!size || !manifest.contains("directions") || !manifest["directions"].is_array() ||
      manifest["directions"].size() > kMaximumDirections) {
    return std::nullopt;
  }
  std::vector<ImpostorAtlas::View> views;
  uint32_t surfaces = 1;
  const auto side = static_cast<size_t>(size->Pixels);
  for (size_t at = 0; at < manifest["directions"].size(); ++at) {
    const auto axis = ReadVector(manifest["directions"][at]);
    if (!axis) { return std::nullopt; }
    auto view = ReadView(folder / (std::to_string(at) + ".field"), *axis, side * side);
    if (!view) { return std::nullopt; }
    surfaces = std::max(surfaces,
                        std::ranges::max(view->Texels, {}, &ImpostorAtlas::Texel::Surface).Surface);
    views.push_back(std::move(*view));
  }
  constexpr uint32_t kMaximumSurfaces = 64;
  if (surfaces > kMaximumSurfaces) { return std::nullopt; }
  return ImpostorAtlas::Create(size->Pixels,
                               size->CentreM,
                               size->HalfExtentM,
                               std::vector<Material>(surfaces),
                               std::move(views),
                               error);
}

std::optional<std::vector<GroupImpostorInstance>> ReadInstances(const Json &manifest) {
  constexpr size_t kMaximumInstances = 1024;
  if (!manifest.contains("roots_m") || !manifest["roots_m"].is_array() ||
      manifest["roots_m"].size() > kMaximumInstances) {
    return std::nullopt;
  }
  const auto &roots = manifest["roots_m"];
  const bool scaled = manifest.contains("tree_scales");
  if (scaled &&
      (!manifest["tree_scales"].is_array() || manifest["tree_scales"].size() != roots.size())) {
    return std::nullopt;
  }
  std::vector<GroupImpostorInstance> placed;
  placed.reserve(roots.size());
  for (size_t at = 0; at < roots.size(); ++at) {
    const auto root = ReadVector(roots[at]);
    if (!root || (scaled && !manifest["tree_scales"][at].is_number())) { return std::nullopt; }
    const double scale = scaled ? manifest["tree_scales"][at].get<double>() : 1.0;
    placed.push_back({.RootM = *root, .Scale = scale});
  }
  return placed;
}

bool Emit(const ImpostorAtlas &atlas, const std::filesystem::path &folder, uint32_t cardSide) {
  for (size_t at = 0; at < atlas.Views().size(); ++at) {
    const auto name = std::to_string(at);
    if (!GroupProbe::SaveView(folder / (name + ".field"), atlas.Views()[at]) ||
        !GroupProbe::SaveCard(folder / (name + ".card"), atlas, at, cardSide)) {
      return false;
    }
  }
  return true;
}

int Run(const char *const *arguments) {
  uint32_t cardSide = 0;
  const std::string_view pixels = arguments[4];
  const auto parsed = std::from_chars(pixels.data(), pixels.data() + pixels.size(), cardSide);
  if (parsed.ec != std::errc{} || parsed.ptr != pixels.data() + pixels.size() ||
      !GroupImpostorView::ValidSide(cardSide)) {
    return 2;
  }
  std::string error;
  const auto prototype = ReadPrototype(arguments[1], error);
  auto manifest = ReadManifest(arguments[2]);
  const auto size = ReadAtlasSize(manifest);
  const auto placed = ReadInstances(manifest);
  if (!prototype || !size || !placed) {
    std::cerr << "invalid reusable atlas or group layout " << error << '\n';
    return 1;
  }
  const auto began = std::chrono::steady_clock::now();
  const auto atlas = ComposeGroupImpostor(*prototype,
                                          {.CentreM = size->CentreM,
                                           .HalfExtentM = size->HalfExtentM,
                                           .Pixels = size->Pixels,
                                           .Instances = *placed},
                                          error);
  if (!atlas) {
    std::cerr << error << '\n';
    return 1;
  }
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began);
  const std::filesystem::path folder(arguments[3]);
  if (!Emit(*atlas, folder, cardSide)) { return 1; }
  auto axes = Json::array();
  for (const auto &view : atlas->Views()) { axes.push_back(view.TowardEye.Axis); }
  manifest.erase("roots_m");
  manifest.erase("tree_scales");
  manifest["directions"] = std::move(axes);
  manifest["card_side"] = cardSide;
  manifest["card_views"] = atlas->Views().size();
  manifest["composition_ms"] = elapsed.count();
  const auto text = manifest.dump(2);
  return WriteFileAtomically((folder / "group.json").string(), std::as_bytes(std::span(text))) ? 0
                                                                                               : 1;
}

}

int main(int argc, const char *const *argv) {
  if (argc != 5) {
    std::cerr << "compose_groups <tree capture> <group layout> <output> <card pixels>\n";
    return 2;
  }
  return Run(argv);
}
