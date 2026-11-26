#include "MetalProductBenchmark.h"
#include "BenchmarkOptions.h"
#include <expected>
#include <string>

#ifdef __APPLE__
#include "AssetCache.h"
#include "BenchmarkInput.h"
#include "BenchmarkMotion.h"
#include "BenchmarkReport.h"
#include "FarPanoramaAsset.h"
#include "GeodeticCamera.h"
#include "GroupImpostorAsset.h"
#include "ImpostorViewDemand.h"
#include "MetalProductRenderer.h"
#include "SurfaceTileAsset.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <numbers>
#include <string_view>
#include <optional>
#include <span>
#include <system_error>
#include <utility>
#include <vector>
#endif

namespace outshine::App {
#ifdef __APPLE__
namespace {
constexpr double kPoseRoundoffM = 1e-6;

std::expected<Camera, std::string> TileCamera(const Scenario::Document &document,
                                              std::string_view selected,
                                              LongitudeLatitude origin) {
  const auto view = std::ranges::find(document.Views, selected, &Scenario::View::Id);
  if (view == document.Views.end() || view->Placement != Scenario::CameraPlacement::Geodetic ||
      view->Geographic.SamplesHeight || view->Sees.LooksAt || view->OffsetM != Vec3{}) {
    return std::unexpected(
        "Metal product capture requires a geographic camera with absolute height, "
        "bearing and pitch");
  }
  const auto position = ResolveGeodeticCamera(*view, origin, nullptr);
  const auto axes = ResolveGeodeticCameraAxes(view->Geographic, origin);
  if (!position || !*position || !axes) { return std::unexpected("invalid tile capture camera"); }
  auto camera = view->Sees;
  camera.PositionM = **position;
  camera.LooksAt = true;
  camera.LookAtM = camera.PositionM + axes->Forward;
  camera.UpM = axes->Up;
  if (!camera.Orthographic) {
    if (camera.NearM == 0) { camera.NearM = Camera::kNearestM; }
    if (camera.FovDeg == 0) { camera.FovDeg = Scenario::kFovUnsaidDeg; }
  }
  return camera;
}

Mat4 TilePlacement(LongitudeLatitude tile, const TangentFrame &world) {
  const auto local = TangentFrame::At(tile);
  const auto position = RenderFrame::Of(
      world.ToLocalPosition({.LongitudeDeg = tile.LongitudeDeg, .LatitudeDeg = tile.LatitudeDeg}));
  const auto east = RenderFrame::Of(world.ToLocalDirection(local.EastEcef()));
  const auto up = RenderFrame::Of(world.ToLocalDirection(local.UpEcef()));
  const auto south = RenderFrame::Of(world.ToLocalDirection(local.NorthEcef() * -1.0));
  return {{east[0],
           east[1],
           east[2],
           0,
           up[0],
           up[1],
           up[2],
           0,
           south[0],
           south[1],
           south[2],
           0,
           position[0],
           position[1],
           position[2],
           1}};
}

void CaptureProductSamples(ScenarioBenchmarkReport &report,
                           const Render::MetalProductFrames &frames) {
  report.WindowMs = frames.WindowMs;
  report.FlushMs = frames.FlushMs;
  report.Gpu = frames.Device;
  report.GpuMemory.ResourceAllocatedBytes = frames.ResourceBytes;
  report.GpuMemory.RecommendedWorkingSetBytes = frames.RecommendedResourceBytes;
  report.GpuMemory.UnifiedMemory = frames.UnifiedMemory;
  report.GpuTiming.RenderedFrames = static_cast<double>(frames.Samples.size());
  report.Frames.reserve(frames.Samples.size());
  report.GpuSamples.reserve(frames.Samples.size());
  auto &group = report.GroupCoverage;
  if (!frames.Samples.empty()) {
    group.SurfaceEndpointTriangles = {frames.Samples.front().TileTriangles,
                                      frames.Samples.back().TileTriangles};
    group.GroupEndpointTriangles = {frames.Samples.front().GroupTriangles,
                                    frames.Samples.back().GroupTriangles};
  }
  for (size_t frame = 0; frame < frames.Samples.size(); ++frame) {
    const auto &sample = frames.Samples[frame];
    group.PeakTriangles = std::max(group.PeakTriangles, sample.GroupTriangles);
    group.PeakSurfaceAndGroupTriangles =
        std::max(group.PeakSurfaceAndGroupTriangles, sample.TileTriangles + sample.GroupTriangles);
    if (sample.BlendingGroups > 0) { ++group.BlendedFrames; }
    report.Frames.push_back({.WallMs = sample.WallMs,
                             .RenderMs = sample.RenderMs,
                             .SyncWaitMs = sample.SyncWaitMs,
                             .Refined = sample.Refined});
    if (sample.GpuMs) {
      report.GpuSamples.push_back(
          {.Device = frames.DeviceId, .Submission = frame + 1, .ElapsedMs = *sample.GpuMs});
      ++report.GpuTiming.TimedFrames;
      report.GpuTiming.TotalMs += *sample.GpuMs;
      report.GpuTiming.MaxMs = std::max(report.GpuTiming.MaxMs, *sample.GpuMs);
    } else {
      ++report.GpuTiming.UnavailableFrames;
    }
  }
}

Vec3 PanoramaOffset(const Camera &pose, const FarPanorama &panorama) {
  const auto offset = pose.PositionM - panorama.AnchorM;
  return Length(offset) > kPoseRoundoffM ? offset : Vec3{};
}

std::expected<Render::PanoramaView, std::string>
MakePanoramaView(const Camera &pose, Extent frame, const FarPanorama &panorama) {
  const double aspect = static_cast<double>(frame.WidthPx) / frame.HeightPx;
  constexpr double kBlendTexels = 8;
  Mat4 basis;
  Mat4 projection;
  if (pose.Orthographic || !pose.modelMatrix(basis) || !pose.projectionMatrix(aspect, projection)) {
    return std::unexpected("far panorama requires a valid perspective camera");
  }
  return Render::PanoramaView{
      .Forward = {{-basis[8], -basis[9], -basis[10]}},
      .RightSpan = Vec3{{basis[0], basis[1], basis[2]}} * (1.0 / projection[0]),
      .UpSpan = Vec3{{basis[4], basis[5], basis[6]}} * (1.0 / projection[5]),
      .EyeOffsetM = PanoramaOffset(pose, panorama),
      .FocalPixels = static_cast<double>(frame.HeightPx) * projection[5] * 0.5,
      .DepthScaleM = -projection[14] * 0.5,
      .DepthBias = -(1.0 + projection[10]) * 0.5,
      .Exposure = panorama.ReferenceExposure,
      .AxialNearM = panorama.AxialNearM,
      .AxialOverlapEndM = panorama.AxialOverlapEndM,
      .CoverageFadeM =
          panorama.AxialNearM * (2 * kBlendTexels) / static_cast<double>(panorama.Side)};
}

std::expected<std::array<double, 2>, std::string> PanoramaOverlap(
    const FarPanorama &first, const FarPanorama &next, const ScenarioBenchmarkReport &report) {
  const Vec3 start = report.Start.PositionM;
  const Vec3 finish = report.Finish.PositionM;
  const double length = Length(finish - start);
  if (length <= kPoseRoundoffM || !first.Domain().Covers(start) || !next.Domain().Covers(finish)) {
    return std::unexpected("panorama handoff requires a flight covered at both endpoints");
  }
  if (first.Side != next.Side || first.AxialNearM != next.AxialNearM ||
      first.AxialOverlapEndM != next.AxialOverlapEndM ||
      first.MaximumRangeM != next.MaximumRangeM ||
      first.ReferenceExposure != next.ReferenceExposure || first.Lighting != next.Lighting) {
    return std::unexpected(
        "panorama handoff requires matching resolution, clip range, exposure and lighting");
  }
  const PanoramaRay ray{.OriginM = start, .Direction = (finish - start) * (1.0 / length)};
  const double exit = first.Domain().Along(ray)[1];
  const double entry = next.Domain().Along(ray)[0];
  if (exit < entry) {
    return std::unexpected("prepared panorama domains leave a gap in the flight");
  }
  const double framesPerMetre = static_cast<double>(report.Motion.Frames) / length;
  return {{entry * framesPerMetre, exit * framesPerMetre}};
}

BenchmarkPanoramaCoverage PanoramaCoverage(const FarPanorama &panorama) {
  return {.Side = panorama.Side,
          .AnchorM = panorama.AnchorM,
          .Revision = panorama.Revision,
          .AxialNearM = panorama.AxialNearM,
          .AxialOverlapEndM = panorama.AxialOverlapEndM,
          .MaximumRangeM = panorama.MaximumRangeM,
          .MaximumTranslationM = panorama.MaximumTranslationM,
          .StoredBytes = panorama.Colour.size() + panorama.Depth.size()};
}

struct PreparedPanoramas {
  std::vector<FarPanorama> Products;
  std::optional<std::array<double, 2>> BlendWindowFrames;

