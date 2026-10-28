#include "ImpostorBaker.h"
#include "ImpostorOptics.h"
#include "ImpostorSurface.h"
#include "ImpostorViewAssets.h"
#include "ImpostorViewJob.h"
#include "AssetPipeline.h"
#include "AssetPublication.h"
#include "Tasks.h"
#include "BuildingSourceCapture.h"
#include "BuildingRadianceField.h"
#include "NativeMaterialMemory.h"
#include "ImpostorInstances.h"
#include "WorldCandidate.h"
#include "AssetCache.h"
#include "Sha256.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <optional>
#include <numbers>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <stop_token>
#include <thread>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
namespace Fs = std::filesystem;

[[noreturn]] void Fail(const std::string &reason) {
  std::println(stderr, "{}", reason);
  std::_Exit(1);
}

double Milliseconds(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

Vec3 Vector(const Json &value) {
  return {{value.at(0).get<double>(), value.at(1).get<double>(), value.at(2).get<double>()}};
}

template <class T> std::vector<T> Read(const Fs::path &path, size_t count) {
  if (count == 0 || count > (size_t{64} * 1024 * 1024) / sizeof(T)) {
    Fail("invalid input extent");
  }
  std::vector<T> data(count);
  std::ifstream file(path, std::ios::binary);
  file.read(reinterpret_cast<char *>(data.data()),
            static_cast<std::streamsize>(data.size() * sizeof(T)));
  if (!file || file.peek() != std::char_traits<char>::eof()) {
    Fail("invalid input: " + path.string());
  }
  return data;
}

template <class T> void Write(const Fs::path &path, std::span<const T> data) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(data.data()),
             static_cast<std::streamsize>(data.size_bytes()));
  if (!file) { Fail("write failed: " + path.string()); }
}

Geometry Source(const Fs::path &root, const Json &input) {
  Geometry mesh;
  const std::array<Vec4f, 3> base{
      {{{0.74f, 0.71f, 0.65f, 1}}, {{0.42f, 0.20f, 0.14f, 1}}, {{0.74f, 0.71f, 0.65f, 1}}}};
  for (size_t at = 0; at < input.at("parts").size(); ++at) {
    const auto &record = input.at("parts").at(at);
    const size_t vertices = record.at("vertices").get<size_t>();
    const size_t triangles = record.at("triangles").get<size_t>();
    const auto directory = root / std::to_string(at);
    const size_t role = record.value("material", at);
    Material material;
    material.BaseColour = base.at(role);
    material.Roughness = role == 1 ? 0.72f : 0.88f;
    material.Pattern = role == 2 ? SurfacePattern::FacadeMetres : SurfacePattern::Facade;
    const auto surface = mesh.addSurface("native-source", material);
    if (!surface) { Fail("source material rejected"); }
    const auto part = mesh.addPart("native-part", *surface);
    if (!part ||
        !mesh.setPositions(*part, Read<float>(directory / "positions.bin", vertices * 3)) ||
        !mesh.setNormals(*part, Read<float>(directory / "normals.bin", vertices * 3)) ||
        !mesh.setTexture(*part, Read<float>(directory / "uv.bin", vertices * 2)) ||
        !mesh.setTexture(*part, Read<float>(directory / "uv1.bin", vertices * 2), 1) ||
        !mesh.setColours(*part, Read<float>(directory / "colours.bin", vertices * 4)) ||
        !mesh.setTriangles(*part, Read<uint32_t>(directory / "indices.bin", triangles * 3))) {
      Fail("source attributes rejected");
    }
  }
  return mesh;
}

int FilterSamples(const Json &query) {
  const int samples = query.value("filter_axis_samples", 4);
  if (samples < 1 || samples > 128) { Fail("filter samples require 1..128 per axis"); }
  return samples;
}

void Filtered(Render::SceneRenderer &renderer,
              const Render::Viewpoint &camera,
              const Fs::path &output,
              int samplesAxis) {
  constexpr size_t pixels = size_t{1280} * 720;
  std::vector<float> average(pixels * 4);
  auto lens = Render::Lens::From(camera, 1280, 720);
  if (!lens) { Fail("filtered camera has no projection"); }
  const auto basis = renderer.LastSubmittedCamera().Basis;
  std::vector<float> values;
  const auto sampleCount = static_cast<float>(samplesAxis * samplesAxis);
  for (int y = 0; y < samplesAxis; ++y) {
    for (int x = 0; x < samplesAxis; ++x) {
      const double dx = (static_cast<double>(x) + 0.5) / samplesAxis - 0.5;
      const double dy = (static_cast<double>(y) + 0.5) / samplesAxis - 0.5;
      const double focal = 360 / std::tan(camera.YfovRad * 0.5);
      auto sampled = basis;
      sampled.Forward = basis.Forward + basis.Right * (dx / focal) + basis.Up * (dy / focal);
      if (!Normalise(sampled.Forward)) { Fail("sampled forward has no direction"); }
      sampled.Right = Cross(sampled.Forward, basis.Up);
      if (!Normalise(sampled.Right)) { Fail("sampled screen axes are singular"); }
      sampled.Up = Cross(sampled.Right, sampled.Forward);
      renderer.SetCamera(sampled, *lens);
      if (auto rendered = renderer.RenderFrame(); !rendered) { Fail(rendered.error()); }
      if (renderer.ReadSceneLinear(values) != Render::ReadState::Ready ||
          values.size() != average.size()) {
        Fail("filtered native colour readback incomplete");
      }
      for (size_t at = 0; at < average.size(); ++at) { average[at] += values[at] / sampleCount; }
    }
  }
  Write<float>(output.string() + ".ss" + std::to_string(samplesAxis * samplesAxis), average);
}

