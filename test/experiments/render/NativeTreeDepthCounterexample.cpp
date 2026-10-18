#include "NativeTreeDepthNormals.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <cmath>
#include <print>
#include <vector>

int main() {
  using namespace outshine;
  constexpr uint32_t kPixels = 16;
  constexpr uint32_t kFirst = 4 + 4 * kPixels;
  constexpr float kFarDepth = 0.4f;
  constexpr float kNearDepth = 0.6f;
  std::vector<float> depth(size_t{kPixels} * kPixels);
  std::vector<uint32_t> labels(depth.size());
  std::vector<Content::ImpostorAtlas::Texel> texels(depth.size());
  for (const auto at : {kFirst, kFirst + 1}) {
    depth[at] = at == kFirst ? kFarDepth : kNearDepth;
    labels[at] = 1;
    texels[at] = {.Normal = {{0, 0, 1}}, .Depth = depth[at], .Surface = 1};
  }
  const auto patches = Render::BuildDepthSurfacePatches(
      {.Width = kPixels, .Height = kPixels, .Depth = depth, .Surface = labels, .AllowedError = 0});
  if (!patches || patches->size() != 1) { return 1; }
  Content::ImpostorAtlas::View view{
      .TowardEye = {{0, 0, 1}}, .Texels = std::move(texels), .Materials = {}};
  std::string error;
  auto atlas =
      Content::ImpostorAtlas::Create(kPixels, {}, 1, {Material{}}, {std::move(view)}, error);
  if (!atlas) {
    std::println(stderr, "{}", error);
    return 2;
  }
  const auto separate = NativeTreeDetailProbe::NormalPatches(*atlas, atlas->Views()[0], *patches);
  if (separate.size() != 2 ||
      std::ranges::any_of(separate, [](const auto &p) { return p.SlopeX != 0 || p.SlopeY != 0; })) {
    return 3;
  }
  const auto &joined = patches->front();
  const double x = kFirst % kPixels + 0.5;
  const auto row = kFirst / kPixels;
  const double y = static_cast<double>(row) + 0.5;
  const double residual = std::max(std::abs(joined.At(x, y) - depth[kFirst]),
                                   std::abs(joined.At(x + 1, y) - depth[kFirst + 1]));
  if (residual != 0) { return 4; }
  std::println("depth-only=1 joined-plane normal-control=2 separate-samples residual={}", residual);
}