  [[nodiscard]] double NextWeight(size_t frame) const {
    if (!BlendWindowFrames) { return 0; }
    const auto [begin, end] = *BlendWindowFrames;
    const auto sample = static_cast<double>(frame + 1);
    if (sample >= end) { return 1; }
    if (sample <= begin) { return 0; }
    const double t = (sample - begin) / (end - begin);
    return t * t * (3.0 - 2.0 * t);
  }
};

std::expected<PreparedPanoramas, std::string>
PreparePanoramas(const ScenarioBenchmarkOptions &options,
                 AssetCache *cache,
                 ScenarioBenchmarkReport &report,
                 LongitudeLatitude origin) {
  PreparedPanoramas prepared;
  if (cache != nullptr && options.PanoramaProduct.empty()) {
    auto indexed = LoadFarPanoramas(
        *cache, options.ProductRevision, origin, report.Start.PositionM, report.Finish.PositionM);
    if (!indexed) { return std::unexpected(indexed.error()); }
    report.IndexedPanoramaRequestedBytes = indexed->RequestedBytes;
    prepared.Products = std::move(indexed->Products);
  } else {
    for (const auto path : {options.PanoramaProduct, options.NextPanorama}) {
      if (path.empty()) { continue; }
      auto input = ReadFarPanorama(path);
      if (!input) { return std::unexpected(input.error()); }
      if (input->Origin.LongitudeDeg != origin.LongitudeDeg ||
          input->Origin.LatitudeDeg != origin.LatitudeDeg) {
        return std::unexpected("prepared panorama must share the scenario origin");
      }
      prepared.Products.push_back(std::move(*input));
    }
  }
  if (prepared.Products.empty()) { return prepared; }
  const auto &first = prepared.Products.front();
  report.Panorama = PanoramaCoverage(first);
  if (prepared.Products.size() == 2) {
    const auto &next = prepared.Products.back();
    const auto overlap = PanoramaOverlap(first, next, report);
    if (!overlap) { return std::unexpected(overlap.error()); }
    prepared.BlendWindowFrames = *overlap;
    report.PanoramaHandoff = {.Next = PanoramaCoverage(next), .BlendWindowFrames = *overlap};
  } else if (!first.Domain().Covers(report.Start.PositionM) ||
             !first.Domain().Covers(report.Finish.PositionM)) {
    return std::unexpected("camera path exceeds the prepared far panorama translation domain");
  }
  return prepared;
}

std::expected<std::unique_ptr<AssetCache>, std::string> OpenStore(std::string_view path) {
  if (path.empty()) { return nullptr; }
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error)) {
    return std::unexpected("prepared product store must be an existing database");
  }
  auto cache = AssetCache::Open(std::string(path));
  if (!cache) { return std::unexpected("could not open the prepared product store"); }
  return std::move(*cache);
}

