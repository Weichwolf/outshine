#include "AssetCache.h"
#include "PreparedBuildingAssets.h"
#include "PageIndexAsset.h"
#include "SourceProductSelection.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "Viewing.h"
#include "Vec2.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <ratio>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <utility>

namespace {
template <class T> bool Number(std::string_view text, T &value) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

outshine::Render::CameraBasis Toward(outshine::Vec3 eye, const outshine::Vec3 &target) {
  using namespace outshine;
  Vec3 forward = target - eye;
  if (!Normalise(forward)) { return {}; }
  const std::array axes{Vec3{{1, 0, 0}}, Vec3{{0, 1, 0}}, Vec3{{0, 0, 1}}};
  const auto *const helper = std::ranges::min_element(
      axes, {}, [&](const Vec3 &axis) { return std::abs(Dot(axis, forward)); });
  auto right = Cross(forward, *helper);
  (void)Normalise(right);
  auto up = Cross(right, forward);
  (void)Normalise(up);
  return {.EyeM = eye, .Forward = forward, .Right = right, .Up = up};
}

std::string_view Kind(outshine::SourceProductSelection::Kind kind) {
  using K = outshine::SourceProductSelection::Kind;
  switch (kind) {
    case K::Geometry: return "original";
    case K::Hull: return "hull";
    case K::Shell: return "shell";
    case K::Unresolved: return "unresolved";
    case K::MaterialField: return "field";
  }
  return "invalid";
}
}

int main(int argc, char *const *argv) {
  using namespace outshine;
  if (argc != 10 && argc != 12) {
    std::cerr << "source_geometry_control <asset-db> <base-key> <sources> <eye-x> <eye-y> <eye-z> "
                 "<focal-px> <error-px> <lookahead-m> [viewport-width-px viewport-height-px]\n";
    return 2;
  }
  size_t count = 0;
  Vec3 eye;
  ProjectedErrorBudget projection;
  double lookahead = 0;
  if (!Number(argv[3], count) || !Number(argv[4], eye[0]) || !Number(argv[5], eye[1]) ||
      !Number(argv[6], eye[2]) || !Number(argv[7], projection.FocalPx) ||
      !Number(argv[8], projection.AllowedErrorPx) || !Number(argv[9], lookahead)) {
    return 2;
  }
  Vec2 viewport;
  if (argc == 12 && (!Number(argv[10], viewport[0]) || !Number(argv[11], viewport[1]))) {
    return 2;
  }
  const auto bytes = PageIndexAsset::EncodedBytes(count);
  auto opened = AssetCache::Open(argv[1]);
  if (!bytes || !opened) { return 3; }
  const std::shared_ptr<AssetCache> cache(std::move(*opened));
  Data::RawSourceCache raw({.Directory = (std::filesystem::temp_directory_path() /
                                          "outshine-master-source-geometry-unused-sources")
                                             .string()});
  const Data::SourceSet sources(raw);
  const auto assets = Generators::PreparedBuildingAssets::Open(cache, sources);
  if (!assets) { return 4; }
  const auto metadataBegan = std::chrono::steady_clock::now();
  const auto groups = (*assets)->DescribeSourceGroups(argv[2], {.IndexBytesMost = *bytes});
  if (!groups || !*groups || (**groups).Members.size() != count) { return 5; }
  const auto metadataMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - metadataBegan)
          .count();
  const auto &input = **groups;
  std::vector<SourceProductSelection::Source> traits;
  traits.reserve(input.Members.size());
  for (const auto &member : input.Members) {
    traits.push_back({.Cohort = member.Cell,
                      .FieldSupported = member.FieldSupported,
                      .Faces = member.Faces,
                      .ShellErrorM = member.ShellErrorM});
  }
  const auto began = std::chrono::steady_clock::now();
  const auto selector = SourceProductSelection::Of(
      std::shared_ptr<const PlanHierarchy>(input.Index, &input.Index->Hierarchy), traits, argv[2]);
  if (!selector) { return 6; }
  const auto camera = Toward(eye, input.Index->Hierarchy.Nodes()[0].Bounds.Middle());
  const auto selected = argc == 12 ? selector->SelectGeometry(eye, viewport, projection, lookahead)
                                   : selector->SelectGeometry(camera, projection, lookahead);
  if (!selected) { return 7; }
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  const auto costs = (*assets)->Costs();
  std::cout << std::setprecision(17) << "{\"sources\":" << count
            << ",\"header_bytes\":" << costs.ReadBytes
            << ",\"form_bytes\":" << costs.SurfaceReadBytes
            << ",\"geometry_bytes\":" << costs.GeometryReadBytes
            << ",\"metadata_ms\":" << metadataMs << ",\"selection_with_setup_ms\":" << elapsed
            << ",\"visited\":" << selected->VisitedNodes << ",\"products\":[";
  bool first = true;
  for (const auto &product : selected->Products) {
    if (!first) { std::cout << ','; }
    first = false;
    std::cout << '[' << product.Node << ",\"" << Kind(product.Representation) << "\","
              << product.WorldErrorM << ',' << product.ProjectedErrorPx << ']';
  }
  std::cout << "]}\n";
  return costs.SurfaceReadBytes == 0 && costs.GeometryReadBytes == 0 ? 0 : 8;
}