std::unique_ptr<Render::ImpostorInstances> PublishCard(Render::SceneRenderer &renderer,
                                                       std::unique_ptr<Core::RuntimeScene> &scene,
                                                       const ImpostorCards &cards,
                                                       const Vec3 &eye) {
  Core::WorldCandidate candidate(renderer);
  if (const auto prepared = candidate.Prepare(*scene, nullptr); !prepared) {
    Fail(prepared.error());
  }
  std::string error;
  if (!candidate.SetGeometry(Geometry{}, 0, error)) { Fail(error); }
  std::unique_ptr<Render::ImpostorInstances> instances;
  {
    const auto edit = renderer.EditWorldCandidate();
    instances = Render::ImpostorInstances::Create(renderer, cards, 1, error);
    const Mat4 model;
    if (!instances || !instances->Update(std::span(&model, 1), eye, error)) { Fail(error); }
  }
  if (const auto published = candidate.Publish(scene); !published) { Fail(published.error()); }
  return instances;
}

Json MeshStatistics(const Geometry &mesh) {
  size_t triangles = 0;
  size_t vertices = 0;
  size_t imageBytes = 0;
  for (int part = 0; part < mesh.parts(); ++part) {
    triangles += mesh.trianglesOf(part).size() / 3;
    vertices += mesh.positionsOf(part).size() / 3;
  }
  for (int image = 0; image < mesh.images(); ++image) {
    const auto data = mesh.imageAt(image);
    imageBytes += data.Rgba.size();
    for (const auto &mip : data.LowerMips) {
      if (mip) { imageBytes += mip->size(); }
    }
  }
  return {{"images", mesh.images()},
          {"image_storage_bytes", imageBytes},
          {"triangles", triangles},
          {"vertices", vertices}};
}

Render::GpuSubmission TimedSubmission(size_t &creations) {
  Render::GpuSubmission submission;
  submission.Context = &creations;
  submission.Create = [](void *context, Render::Native::DeviceOptions options) {
    ++*static_cast<size_t *>(context);
    options.GpuTiming = Render::Native::GpuTimingMode::Submission;
    return Render::Native::CreateDevice(options);
  };
  return submission;
}