std::expected<SurfaceTileLookup, std::string> ReadIndexedTile(AssetCache &cache,
                                                              std::string_view revision,
                                                              const Vec3 &position,
                                                              LongitudeLatitude origin) {
  const auto eye = TangentFrame::At(origin).ToEcefPosition(
      {.EastM = position[0], .NorthM = -position[2], .UpM = position[1]});
  const auto geo = Ground::EcefToGeoWgs84({.X = eye[0], .Y = eye[1], .Z = eye[2]});
  return LoadSurfaceTileAt(
      cache, revision, {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg});
}

std::expected<SurfaceCutLookup, std::string> PrepareTiles(const ScenarioBenchmarkOptions &options,
                                                          AssetCache *cache,
                                                          const ScenarioBenchmarkReport &report,
                                                          LongitudeLatitude origin,
                                                          std::span<const FarPanorama> panoramas,
                                                          bool preferCoarse = false) {
  if (cache != nullptr && !panoramas.empty() && panoramas.front().AxialNearM > 0) {
    const auto projection = MakePanoramaView(report.Start, report.Frame, panoramas.front());
    if (!projection) { return std::unexpected(projection.error()); }
    const auto frame = TangentFrame::At(origin);
    const auto earth = [&](const Vec3 &p) {
      return frame.ToEcefPosition({.EastM = p[0], .NorthM = -p[2], .UpM = p[1]});
    };
    const ImpostorEyeSweep sweep{.Begin = earth(report.Start.PositionM),
                                 .End = earth(report.Finish.PositionM)};
    Box eye;
    eye.Cover(sweep.Begin);
    eye.Cover(sweep.End);
    const double handoff = projection->AxialOverlapEndM > 0
                               ? projection->AxialOverlapEndM
                               : projection->AxialNearM + projection->CoverageFadeM;
    const double radius = std::numbers::sqrt3 * handoff;
    return LoadSurfaceCut(
        *cache,
        options.ProductRevision,
        {.SweptEyeEcef = eye,
         .RadiusM = radius + panoramas.front().MaximumTranslationM,
         .FocalPixels = preferCoarse ? 0 : projection->FocalPixels,
         .GroupDemand = GroupImpostorDemand{.EyeEcef = sweep,
                                            .FocalPixels = projection->FocalPixels,
                                            .ReplaceSurface = !preferCoarse}});
  }
  SurfaceCutLookup cut;
  if (cache != nullptr) {
    auto indexed = ReadIndexedTile(*cache, options.ProductRevision, report.Start.PositionM, origin);
    if (!indexed) { return std::unexpected(indexed.error()); }
    cut.RequestedBytes = indexed->RequestedBytes;
    if (indexed->Product) { cut.Products.push_back(std::move(*indexed->Product)); }
  } else if (!options.TileProduct.empty()) {
    auto direct = ReadSurfaceTile(options.TileProduct);
    if (!direct) { return std::unexpected(direct.error()); }
    cut.RequestedBytes = direct->StoredBytes;
    cut.Products.push_back(std::move(*direct));
  }
  return cut;
}

