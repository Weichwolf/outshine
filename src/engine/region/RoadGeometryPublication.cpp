#include "RoadGeometryPublication.h"
#include "Digest.h"
#include "GeometryPageView.h"
#include "PieceMesh.h"
#include "SceneRenderer.h"
#include "RoadPageAsset.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
namespace {
uint64_t Identity(std::string_view key) {
  uint64_t result = kDigestBasis;
  for (const unsigned char byte : key) { result = DigestFolded(result, byte); }
  return result == 0 ? 1 : result;
}

uint64_t Ordinal(Generators::RoadPagePart part) {
  return (static_cast<uint64_t>(part.Kind) << 32u) | part.Index;
}

std::vector<SourceCoverage::Range> Ranges(std::span<const Generators::RoadPagePart> parts) {
  std::vector<SourceCoverage::Range> result;
  result.reserve(parts.size());
  for (const auto part : parts) { result.push_back({.First = Ordinal(part), .Count = 1}); }
  std::ranges::sort(result, {}, &SourceCoverage::Range::First);
  return result;
}

Mat4 Placement(const TangentFrame &source, const TangentFrame &target) {
  Mat4 result;
  if (source.Anchor().LatitudeDeg == target.Anchor().LatitudeDeg &&
      source.Anchor().LongitudeDeg == target.Anchor().LongitudeDeg) {
    return result;
  }
  const std::array axes{source.EastEcef(), source.UpEcef(), source.NorthEcef() * -1.0};
  for (size_t axis = 0; axis < axes.size(); ++axis) {
    const auto mapped = target.ToLocalDirection(axes[axis]);
    result[axis * 4] = mapped.EastM;
    result[axis * 4 + 1] = mapped.UpM;
    result[axis * 4 + 2] = -mapped.NorthM;
  }
  const auto shift = target.ToLocalPosition(source.OriginEcef());
  result[12] = shift.EastM;
  result[13] = shift.UpM;
  result[14] = -shift.NorthM;
  return result;
}
}

RoadGeometryPublication::RoadGeometryPublication(Render::GeometryPageResidency &pages,
                                                 SourceCoverage expected,
                                                 Render::MaterialRegistration material,
                                                 std::shared_ptr<const Mat4> placement,
                                                 Vec3 anchor)
    : Pages_(pages),
      Expected_(std::move(expected)),
      Material_(std::move(material)),
      Placement_(std::move(placement)),
      Anchor_(anchor),
      SourceKey_(Identity(Expected_.Revision())) {}

std::expected<std::unique_ptr<RoadGeometryPublication>, std::string>
RoadGeometryPublication::Begin(Render::SceneRenderer &renderer,
                               Render::GeometryPageResidency &pages,
                               const Generators::RoadPageManifest &manifest,
                               const TangentFrame &frame) {
  const auto began = std::chrono::steady_clock::now();
  std::vector<SourceCoverage::Range> ranges;
  for (const auto &page : manifest.Pages) {
    const auto part = Ranges(page.Parts);
    ranges.insert(ranges.end(), part.begin(), part.end());
  }
  std::ranges::sort(ranges, {}, &SourceCoverage::Range::First);
  const uint64_t sourceCount = ranges.empty() ? 0 : ranges.back().First + 1;
  auto expected = SourceCoverage::FromRanges(
      manifest.PlanKey + "/" + manifest.MaterialKey, sourceCount, ranges);
  if (!expected) {
    return std::unexpected("road publication source coverage overlaps or is invalid");
  }
  auto material = renderer.RegisterPieceMaterials(manifest.Materials.clone());
  if (!material) { return std::unexpected(std::move(material.error())); }
  const auto source = TangentFrame::At(manifest.Anchor);
  auto result = std::unique_ptr<RoadGeometryPublication>(
      new RoadGeometryPublication(pages,
                                  std::move(*expected),
                                  std::move(*material),
                                  std::make_shared<const Mat4>(Placement(source, frame)),
                                  source.OriginEcef()));
  result->PageCount_ = manifest.Pages.size();
  result->Keys_.reserve(manifest.Pages.size());
  pages.Clear();
  pages.BindRenderer(&renderer);
  pages.SetFrame(frame);
  result->PublicationMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  return result;
}