Json DrawWith(Render::SceneRenderer &renderer,
              const Geometry &mesh,
              const Json &query,
              const Fs::path &output,
              const ImpostorCards *cards = nullptr) {
  const int samplesAxis = FilterSamples(query);
  const auto began = Clock::now();
  Core::Declaration declaration;
  declaration.InitialGeometry = mesh.parts() > 0 ? &mesh : nullptr;
  declaration.SurfaceWidthPx = 1280;
  declaration.SurfaceHeightPx = 720;
  declaration.KeyLux = 100;
  declaration.KeyElevationDeg = query.value("sun_elevation_deg", 0.0);
  declaration.Haze = 0;
  declaration.Exposure = 1;
  declaration.Stages = {"opaqueGeometry"};
  if (cards != nullptr) {
    declaration.Stages = {"opaqueGeometry", "transmissiveGeometry", "compositeTransmission"};
  }
  declaration.Outputs = {"sceneLinear", "sceneDepth", "sceneShadingNormal"};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) { Fail(error); }
  Render::Viewpoint camera;
  camera.EyeM = Vector(query.at("eye"));
  camera.Right = Vector(query.at("basis").at(0));
  camera.Up = Vector(query.at("basis").at(1));
  camera.Forward = Vector(query.at("basis").at(2));
  camera.YfovRad = 2 * std::atan(360.0 / query.at("focal_px").get<double>());
  camera.ZNearM = 0.1;
  scene->Eye(camera);
  if (!scene->Draw(error)) { Fail(error); }
  const auto instances =
      cards != nullptr ? PublishCard(renderer, scene, *cards, camera.EyeM) : nullptr;
  if (instances) {
    scene->Eye(camera);
    if (!scene->Draw(error)) { Fail(error); }
  }
  renderer.WaitForGpu();
  const double setupMs = Milliseconds(began);
  renderer.ResetMeasurements();
  renderer.KeepGpuSamples(24);
  for (size_t frame = 0; frame < 24; ++frame) {
    if (auto rendered = renderer.RenderFrame(); !rendered) { Fail(rendered.error()); }
    renderer.WaitForGpu();
  }
  std::vector<float> colour;
  std::vector<float> depth;
  std::vector<float> normal;
  std::vector<uint8_t> rgba;
  if (renderer.ReadSceneLinear(colour) != Render::ReadState::Ready ||
      renderer.ReadDepth(depth) != Render::ReadState::Ready ||
      renderer.ReadShadingNormal(normal) != Render::ReadState::Ready ||
      renderer.ReadPixels(rgba) != Render::ReadState::Ready) {
    Fail("incomplete readback");
  }
  Write<float>(output.string() + ".linear", colour);
  Write<float>(output.string() + ".depth", depth);
  Write<uint8_t>(output.string() + ".rgba", rgba);
  Json samples = Json::array();
  for (const auto &sample : renderer.GpuSamples()) {
    if (!std::isfinite(sample.ElapsedMs) || sample.ElapsedMs <= 0) { Fail("invalid GPU timing"); }
    samples.push_back(sample.ElapsedMs);
  }
  if (samples.size() != 24) { Fail("missing GPU samples"); }
  Filtered(renderer, camera, output, samplesAxis);
  const auto &drawn = cards != nullptr ? cards->Views.front().Surface : mesh;
  std::array<double, 3> normalSum{};
  for (size_t pixel = 0; pixel < depth.size(); ++pixel) {
    if (depth[pixel] == 0) { continue; }
    for (size_t axis = 0; axis < 3; ++axis) { normalSum[axis] += normal[pixel * 4 + axis]; }
  }
  Json result = MeshStatistics(drawn);
  result.update({{"normal_sum", normalSum},
                 {"setup_ms", setupMs},
                 {"device", renderer.GpuName()},
                 {"gpu_ms", samples},
                 {"filter_axis_samples", samplesAxis}});
  return result;
}

Json Draw(const Geometry &mesh,
          const Json &query,
          const Fs::path &output,
          const ImpostorCards *cards = nullptr) {
  size_t creations = 0;
  Render::SceneRenderer renderer{TimedSubmission(creations)};
  return DrawWith(renderer, mesh, query, output, cards);
}

Json CaptureStatistics(const ImpostorAtlas &atlas) {
  Json result = Json::array();
  for (const auto &view : atlas.Views()) {
    size_t covered = 0;
    Vec3 base{};
    Vec3 normal{};
    for (size_t pixel = 0; pixel < view.Texels.size(); ++pixel) {
      if (view.Texels[pixel].Surface == 0) { continue; }
      ++covered;
      for (size_t axis = 0; axis < 3; ++axis) {
        base[axis] += view.Materials[pixel].BaseColour[axis];
        normal[axis] += view.Texels[pixel].Normal[axis];
      }
    }
    result.push_back({{"covered", covered},
                      {"base_sum", {base[0], base[1], base[2]}},
                      {"normal_sum", {normal[0], normal[1], normal[2]}}});
  }
  return result;
}

void ApplyMomentImages(Geometry &surface, const ImpostorAtlas &atlas, size_t view) {
  const auto id = surface.materialOf(0);
  auto material = surface.surfaceAt(id);
  if (!Render::AddOpticalCardImages(surface, material, atlas, atlas.Views()[view])) {
    Fail("source atlas has no moment images");
  }
  material.Alpha = AlphaMode::Blended;
  if (!surface.setSurface(id, material)) { Fail("moment material rejected"); }
}

Geometry
MomentSurface(const ImpostorAtlas &atlas, size_t view, Render::ImpostorSurfaceDetail detail) {
  if (detail == Render::ImpostorSurfaceDetail::Flat) {
    auto surface = Render::BuildFractionalImpostorSurface(atlas, view);
    if (!surface) { Fail("source atlas has no fractional surface"); }
    return std::move(*surface);
  }
  auto surface = Render::BuildImpostorSurface(atlas, view, detail);
  if (!surface) { Fail("source atlas has no moment geometry"); }
  ApplyMomentImages(*surface, atlas, view);
  return std::move(*surface);
}