void BindGroupHandoffs(std::span<const SurfaceCutGroup> groups, auto &tiles) {
  for (size_t at = 0; at < groups.size(); ++at) {
    const auto &group = groups[at];
    if (group.Group.Handoff) { tiles[group.SurfaceIndex].HandoffGroup = at; }
  }
}

BenchmarkGroupCoverage DescribeGroups(const SurfaceCutLookup &cut) {
  BenchmarkGroupCoverage coverage;
  coverage.Groups = cut.Groups.size();
  coverage.RequestedBytes = cut.GroupRequestedBytes;
  for (const auto &group : cut.Groups) {
    coverage.AvailableViews += group.AvailableViews;
    coverage.ResidentViews += group.Group.Views.size();
    if (group.Group.Handoff) { ++coverage.HandoffGroups; }
  }
  return coverage;
}

std::vector<Render::MetalSurface>
PlaceTiles(const SurfaceCutLookup &cut, ScenarioBenchmarkReport &report, LongitudeLatitude origin) {
  std::vector<Render::MetalSurface> placed;
  placed.reserve(cut.Products.size());
  const auto world = TangentFrame::At(origin);
  report.TileRequestedBytes = cut.RequestedBytes;
  report.Tiles.reserve(cut.Products.size());
  for (const auto &tile : cut.Products) {
    placed.push_back(
        {.Product = tile, .WorldFromTile = TilePlacement(tile.Origin, world), .HandoffGroup = {}});
    report.Tiles.push_back(
        {.Address = {static_cast<uint32_t>(tile.Coverage.Zoom), tile.Coverage.X, tile.Coverage.Y},
         .OriginLonLat = {tile.Origin.LongitudeDeg, tile.Origin.LatitudeDeg},
         .Revision = tile.Revision,
         .Triangles = tile.Indices.size() / 3,
         .OverlayTriangles = (tile.Indices.size() - tile.GroundIndices) / 3,
         .GeometricErrorM = tile.GeometricErrorM,
         .StoredBytes = tile.StoredBytes});
  }
  BindGroupHandoffs(cut.Groups, placed);
  return placed;
}

std::vector<Render::MetalImpostorGroup> PlaceGroups(const SurfaceCutLookup &cut,
                                                    LongitudeLatitude origin) {
  std::vector<Render::MetalImpostorGroup> placed;
  placed.reserve(cut.Groups.size());
  const auto world = TangentFrame::At(origin);
  for (const auto &group : cut.Groups) {
    placed.push_back({.Views = group.Group.Views,
                      .WorldFromTile = TilePlacement(group.Origin, world),
                      .CentreM = group.Group.CentreM,
                      .HalfExtentM = group.Group.HalfExtentM,
                      .Handoff = group.Group.Handoff,
                      .FractionalCoverage = group.Group.FractionalCoverage});
  }
  return placed;
}

struct SurfaceReadCost {
  size_t RequestedBytes = 0;
  size_t DecodedBytes = 0;
  double Ms = 0;
  BenchmarkGroupCoverage Groups;
};