std::expected<bool, std::string>
RoadGeometryPublication::Advance(const Generators::RoadPageAsset &page,
                                 const Generators::RoadPageReference &reference,
                                 std::chrono::steady_clock::time_point until) {
  const std::string_view key = reference.Key;
  const auto began = std::chrono::steady_clock::now();
  const auto &mesh = page.Geometry.Mesh;
  const size_t sourceVertices = mesh.PositionM.size() / 3;
  const size_t vertices = mesh.Index.empty() ? 0 : sourceVertices;
  if (PackingKey_.empty()) {
    if (key.empty() || mesh.PositionM.size() % 3 != 0 ||
        mesh.NormalM.size() != mesh.PositionM.size() ||
        mesh.ColourRgba.size() / 4 != sourceVertices || mesh.ColourRgba.size() % 4 != 0) {
      return std::unexpected("road publication received incomplete vertex streams or identity");
    }
    PackingKey_ = key;
    Packed_.clear();
    Packed_.reserve(vertices);
  }
  if (PackingKey_ != key) { return std::unexpected("road publication changed an unfinished page"); }
  constexpr size_t batch = 128;
  while (Packed_.size() < vertices && std::chrono::steady_clock::now() < until) {
    const size_t end = std::min(vertices, Packed_.size() + batch);
    for (size_t at = Packed_.size(); at < end; ++at) {
      const size_t first = at * 3;
      Packed_.push_back(StoredVertex::Of(
          {{mesh.PositionM[first], mesh.PositionM[first + 1], mesh.PositionM[first + 2]}},
          {},
          {{mesh.NormalM[first], mesh.NormalM[first + 1], mesh.NormalM[first + 2]}}));
    }
  }
  const auto packed = std::chrono::steady_clock::now();
  PackingMs_ += std::chrono::duration<double, std::milli>(packed - began).count();
  if (Packed_.size() != vertices) { return false; }
  auto coverage = SourceCoverage::FromRanges(
      std::string(Expected_.Revision()), Expected_.SourceCount(), Ranges(reference.Parts));
  if (!coverage || !Expected_.Contains(*coverage)) {
    return std::unexpected("road publication has foreign source coverage");
  }
  const Render::GeometryPagePartView part{.Tangents = {},
                                          .Verts = Packed_,
                                          .Indices = mesh.Index,
                                          .Clusters = page.Geometry.Clusters,
                                          .Colours = mesh.ColourRgba,
                                          .Uv1 = {},
                                          .Surface = Render::PieceSurface::Registered(Material_),
                                          .Textured = false};
  const Render::GeometryPageView view{.Parts = mesh.Index.empty()
                                                   ? std::span<const Render::GeometryPagePartView>{}
                                                   : std::span(&part, 1),
                                      .Digest = Identity(key),
                                      .AssetKey = key,
                                      .FallbackHeights = false,
                                      .SelectedView = std::nullopt,
                                      .RequestedDetail = std::nullopt,
                                      .RequestedCell = std::nullopt,
                                      .OccupiedCells = 0,
                                      .Coverage = std::move(*coverage),
                                      .Placement = std::make_shared<const Mat4>(*Placement_)};
  std::string error;
  if (!Pages_.StageProduct(0, view, Anchor_, error, SourceKey_)) {
    return std::unexpected(std::move(error));
  }
  PublicationMs_ +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - packed).count();
  Keys_.emplace_back(key);
  PackingKey_.clear();
  Packed_.clear();
  return true;
}

std::expected<void, std::string> RoadGeometryPublication::Finish() {
  const auto began = std::chrono::steady_clock::now();
  if (!PackingKey_.empty() || Keys_.size() != PageCount_) {
    return std::unexpected("road publication has not consumed its complete manifest");
  }
  if (Keys_.empty()) { return {}; }
  std::ranges::sort(Keys_);
  std::vector<std::string_view> keys;
  keys.reserve(Keys_.size());
  for (const auto &key : Keys_) { keys.emplace_back(key); }
  std::string error;
  if (!Pages_.ActivateProducts(0, SourceKey_, Expected_, keys, keys, error)) {
    return std::unexpected(std::move(error));
  }
  PublicationMs_ +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  return {};
}

size_t RoadGeometryPublication::HeapBytes() const noexcept {
  size_t bytes = Expected_.CapacityBytes() + Keys_.capacity() * sizeof(std::string) +
                 PackingKey_.capacity() + Packed_.capacity() * sizeof(StoredVertex) + sizeof(Mat4);
  for (const auto &key : Keys_) { bytes += key.capacity(); }
  return bytes;
}
}