ImpostorAtlas ExtendedDepth(const ImpostorAtlas &atlas, size_t selected) {
  auto view = atlas.Views()[selected];
  const size_t count = view.Texels.size();
  const auto width = static_cast<size_t>(atlas.Pixels());
  std::vector<size_t> owner(count, count);
  std::vector<size_t> queue;
  queue.reserve(count);
  for (size_t at = 0; at < count; ++at) {
    if (view.Texels[at].Surface == 0) { continue; }
    owner[at] = at;
    queue.push_back(at);
  }
  if (queue.empty()) { Fail("extended depth has no source"); }
  for (size_t next = 0; next < queue.size(); ++next) {
    const size_t at = queue[next];
    const auto extend = [&](size_t to) {
      if (owner[to] != count) { return; }
      owner[to] = owner[at];
      queue.push_back(to);
    };
    if (at % width > 0) { extend(at - 1); }
    if (at % width + 1 < width) { extend(at + 1); }
    if (at >= width) { extend(at - width); }
    if (at + width < count) { extend(at + width); }
  }
  const auto &original = atlas.Views()[selected];
  for (size_t at = 0; at < count; ++at) {
    view.Texels[at] = original.Texels[owner[at]];
    if (!view.Materials.empty()) { view.Materials[at] = original.Materials[owner[at]]; }
  }
  std::string error;
  auto extended = ImpostorAtlas::Create(atlas.Pixels(),
                                        atlas.CentreM(),
                                        atlas.HalfExtentM(),
                                        atlas.Surfaces(),
                                        {std::move(view)},
                                        error);
  if (!extended) { Fail(error); }
  return std::move(*extended);
}

Geometry SupportedMomentSurface(const ImpostorAtlas &atlas, size_t view) {
  const auto extended = ExtendedDepth(atlas, view);
  auto surface = Render::BuildImpostorSurface(extended, 0, Render::ImpostorSurfaceDetail::Depth);
  if (!surface) { Fail("extended depth has no geometry"); }
  ApplyMomentImages(*surface, atlas, view);
  return std::move(*surface);
}

void CollapseDepth(Geometry &surface, const ImpostorAtlas &atlas, size_t view) {
  const Vec3 toward = atlas.Views()[view].TowardEye;
  for (int part = 0; part < surface.parts(); ++part) {
    const auto source = surface.positionsOf(part);
    std::vector<float> positions(source.begin(), source.end());
    for (size_t at = 0; at < positions.size(); at += 3) {
      Vec3 point{{positions[at], positions[at + 1], positions[at + 2]}};
      point = point - toward * Dot(point - atlas.CentreM(), toward);
      for (size_t axis = 0; axis < 3; ++axis) {
        positions[at + axis] = static_cast<float>(point[axis]);
      }
    }
    if (!surface.setPositions(part, positions)) { Fail("collapsed geometry rejected"); }
  }
}

Json ClippedFlatControl(const ImpostorAtlas &atlas,
                        size_t selected,
                        const Json &query,
                        const Fs::path &output) {
  const auto began = Clock::now();
  auto surface = MomentSurface(atlas, selected, Render::ImpostorSurfaceDetail::Depth);
  CollapseDepth(surface, atlas, selected);
  const double productMs = Milliseconds(began);
  auto record = Draw(surface, query, output);
  record["product_ms"] = productMs;
  return record;
}

struct BuildingViewSource {
  Fs::path AtlasFile;
  std::string AtlasDigest;
  std::string AtlasProvenance;
  Fs::path SourceInput;
  Json Recipe = nullptr;
  std::unique_ptr<Test::BuildingSourceCapture> Producer = nullptr;
  std::optional<size_t> CurrentView = std::nullopt;
  Json Captures = Json::array();
  std::optional<ImpostorAtlas> CurrentAtlas = std::nullopt;
  size_t ReadBytes = 0;
  size_t Decodes = 0;
  double ReadMs = 0;
  double DecodeMs = 0;
  double HeaderMs = 0;

  [[nodiscard]] std::string Identity() const {
    if (Recipe.is_object()) {
      Json content = Json::object();
      for (const auto *const key :
           {"parent", "cells", "east", "north", "up", "origin", "pixels", "directions"}) {
        if (!Recipe.contains(key)) { Fail("source recipe is incomplete"); }
        content[key] = Recipe.at(key);
      }
      auto cells = content.at("cells").get<std::vector<uint32_t>>();
      std::ranges::sort(cells);
      cells.erase(std::ranges::unique(cells).begin(), cells.end());
      content["cells"] = cells;
      return "native-source-group-views-1/" + Sha256Hex(content.dump());
    }
    return AtlasProvenance + "/" + AtlasDigest;
  }

  Test::BuildingSourceCapture &RequireInputs() {
    if (!Producer) {
      const auto began = Clock::now();
      Producer = std::make_unique<Test::BuildingSourceCapture>(SourceInput, Recipe);
      HeaderMs += Milliseconds(began);
    }
    return *Producer;
  }

  Content::ImpostorViewAssets::Description Description() {
    if (Recipe.is_object()) {
      const auto &source = RequireInputs();
      return {.Centre = source.Projection().Centre,
              .HalfExtentM = source.Projection().HalfExtentM,
              .Pixels = Recipe.at("pixels").get<uint32_t>(),
              .Directions = source.Metadata().ViewDirections};
    }
    const auto &atlas = Require(0);
    Content::ImpostorViewAssets::Description description{.Centre = atlas.CentreM(),
                                                         .HalfExtentM = atlas.HalfExtentM(),
                                                         .Pixels =
                                                             static_cast<uint32_t>(atlas.Pixels()),
                                                         .Directions = {}};
    for (const auto &view : atlas.Views()) { description.Directions.push_back(view.TowardEye); }
    return description;
  }