std::expected<Render::MetalPlacedCut, std::string>
PrepareSurfaceReplacement(const ScenarioBenchmarkOptions &options,
                          AssetCache &cache,
                          const ScenarioBenchmarkReport &report,
                          LongitudeLatitude origin,
                          std::span<const FarPanorama> panoramas,
                          std::span<const SurfaceTile> active,
                          SurfaceReadCost &cost) {
  const auto readAt = BenchmarkClock::now();
  auto cut = PrepareTiles(options, &cache, report, origin, panoramas);
  cost.Ms = BenchmarkElapsedMs(readAt);
  if (!cut) { return std::unexpected(cut.error()); }
  cost.RequestedBytes = cut->RequestedBytes;
  cost.Groups = DescribeGroups(*cut);
  for (const auto &tile : cut->Products) { cost.DecodedBytes += tile.StoredBytes; }
  const auto coverage = SurfaceCoverage(active);
  if (!coverage || SurfaceCoverage(cut->Products) != coverage) {
    return std::unexpected("replacement surface cut changed the prepared coverage");
  }
  Render::MetalPlacedCut placed;
  placed.Tiles.reserve(cut->Products.size());
  placed.Groups.reserve(cut->Groups.size());
  const auto world = TangentFrame::At(origin);
  for (auto &tile : cut->Products) {
    const auto placement = TilePlacement(tile.Origin, world);
    placed.Tiles.push_back(
        {.Product = std::move(tile), .WorldFromTile = placement, .HandoffGroup = {}});
  }
  BindGroupHandoffs(cut->Groups, placed.Tiles);
  for (auto &group : cut->Groups) {
    placed.Groups.push_back({.Views = std::move(group.Group.Views),
                             .WorldFromTile = TilePlacement(group.Origin, world),
                             .CentreM = group.Group.CentreM,
                             .HalfExtentM = group.Group.HalfExtentM,
                             .Handoff = group.Group.Handoff,
                             .FractionalCoverage = group.Group.FractionalCoverage});
  }
  return placed;
}

std::expected<Render::MetalProductView, std::string>
ProductViewAt(const BenchmarkMotion &motion,
              Extent extent,
              const PreparedPanoramas &prepared,
              bool hasTiles,
              size_t frame) {
  const double aspect = static_cast<double>(extent.WidthPx) / extent.HeightPx;
  const auto &panoramas = prepared.Products;
  const auto pose = motion.At(frame);
  if (!pose) { return std::unexpected(std::string(pose.error())); }
  Render::MetalProductView view{};
  view.EyeM = pose->PositionM;
  if (hasTiles && !pose->clipMatrix(aspect, view.ClipFromWorld)) {
    return std::unexpected("invalid tile camera");
  }
  if (!panoramas.empty()) {
    const double blend = prepared.NextWeight(frame);
    view.PanoramaIndex = blend >= 1 ? 1 : 0;
    const auto projected = MakePanoramaView(*pose, extent, panoramas[view.PanoramaIndex]);
    if (!projected) { return std::unexpected(projected.error()); }
    view.Panorama = *projected;
    if (blend > 0 && blend < 1) {
      view.Panorama.NextEyeOffsetM = PanoramaOffset(*pose, panoramas.back());
      view.Panorama.NextWeight = blend;
    }
  }
  return view;
}

