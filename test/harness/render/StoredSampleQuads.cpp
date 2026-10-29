#include "StoredSampleQuads.h"
#include "ImpostorAtlas.h"
#include "ImpostorViewFrame.h"
#include "ImpostorCaptureMaterial.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Test {
namespace {
constexpr size_t kSampleMeshBytes = size_t{4} * (3 + 3 + 2) * sizeof(float) + 6 * sizeof(uint32_t);
constexpr size_t kSampleImageBytes = size_t{2} * 4 * sizeof(float);

struct SampleMesh {
  std::vector<float> Positions, Normals, Uv;
  std::vector<uint32_t> Indices;

  void Reserve(size_t pixels) {
    Positions.reserve(pixels * 12);
    Normals.reserve(pixels * 12);
    Uv.reserve(pixels * 8);
    Indices.reserve(pixels * 6);
  }
};

std::expected<std::array<int, 2>, std::string>
Images(Geometry &geometry, const ImpostorAtlas &atlas, const ImpostorAtlas::View &view) {
  std::array<std::vector<uint8_t>, 2> images;
  for (auto &image : images) { image.resize(view.Texels.size() * 4 * sizeof(float)); }
  for (size_t at = 0; at < view.Texels.size(); ++at) {
    const auto &texel = view.Texels[at];
    if (texel.Surface == 0) { continue; }
    const auto &material = atlas.Surfaces()[texel.Surface - 1];
    const auto sample = view.Materials.empty()
                            ? ImpostorAtlas::SurfaceSample{.BaseColour = {{material.BaseColour[0],
                                                                           material.BaseColour[1],
                                                                           material.BaseColour[2]}},
                                                           .Roughness = material.Roughness,
                                                           .Metalness = material.Metalness}
                            : view.Materials[at];
    const std::array<float, 4> colour{
        sample.BaseColour[0], sample.BaseColour[1], sample.BaseColour[2], 1};
    const std::array<float, 4> parameters{1, sample.Roughness, sample.Metalness, 1};
    const size_t offset = at * sizeof(colour);
    std::memcpy(images[0].data() + offset, colour.data(), sizeof(colour));
    std::memcpy(images[1].data() + offset, parameters.data(), sizeof(parameters));
  }
  std::array<int, 2> output{};
  for (size_t at = 0; at < images.size(); ++at) {
    auto image = geometry.addImage({.WidthPx = atlas.Pixels(),
                                    .HeightPx = atlas.Pixels(),
                                    .Format = ImagePixelFormat::Rgba32Float,
                                    .Rgba = images[at]});
    if (!image) { return std::unexpected("material samples cannot own their image data"); }
    output[at] = *image;
  }
  return output;
}

void Append(SampleMesh &mesh,
            const ImpostorAtlas &atlas,
            const ImpostorAtlas::View &view,
            const ImpostorViewFrame &frame,
            size_t at) {
  const auto first = static_cast<uint32_t>(mesh.Positions.size() / 3);
  const auto pixels = static_cast<size_t>(atlas.Pixels());
  const size_t x = at % pixels;
  const size_t y = at / pixels;
  const auto &sample = view.Texels[at];
  const std::array<std::pair<size_t, size_t>, 4> corners{
      {{x, y + 1}, {x + 1, y + 1}, {x + 1, y}, {x, y}}};
  const double extent = atlas.HalfExtentM();
  for (const auto [column, row] : corners) {
    const double u = static_cast<double>(column) / static_cast<double>(pixels);
    const double v = static_cast<double>(row) / static_cast<double>(pixels);
    const auto point = atlas.CentreM() + frame.Right * ((2 * u - 1) * extent) +
                       frame.Up * ((1 - 2 * v) * extent) +
                       view.TowardEye * ((4.0 * sample.Depth - 2.0) * extent);
    for (size_t axis = 0; axis < 3; ++axis) {
      mesh.Positions.push_back(static_cast<float>(point[axis]));
      mesh.Normals.push_back(sample.Normal[axis]);
    }
    mesh.Uv.push_back(static_cast<float>(u));
    mesh.Uv.push_back(static_cast<float>(v));
  }
  for (const uint32_t offset : {0u, 1u, 2u, 0u, 2u, 3u}) { mesh.Indices.push_back(first + offset); }
}

std::expected<void, std::string> Parts(Geometry &geometry,
                                       const ImpostorAtlas &atlas,
                                       const std::vector<SampleMesh> &meshes,
                                       const std::array<int, 2> &images) {
  for (size_t at = 0; at < meshes.size(); ++at) {
    const auto &mesh = meshes[at];
    if (mesh.Indices.empty()) { continue; }
    auto material = CapturedImpostorMaterial(atlas.Surfaces()[at]);
    if (!material) { return std::unexpected("material samples require captured PBR materials"); }
    material->BaseColour = {{1, 1, 1, 1}};
    material->Metalness = material->Roughness = 1;
    const std::array maps{&material->BaseColourMap, &material->MetalRoughMap};
    for (size_t map = 0; map < maps.size(); ++map) {
      maps[map]->Image = images[map];
      maps[map]->Sampler = {.Magnify = Filter::Nearest,
                            .Minify = Filter::Nearest,
                            .Mip = MipFilter::None,
                            .WrapU = Wrap::ClampToEdge,
                            .WrapV = Wrap::ClampToEdge};
    }
    const auto surface = geometry.addSurface("sample", *material);
    if (!surface) { return std::unexpected("material samples have an invalid material"); }
    const auto part = geometry.addPart("samples", *surface);
    if (!part || !geometry.setPositions(*part, mesh.Positions) ||
        !geometry.setNormals(*part, mesh.Normals) || !geometry.setTexture(*part, mesh.Uv) ||
        !geometry.setTriangles(*part, mesh.Indices)) {
      return std::unexpected("material samples cannot own their native geometry");
    }
  }
  return {};
}
}