  const ImpostorAtlas &Require(size_t view) {
    if (Recipe.is_object()) {
      if (!CurrentAtlas || CurrentView != view) {
        CurrentAtlas = RequireInputs().Capture(view);
        CurrentView = view;
        Captures.push_back({{"view", view}, {"work", RequireInputs().Report().at("views")}});
      }
      if (!CurrentAtlas) { Fail("selected source capture unavailable"); }
      return *CurrentAtlas;
    }
    if (!CurrentAtlas) {
      const auto began = Clock::now();
      std::error_code fileError;
      const auto size = Fs::file_size(AtlasFile, fileError);
      if (fileError) { Fail("source atlas unavailable: " + fileError.message()); }
      const auto bytes = Read<uint8_t>(AtlasFile, size);
      if (Sha256Hex(bytes.data(), bytes.size()) != AtlasDigest) {
        Fail("source atlas digest mismatch");
      }
      ReadMs += Milliseconds(began);
      const auto decodeAt = Clock::now();
      std::string error;
      CurrentAtlas = ImpostorAtlas::Decode(bytes, AtlasProvenance, error);
      if (!CurrentAtlas) { Fail(error); }
      ReadBytes += bytes.size();
      ++Decodes;
      DecodeMs += Milliseconds(decodeAt);
    }
    return *CurrentAtlas;
  }
};

Content::ImpostorViewAssets
OpenViewAssets(AssetCache &cache, BuildingViewSource &source, Box bounds) {
  using Content::ImpostorViewAssets;
  const auto provenance = source.Identity();
  auto opened = ImpostorViewAssets::Open(cache, provenance);
  if (!opened) { Fail("native world metadata read failed"); }
  if (*opened) { return std::move(**opened); }
  auto published = ImpostorViewAssets::Publish(cache, provenance, source.Description(), bounds);
  if (!published) { Fail("native world metadata publication failed"); }
  return std::move(*published);
}

ImpostorAtlas ResolveSourceField(const Content::ImpostorViewAssets &products,
                                 BuildingViewSource &source,
                                 size_t view,
                                 bool &hit) {
  constexpr size_t limit = size_t{64} * 1024 * 1024;
  auto loaded = products.ResolveField(
      {view},
      limit,
      [&](Content::ImpostorViewAssets::ViewIndex selected,
          size_t budget) -> std::expected<ImpostorAtlas, AssetCacheError> {
        const auto bound = ImpostorAtlas::EncodedBytesMost(
            {.Pixels = static_cast<int>(products.Metadata().Pixels), .Views = 1});
        if (!bound || *bound > budget) {
          return std::unexpected(AssetCacheError::CapacityExceeded);
        }
        hit = false;
        const auto &atlas = source.Require(selected.Value);
        const size_t local = source.Recipe.is_object() ? 0 : selected.Value;
        std::string error;
        auto field = ImpostorAtlas::Create(atlas.Pixels(),
                                           atlas.CentreM(),
                                           atlas.HalfExtentM(),
                                           atlas.Surfaces(),
                                           {atlas.Views()[local]},
                                           error);
        if (!field) { return std::unexpected(AssetCacheError::InvalidInput); }
        return std::move(*field);
      });
  if (!loaded) { Fail("required source material field unavailable"); }
  return std::move(*loaded);
}