std::expected<Render::MetalProductFrames, std::string>
MeasureProducts(const ScenarioBenchmarkOptions &options,
                ScenarioBenchmarkReport &report,
                const BenchmarkMotion &motion,
                LongitudeLatitude origin) {
  const auto loadAt = BenchmarkClock::now();
  const auto cache = OpenStore(options.ProductStore);
  if (!cache) { return std::unexpected(cache.error()); }
  const auto prepared = PreparePanoramas(options, cache->get(), report, origin);
  if (!prepared) { return std::unexpected(prepared.error()); }
  const auto &panoramas = prepared->Products;
  if (options.StreamProducts &&
      (!*cache || panoramas.empty() || panoramas.front().AxialNearM <= 0)) {
    return std::unexpected("streaming tiles requires a prepared store and a clipped far panorama");
  }
  auto cut = PrepareTiles(options, cache->get(), report, origin, panoramas, options.StreamProducts);
  if (!cut) { return std::unexpected(cut.error()); }
  if (options.StreamProducts && cut->Products.empty()) {
    return std::unexpected("streaming tiles requires resident fallback coverage");
  }
  report.GroupCoverage = DescribeGroups(*cut);
  const auto groups = PlaceGroups(*cut, origin);
  const auto tiles = PlaceTiles(*cut, report, origin);
  report.ProductReadMs = BenchmarkElapsedMs(loadAt);
  const auto viewAt = [&](size_t frame) {
    return ProductViewAt(motion, report.Frame, *prepared, !tiles.empty(), frame);
  };
  report.PreloadMs = BenchmarkElapsedMs(loadAt);
  SurfaceReadCost refinement;
  Render::MetalSurfaceStream stream;
  if (options.StreamProducts) {
    stream.FrameSeconds = 1.0 / report.Motion.RateHz;
    stream.Load = [&] {
      return PrepareSurfaceReplacement(
          options, **cache, report, origin, panoramas, cut->Products, refinement);
    };
  }
  auto frames =
      Render::RenderMetalProducts({.Tiles = tiles, .Panoramas = panoramas, .Groups = groups},
                                  report.Motion.Frames,
                                  viewAt,
                                  !options.Image.empty(),
                                  stream);
  if (*cache) { report.ProductStoreReadBytes = (*cache)->PackageBytesRead(); }
  if (frames && frames->Transition) {
    const auto &transition = *frames->Transition;
    report.TileRequestedBytes += refinement.RequestedBytes;
    report.ProductReadMs += refinement.Ms;
    const size_t groupRequested =
        report.GroupCoverage.RequestedBytes + refinement.Groups.RequestedBytes;
    if (transition.FirstFrame) { report.GroupCoverage = refinement.Groups; }
    report.GroupCoverage.RequestedBytes = groupRequested;
    report.SurfaceStream = {.FirstRefinedFrame = transition.FirstFrame,
                            .Tiles = transition.Tiles,
                            .ReusedTiles = transition.ReusedTiles,
                            .Triangles = transition.Triangles,
                            .DecodedBytes = refinement.DecodedBytes,
                            .ReadyMs = transition.ReadyMs,
                            .Error = transition.Error};
  }
  return frames;
}

}
#endif

std::expected<std::string, std::string>
RunMetalProductBenchmark(const ScenarioBenchmarkOptions &options) {
#ifdef __APPLE__
  ScenarioBenchmarkReport report;
  const auto setupAt = BenchmarkClock::now();
  CaptureBenchmarkHost(report, true);
  const auto document = ReadBenchmarkScenario(options, "data", report);
  if (!document) { return std::unexpected(document.error()); }
  if (report.Frame != Extent{.WidthPx = Render::MetalProductFrameWidth,
                             .HeightPx = Render::MetalProductFrameHeight}) {
    return std::unexpected("Metal product capture requires a 1280x720 scenario");
  }
  const LongitudeLatitude origin{.LongitudeDeg = document->Ground.Origin.LongitudeDeg,
                                 .LatitudeDeg = document->Ground.Origin.LatitudeDeg};
  const auto camera = TileCamera(*document, report.View, origin);
  if (!camera) { return std::unexpected(camera.error()); }
  report.Start = *camera;
  const auto motion = BenchmarkMotion::Make(report.Start, report.Motion);
  if (!motion) { return std::unexpected(std::string(motion.error())); }
  const auto finish = motion->At(report.Motion.Frames - 1);
  if (!finish) { return std::unexpected(std::string(finish.error())); }
  report.Finish = *finish;
  report.SetupMs = BenchmarkElapsedMs(setupAt);

  const auto frames = MeasureProducts(options, report, *motion, origin);
  if (!frames) { return std::unexpected(frames.error()); }
  report.ProductUploadBytes = frames->UploadedBytes;
  report.ProductUploadMs =
      frames->UploadMs + (frames->Transition ? frames->Transition->UploadMs : 0);
  report.SetupMs += frames->SetupMs;
  report.PreloadMs += frames->UploadMs;
  CaptureProductSamples(report, *frames);
  if (!options.Image.empty()) {
    const auto imageAt = BenchmarkClock::now();
    if (const auto saved = SaveBenchmarkImage(report, frames->Rgba, options.Image); !saved) {
      return std::unexpected(saved.error());
    }
    report.ImageMs = BenchmarkElapsedMs(imageAt);
  }
  const auto hardwareAt = BenchmarkClock::now();
  CaptureBenchmarkHost(report, false);
  report.HardwareMs = BenchmarkElapsedMs(hardwareAt);
  return ScenarioBenchmarkJson(report);
#else
  static_cast<void>(options);
  return std::unexpected("product capture requires the Metal backend");
#endif
}

}