std::expected<StoredSampleQuads, std::string> StoredSampleQuads::Prepare(const ImpostorAtlas &atlas,
                                                                         Request request) {
  if (request.View >= atlas.Views().size()) {
    return std::unexpected("material sample view unavailable");
  }
  const auto &view = atlas.Views()[request.View];
  const auto frame = ImpostorViewFrame::From(view.TowardEye);
  if (!frame || view.Texels.size() > request.SourceBytesMost / kSampleImageBytes) {
    return std::unexpected("material samples exceed their valid source image admission");
  }
  const size_t imageBytes = view.Texels.size() * kSampleImageBytes;
  const auto covered = static_cast<size_t>(
      std::ranges::count_if(view.Texels, [](const auto &sample) { return sample.Surface != 0; }));
  if (covered == 0 || covered > (request.SourceBytesMost - imageBytes) / kSampleMeshBytes ||
      covered > std::numeric_limits<uint32_t>::max() / 4) {
    return std::unexpected("material samples exceed their valid source geometry admission");
  }
  std::vector<size_t> counts(atlas.Surfaces().size());
  for (const auto &sample : view.Texels) {
    if (sample.Surface != 0) { ++counts[sample.Surface - 1]; }
  }
  std::vector<SampleMesh> meshes(counts.size());
  for (size_t at = 0; at < meshes.size(); ++at) { meshes[at].Reserve(counts[at]); }
  for (size_t at = 0; at < view.Texels.size(); ++at) {
    if (view.Texels[at].Surface != 0) {
      Append(meshes[view.Texels[at].Surface - 1], atlas, view, *frame, at);
    }
  }
  StoredSampleQuads samples;
  samples.SourceBytes = imageBytes + covered * kSampleMeshBytes;
  const auto images = Images(samples.Source, atlas, view);
  if (!images) { return std::unexpected(images.error()); }
  if (auto parts = Parts(samples.Source, atlas, meshes, *images); !parts) {
    return std::unexpected(parts.error());
  }
  auto &camera = samples.Capture;
  camera.EyeM = atlas.CentreM() + view.TowardEye * (3 * atlas.HalfExtentM());
  camera.Right = frame->Right;
  camera.Up = frame->Up;
  camera.Forward = view.TowardEye * -1.0;
  camera.Kind = Render::CameraKind::Orthographic;
  camera.XMagM = camera.YMagM = atlas.HalfExtentM();
  camera.ZNearM = atlas.HalfExtentM();
  camera.ZFarM = 5 * atlas.HalfExtentM();
  return samples;
}
}