void RadianceViews(const Fs::path &root, const Json &input) {
  BuildingViewSource source{
      .AtlasFile = root / "atlas.bin",
      .AtlasDigest = input.value("atlas_sha256", std::string{}),
      .AtlasProvenance = input.value("atlas_provenance", std::string("native-original-impostor-1")),
      .SourceInput = input.value("source_input", std::string{}),
      .Recipe = input.value("source_request", Json(nullptr))};
  auto opened = AssetCache::Open((root / "world-view-assets.sqlite").string());
  if (!opened) { Fail("material field database unavailable"); }
  const auto products = OpenViewAssets(
      **opened, source, {.Min = Vector(input.at("least")), .Max = Vector(input.at("most"))});
  const auto &metadata = products.Metadata();
  size_t writes = 0;
  Geometry original;
  if (input.contains("field_original")) {
    const Fs::path path(input.at("field_original").get<std::string>());
    std::ifstream file(path);
    const auto document = Json::parse(file, nullptr, false);
    if (!document.is_object()) { Fail("original field geometry unavailable"); }
    original = Source(path.parent_path(), document);
  }
  size_t creations = 0;
  Render::SceneRenderer renderer{TimedSubmission(creations)};
  Json report{{"mode", "radiance-world"}, {"cases", Json::array()}};
  for (const auto &query : input.at("queries")) {
    const Vec3 toward = Vector(query.at("eye")) - metadata.Centre;
    size_t view = 0;
    for (size_t at = 1; at < metadata.Directions.size(); ++at) {
      if (Dot(toward, metadata.Directions[at]) > Dot(toward, metadata.Directions[view])) {
        view = at;
      }
    }
    bool hit = true;
    const auto began = Clock::now();
    const auto atlas = ResolveSourceField(products, source, view, hit);
    const double loadMs = Milliseconds(began);
    if (!hit) { ++writes; }
    auto field = Test::PrepareBuildingRadiance(
        renderer, atlas, 0, query, original.parts() > 0 ? &original : nullptr);
    if (!field) { Fail(field.error()); }
    const auto name = query.at("name").get<std::string>();
    Write<float>(root / (name + "-source-field.f32"), field->SourceLinear);
    auto result =
        DrawWith(renderer, Geometry{}, query, root / (name + "-radiance-world"), &field->Cards);
    result.update({{"query", name},
                   {"selected_view", view},
                   {"field_prepare_ms", field->PrepareMs},
                   {"field_gpu_ms", field->GpuMs},
                   {"field_setup_gpu_ms", field->SetupGpuMs},
                   {"field_hit", hit},
                   {"field_load_ms", loadMs},
                   {"field_asset_key", products.FieldKey({view})}});
    const auto allocations = renderer.FrameGraphAllocationCounts();
    result["framegraph_allocations"] = {{"textures", allocations.Textures},
                                        {"buffers", allocations.Buffers},
                                        {"samplers", allocations.Samplers}};
    result["geometry_held_bytes"] = renderer.PieceBytesHeld();
    if (const auto bytes = Test::MaterialDeviceAllocatedBytes()) {
      result["metal_allocated_bytes"] = *bytes;
    }
    report["cases"].push_back(std::move(result));
  }
  report["render_device_creations"] = creations;
  report["field_writes"] = writes;
  report["atlas_read_bytes"] = source.ReadBytes;
  report["atlas_decodes"] = source.Decodes;
  report["captures"] = source.Captures;
  if (source.Producer) { report["source_capture"] = source.Producer->Report(); }
  std::ofstream file(root / "radiance-result.json");
  file << report.dump(2) << '\n';
  if (!file) { Fail("radiance report write failed"); }
}

