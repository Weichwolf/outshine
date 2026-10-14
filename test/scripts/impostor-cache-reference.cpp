#include "content/AssetCache.h"
#include "ImpostorAtlas.h"
#include "ImpostorCards.h"
#include "ImpostorSurface.h"
#include "Sha256.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <ios>
#include <optional>
#include <print>
#include <string>
#include <system_error>
#include <vector>

namespace {
template <class T> bool Write(const std::filesystem::path &path, const std::vector<T> &data) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(data.data()),
               static_cast<std::streamsize>(data.size() * sizeof(T)));
  output.flush();
  return static_cast<bool>(output);
}

bool ExportViews(const outshine::Content::ImpostorAtlas &atlas,
                 const outshine::Content::ImpostorCards &cards,
                 const std::filesystem::path &output,
                 std::ofstream &metadata,
                 bool depth) {
  using namespace outshine;
  for (size_t at = 0; at < atlas.Views().size(); ++at) {
    const auto &view = atlas.Views()[at];
    if (view.Materials.size() != view.Texels.size()) { return false; }
    const auto reconstructed =
        depth ? Render::BuildImpostorSurface(atlas, at) : std::optional<Geometry>{};
    if (depth && !reconstructed) { return false; }
    const Geometry *geometry = depth ? &*reconstructed : &cards.Views[at].Surface;
    const auto positions = geometry->positionsOf(0);
    double maximum = 0;
    for (size_t vertex = 0; vertex < positions.size(); ++vertex) {
      maximum =
          std::max(maximum,
                   std::abs(static_cast<double>(positions[vertex]) - atlas.CentreM()[vertex % 3]) /
                       atlas.HalfExtentM());
    }
    std::vector<float> rgba(view.Texels.size() * 4);
    std::vector<float> depths(view.Texels.size());
    for (size_t pixel = 0; pixel < view.Texels.size(); ++pixel) {
      const auto &texel = view.Texels[pixel];
      depths[pixel] = texel.Depth;
      if (texel.Surface == 0) { continue; }
      const auto &colour = view.Materials[pixel].BaseColour;
      for (size_t channel = 0; channel < 3; ++channel) {
        rgba[pixel * 4 + channel] = colour[channel];
      }
      rgba[pixel * 4 + 3] = 1;
    }
    if (!Write(output / (std::to_string(at) + ".rgba"), rgba) ||
        !Write(output / (std::to_string(at) + ".depth"), depths) ||
        !Write(output / (std::to_string(at) + ".positions"),
               std::vector(positions.begin(), positions.end())) ||
        !Write(output / (std::to_string(at) + ".uv"),
               std::vector(geometry->textureOf(0).begin(), geometry->textureOf(0).end())) ||
        !Write(output / (std::to_string(at) + ".indices"),
               std::vector(geometry->trianglesOf(0).begin(), geometry->trianglesOf(0).end()))) {
      return false;
    }
    metadata << std::format("{},{},{:.17g},{:.17g},{:.17g},{:.17g},{},{},{:.17g}\n",
                            at,
                            atlas.Pixels(),
                            atlas.CentreM()[0],
                            atlas.CentreM()[1],
                            atlas.CentreM()[2],
                            atlas.HalfExtentM(),
                            positions.size() / 3,
                            geometry->trianglesOf(0).size() / 3,
                            maximum);
  }
  return true;
}
}

int main(int argc, const char *const *argv) {
  using namespace outshine;
  if (argc != 4 && argc != 5) { return 2; }
  std::ifstream input(argv[2]);
  const std::string provenance{std::istreambuf_iterator<char>(input), {}};
  if (provenance.empty()) { return 2; }
  auto cache = AssetCache::Open(argv[1]);
  if (!cache) { return 2; }
  const auto loaded = (*cache)->Load(Sha256Hex(provenance), size_t{64} * 1024 * 1024);
  if (!loaded || !*loaded) { return 2; }
  std::string error;
  const auto atlas = Content::ImpostorAtlas::Decode((**loaded).Bytes(), provenance, error);
  if (!atlas) {
    std::println(stderr, "{}", error);
    return 2;
  }
  const auto cardAsset =
      (*cache)->Load(Content::ImpostorCards::AssetKey(provenance), size_t{64} * 1024 * 1024);
  if (!cardAsset || !*cardAsset) { return 2; }
  const auto cards =
      Content::ImpostorCards::Decode((**cardAsset).Bytes(), provenance, size_t{64} * 1024 * 1024);
  if (!cards || cards->Views.size() != atlas->Views().size()) { return 2; }
  const bool depth = argc == 5;
  if (depth && std::string(argv[4]) != "depth") { return 2; }
  const std::filesystem::path output(argv[3]);
  std::error_code failure;
  std::filesystem::create_directories(output, failure);
  if (failure) { return 2; }
  std::ofstream metadata(output / "views.csv");
  metadata
      << "view,pixels,centre_x,centre_y,centre_z,half_extent,vertices,triangles,max_extent_ratio\n";
  if (!ExportViews(*atlas, *cards, output, metadata, depth)) { return 2; }
  std::println("{}", depth ? "diagnostic-depth-reconstruction" : "actual-cached-cards");
  metadata.flush();
  return metadata ? 0 : 2;
}
