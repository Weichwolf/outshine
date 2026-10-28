#include "ImpostorBaker.h"
#include "ImpostorOptics.h"
#include "ImpostorSurface.h"
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
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <print>
#include <ratio>
#include <span>
#include <string>
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
    Material material;
    material.BaseColour = base.at(at);
    material.Roughness = at == 1 ? 0.72f : 0.88f;
    material.Pattern = at == 2 ? SurfacePattern::FacadeMetres : SurfacePattern::Facade;
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

void Filtered(Render::SceneRenderer &renderer,
              const Render::Viewpoint &camera,
              const Fs::path &output) {
  constexpr size_t pixels = size_t{1280} * 720;
  std::vector<float> average(pixels * 4);
  auto lens = Render::Lens::From(camera, 1280, 720);
  if (!lens) { Fail("filtered camera has no projection"); }
  const auto basis = renderer.LastSubmittedCamera().Basis;
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      const double dx = (static_cast<double>(x) + 0.5) / 4 - 0.5;
      const double dy = (static_cast<double>(y) + 0.5) / 4 - 0.5;
      const double focal = 360 / std::tan(camera.YfovRad * 0.5);
      auto sampled = basis;
      sampled.Forward = basis.Forward + basis.Right * (dx / focal) + basis.Up * (dy / focal);
      if (!Normalise(sampled.Forward)) { Fail("sampled forward has no direction"); }
      sampled.Right = Cross(sampled.Forward, basis.Up);
      if (!Normalise(sampled.Right)) { Fail("sampled screen axes are singular"); }
      sampled.Up = Cross(sampled.Right, sampled.Forward);
      renderer.SetCamera(sampled, *lens);
      if (auto rendered = renderer.RenderFrame(); !rendered) { Fail(rendered.error()); }
      std::vector<float> values;
      if (renderer.ReadSceneLinear(values) != Render::ReadState::Ready ||
          values.size() != average.size()) {
        Fail("filtered native colour readback incomplete");
      }
      for (size_t at = 0; at < average.size(); ++at) { average[at] += values[at] / 16; }
    }
  }
  Write<float>(output.string() + ".ss16", average);
}

Json Draw(const Geometry &mesh, const Json &query, const Fs::path &output) {
  const auto began = Clock::now();
  Render::GpuSubmission submission;
  submission.Create = [](void *, Render::Native::DeviceOptions options) {
    options.GpuTiming = Render::Native::GpuTimingMode::Submission;
    return Render::Native::CreateDevice(options);
  };
  Render::SceneRenderer renderer{submission};
  Core::Declaration declaration;
  declaration.InitialGeometry = &mesh;
  declaration.SurfaceWidthPx = 1280;
  declaration.SurfaceHeightPx = 720;
  declaration.KeyLux = 100;
  declaration.KeyElevationDeg = query.value("sun_elevation_deg", 0.0);
  declaration.Haze = 0;
  declaration.Exposure = 1;
  declaration.Stages = {"opaqueGeometry"};
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
  Filtered(renderer, camera, output);
  size_t triangles = 0;
  size_t vertices = 0;
  for (int part = 0; part < mesh.parts(); ++part) {
    triangles += mesh.trianglesOf(part).size() / 3;
    vertices += mesh.positionsOf(part).size() / 3;
  }
  std::array<double, 3> normalSum{};
  for (size_t pixel = 0; pixel < depth.size(); ++pixel) {
    if (depth[pixel] == 0) { continue; }
    for (size_t axis = 0; axis < 3; ++axis) { normalSum[axis] += normal[pixel * 4 + axis]; }
  }
  size_t imageBytes = 0;
  for (int image = 0; image < mesh.images(); ++image) {
    const auto data = mesh.imageAt(image);
    imageBytes += data.Rgba.size();
    for (const auto &mip : data.LowerMips) {
      if (mip) { imageBytes += mip->size(); }
    }
  }
  return {{"normal_sum", normalSum},
          {"setup_ms", setupMs},
          {"device", renderer.GpuName()},
          {"gpu_ms", samples},
          {"images", mesh.images()},
          {"image_storage_bytes", imageBytes},
          {"triangles", triangles},
          {"vertices", vertices}};
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

}

int main(int argc, char *const *argv) {
  if (argc != 2) { Fail("expected a prepared input directory"); }
  const Fs::path root(argv[1]);
  std::ifstream file(root / "input.json");
  const auto input = Json::parse(file, nullptr, false);
  if (input.is_discarded() || !SDL_Init(SDL_INIT_VIDEO)) { Fail("input/video unavailable"); }
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
  SDL_Quit();
  return 0;
}