ImpostorCards ResolveView(AssetPipeline &pipeline,
                          const AssetPipeline::Consumer &consumer,
                          const std::shared_ptr<AssetCache> &cache,
                          const std::shared_ptr<BuildingViewSource> &source,
                          const Content::ImpostorViewAssets &products,
                          Content::ImpostorViewAssets::Selection selected,
                          size_t limit,
                          bool &hit) {
  const auto key = products.Key(selected);
  const auto product = AssetProductKey::Parse(key);
  if (!product) { Fail("invalid selected product key"); }
  ImpostorViewJob job{
      .Cache = cache,
      .Provenance = source->Identity(),
      .ProductKey = key,
      .Selected = selected,
      .BytesMost = limit,
      .Generate = [source, &hit](ImpostorViewJob::Selection selection,
                                 size_t,
                                 const std::stop_token &) -> std::expected<Geometry, std::string> {
        hit = false;
        const auto &atlas = source->Require(selection.View);
        auto geometry = Render::BuildFractionalImpostorSurface(
            atlas, source->Recipe.is_object() ? 0 : selection.View, selection.Mip);
        if (!geometry) { return std::unexpected("native source-view preparation failed"); }
        return std::move(*geometry);
      }};
  auto ticket = pipeline.RequestResolved(consumer,
                                         {.Key = *product,
                                          .Generation = consumer.Generation(),
                                          .InputBytes = 256,
                                          .ConsumerBytes = 0,
                                          .ReceiptBytesMost = 2048},
                                         std::move(job));
  if (!ticket) { Fail("native view job admission failed"); }
  const auto until = Clock::now() + std::chrono::seconds(30);
  for (;;) {
    const auto result = ticket->Poll();
    if (result.Finished) {
      if (result.Status != AssetPublicationStatus::Ready) { Fail(result.Message); }
      break;
    }
    if (Clock::now() >= until) { Fail("native view job exceeded its deadline"); }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  auto ready = products.Load(selected, limit);
  if (!ready || !*ready) { Fail("published native view is unavailable"); }
  return std::move(**ready);
}

void CachedWorldViews(const Fs::path &root, const Json &input) {
  using Content::ImpostorViewAssets;
  constexpr size_t limit = size_t{64} * 1024 * 1024;
  const auto openedAt = Clock::now();
  auto opened = AssetCache::Open((root / "world-view-assets.sqlite").string());
  const double databaseOpenMs = Milliseconds(openedAt);
  if (!opened) { Fail("native world asset database unavailable"); }
  const std::shared_ptr<AssetCache> cache(std::move(*opened));
  const auto source = std::make_shared<BuildingViewSource>(BuildingViewSource{
      .AtlasFile = root / "atlas.bin",
      .AtlasDigest = input.value("atlas_sha256", std::string{}),
      .AtlasProvenance = input.value("atlas_provenance", std::string("native-original-impostor-1")),
      .SourceInput = input.value("source_input", std::string{}),
      .Recipe = input.value("source_request", Json())});
  const auto metadataAt = Clock::now();
  const auto products = OpenViewAssets(
      *cache, *source, {.Min = Vector(input.at("least")), .Max = Vector(input.at("most"))});
  Tasks workers(2);
  AssetPipeline pipeline(workers,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = 256,
                          .ReceiptBytesMost = 2048});
  auto consumer = pipeline.OpenConsumer({.Identity = cache});
  if (!consumer) { Fail("native view consumer unavailable"); }
  size_t writes = 0;
  Json results{{"mode", "cached-world"},
               {"database_open_ms", databaseOpenMs},
               {"metadata_ms", Milliseconds(metadataAt)},
               {"cases", Json::array()}};
  for (const auto &query : input.at("queries")) {
    const Vec3 toward = Vector(query.at("eye")) - products.Metadata().Centre;
    const double radius = products.Metadata().HalfExtentM * std::numbers::sqrt3;
    const double nearestM = std::max(1.0, Length(toward) - radius);
    const double diameter = 2 * radius * query.at("focal_px").get<double>() / nearestM;
    const auto selected = products.Select(toward, diameter);
    if (!selected) { Fail("native world view selection failed"); }
    const auto began = Clock::now();
    bool hit = true;
    const auto loaded =
        ResolveView(pipeline, *consumer, cache, source, products, *selected, limit, hit);
    if (!hit) { ++writes; }
    const double loadMs = Milliseconds(began);
    const auto record = cache->Find(products.Key(*selected));
    if (!record || !*record) { Fail("native world product has no metadata"); }
    const auto name = query.at("name").get<std::string>();
    Json result = Draw(Geometry{}, query, root / (name + "-cached-world"), &loaded);
    result.update({{"query", name},
                   {"selected_view", selected->View},
                   {"selected_mip", selected->Mip},
                   {"projected_support_px", diameter},
                   {"hit", hit},
                   {"asset_load_ms", loadMs},
                   {"package_bytes", (**record).ByteCount},
                   {"asset_key", (**record).Key}});
    results["cases"].push_back(std::move(result));
  }
  results.update({{"source_read_bytes", source->ReadBytes},
                  {"source_decodes", source->Decodes},
                  {"writes", writes},
                  {"source_read_ms", source->ReadMs},
                  {"source_decode_ms", source->DecodeMs}});
  if (source->Producer) {
    const auto &report = source->Producer->Report();
    results["source_read_bytes"] = report.at("index_bytes").get<size_t>() +
                                   report.at("descriptor_bytes").get<size_t>() +
                                   report.at("form_bytes").get<size_t>();
    results["source_decodes"] = report.at("form_reads").get<size_t>() + 2;
    results["source_read_ms"] = report.at("source_read_ms");
    results["source_decode_ms"] = report.at("source_decode_ms");
    results["source_capture"] = report;
  }
  results["captures"] = source->Captures;
  results["source_header_ms"] = source->HeaderMs;
  std::ofstream report(root / "world-result.json");
  report << results.dump(2) << '\n';
  if (!report) { Fail("native world report write failed"); }
}

void SourceMeshViews(const Fs::path &root, const Json &input) {
  Json results{{"mode", "source-mesh"}, {"cases", Json::array()}};
  const Vec3 centre = (Vector(input.at("least")) + Vector(input.at("most"))) * .5;
  for (const auto &query : input.at("queries")) {
    const Vec3 toward = Vector(query.at("eye")) - centre;
    size_t selected = 0;
    double best = -std::numeric_limits<double>::infinity();
    for (size_t view = 0; view < input.at("catalogue").size(); ++view) {
      const double score = Dot(toward, Vector(input.at("catalogue").at(view)));
      if (score > best) {
        selected = view;
        best = score;
      }
    }
    const auto directory = root / "geometry" / std::to_string(selected);
    std::ifstream description(directory / "input.json");
    const auto document = Json::parse(description, nullptr, false);
    if (!document.is_object()) { Fail("source-plane geometry description unavailable"); }
    const auto mesh = Source(directory, document);
    const auto name = query.at("name").get<std::string>();
    auto record = Draw(mesh, query, root / (name + "-source-mesh"));
    record.update({{"query", name}, {"selected_view", selected}});
    results["cases"].push_back(std::move(record));
  }
  std::ofstream report(root / "mesh-result.json");
  report << results.dump(2) << '\n';
  if (!report) { Fail("native source-mesh report write failed"); }
}

void CompareOriginals(const Fs::path &root, const Json &input) {
  const auto sourceStarted = Clock::now();
  const auto original = Source(root, input);
  const double sourceMs = Milliseconds(sourceStarted);
  Render::ImpostorCapture capture;
  capture.LeastM = Vector(input.at("least"));
  capture.MostM = Vector(input.at("most"));
  capture.Sources.push_back({.Mesh = original.clone(), .Instances = {Mat4{}}});
  for (const auto &direction : input.at("catalogue")) {
    capture.ViewDirections.push_back(Vector(direction));
  }
  std::string error;
  const auto began = Clock::now();
  const auto atlas =
      Render::ImpostorBaker::Bake(std::move(capture), {.Pixels = 128, .Views = 9}, error);
  if (!atlas) { Fail(error); }
  const double bakeMs = Milliseconds(began);
  const auto encoded = atlas->Encode("native-original-impostor-1", error);
  if (!encoded) { Fail(error); }
  Write<uint8_t>(root / "atlas.bin", *encoded);
  Json results = {{"source_ms", sourceMs},
                  {"bake_ms", bakeMs},
                  {"atlas_bytes", encoded->size()},
                  {"cases", Json::array()}};
  results["captured_views"] = CaptureStatistics(*atlas);
  for (const auto &query : input.at("queries")) {
    const std::string name = query.at("name").get<std::string>();
    const Vec3 toward = Vector(query.at("eye")) - atlas->CentreM();
    size_t selected = 0;
    double best = -std::numeric_limits<double>::infinity();
    for (size_t view = 0; view < atlas->Views().size(); ++view) {
      const double score = Dot(toward, atlas->Views()[view].TowardEye);
      if (score > best) {
        selected = view;
        best = score;
      }
    }
    Json record = {{"query", name},
                   {"selected_view", selected},
                   {"original", Draw(original, query, root / (name + "-original"))}};
    for (const auto mode :
         {Render::ImpostorSurfaceDetail::Flat, Render::ImpostorSurfaceDetail::Depth}) {
      const auto productBegan = Clock::now();
      auto surface = Render::BuildImpostorSurface(*atlas, selected, mode);
      if (!surface) { Fail("source atlas has no native surface"); }
      const double productMs = Milliseconds(productBegan);
      const std::string label = mode == Render::ImpostorSurfaceDetail::Flat ? "flat" : "depth";
      record[label] = Draw(*surface, query, root / std::string(name).append("-").append(label));
      record[label]["product_ms"] = productMs;
      auto material = surface->surfaceAt(surface->materialOf(0));
      material.Alpha = AlphaMode::Blended;
      if (!surface->setSurface(surface->materialOf(0), material)) {
        Fail("blended parent rejected");
      }
      const std::string blended = std::string(label) + "-blended";
      record[blended] = Draw(*surface, query, root / std::string(name).append("-").append(blended));
      record[blended]["product_ms"] = productMs;
      const auto momentsBegan = Clock::now();
      const auto moments = MomentSurface(*atlas, selected, mode);
      const double momentsMs = Milliseconds(momentsBegan);
      const std::string momentLabel = std::string(label) + "-moments";
      record[momentLabel] =
          Draw(moments, query, root / std::string(name).append("-").append(momentLabel));
      record[momentLabel]["product_ms"] = momentsMs;
    }
    record["clipped-flat-moments"] =
        ClippedFlatControl(*atlas, selected, query, root / (name + "-clipped-flat-moments"));
    const auto supportBegan = Clock::now();
    const auto supported = SupportedMomentSurface(*atlas, selected);
    const double supportMs = Milliseconds(supportBegan);
    record["supported-depth-moments"] =
        Draw(supported, query, root / (name + "-supported-depth-moments"));
    record["supported-depth-moments"]["product_ms"] = supportMs;
    results["cases"].push_back(std::move(record));
  }
  std::ofstream report(root / "result.json");
  report << results.dump(2) << '\n';
}

}

int main(int argc, char *const *argv) {
  const bool cachedWorld = argc == 3 && std::string_view(argv[2]) == "--cached-world";
  const bool sourceMesh = argc == 3 && std::string_view(argv[2]) == "--source-mesh";
  const bool radianceWorld = argc == 3 && std::string_view(argv[2]) == "--radiance-world";
  if (argc != 2 && !cachedWorld && !sourceMesh && !radianceWorld) {
    Fail("expected a prepared input directory [--cached-world|--source-mesh|--radiance-world]");
  }
  const Fs::path root(argv[1]);
  std::ifstream file(root / "input.json");
  const auto input = Json::parse(file, nullptr, false);
  if (input.is_discarded() || !SDL_Init(SDL_INIT_VIDEO)) { Fail("input/video unavailable"); }
  if (cachedWorld || sourceMesh || radianceWorld) {
    if (radianceWorld) {
      RadianceViews(root, input);
      SDL_Quit();
      return 0;
    }
    if (sourceMesh) {
      SourceMeshViews(root, input);
    } else {
      CachedWorldViews(root, input);
    }
    SDL_Quit();
    return 0;
  }
  CompareOriginals(root, input);
  SDL_Quit();
  return 0;
}
