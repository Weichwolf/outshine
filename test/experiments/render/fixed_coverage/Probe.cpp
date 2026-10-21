#include "FixedPatch.h"
#include "Exact.h"
#include "GpuSupport.h"
#include "SpirvFile.h"
#include "device/native/Device.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <expected>
#include <fstream>
#include <ios>
#include <memory>
#include <limits>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace F = FixedCoverage;
namespace N = outshine::Render::Native;
namespace C = outshine::Diagnostics::Calibration;

namespace {
constexpr uint32_t Side = 64;
constexpr uint64_t Bytes = uint64_t{Side} * Side * sizeof(float);
constexpr uint32_t FullSquarePixels = 2304;

[[noreturn]] void Fail(const N::Error &error) {
  std::println(stderr, "{}", error.Message);
  std::_Exit(2);
}

template <class T> T Take(N::Result<T> result) {
  if (!result) { Fail(result.error()); }
  return std::move(*result);
}

void Take(N::Result<void> result) {
  if (!result) { Fail(result.error()); }
}

std::vector<uint32_t> Shader(const std::filesystem::path &file) {
  auto words = outshine::Render::ReadSpirv(file.string());
  if (!words) {
    std::println(stderr, "{}", words.error());
    std::_Exit(2);
  }
  return std::move(*words);
}

struct ImagePair {
  std::vector<float> Colour;
  std::vector<float> Depth;
  double GpuNs = 0;
  bool HasGpuTime = false;
};

struct Target {
  std::unique_ptr<N::Texture> Colour;
  std::unique_ptr<N::Texture> Depth;
  std::unique_ptr<N::Buffer> Readback;
};

Target MakeTarget(N::Device &device) {
  Target target;
  target.Colour = Take(
      device.CreateTexture({.Width = Side,
                            .Height = Side,
                            .Texels = N::Format::R32Float,
                            .Use = N::TextureUse::ColourAttachment | N::TextureUse::CopySource}));
  target.Depth = Take(
      device.CreateTexture({.Width = Side,
                            .Height = Side,
                            .Texels = N::Format::Depth32Float,
                            .Use = N::TextureUse::DepthAttachment | N::TextureUse::CopySource}));
  target.Readback = Take(device.CreateBuffer(
      {.Bytes = Bytes, .Use = N::BufferUse::CopyDestination, .Placement = N::Memory::Readback}));
  return target;
}

std::vector<float> Download(N::Device &device, N::Texture &image, N::Buffer &readback) {
  auto commands = Take(device.Begin());
  N::TextureTransfer transfer;
  transfer.RowBytes = uint64_t{Side} * sizeof(float);
  transfer.Region.Width = Side;
  transfer.Region.Height = Side;
  Take(commands->Copy(image, readback, transfer));
  Take(C::Finish(device, std::move(commands)));
  auto mapped = Take(device.MapRead(readback, {.Bytes = Bytes}));
  std::vector<float> values(size_t{Side} * Side);
  std::memcpy(values.data(), mapped.data(), Bytes);
  Take(device.Unmap(readback));
  return values;
}

ImagePair Draw(N::Device &device,
               N::GraphicsPipeline &pipeline,
               Target &target,
               std::span<const F::FixedPatch> patches) {
  const auto sample = Take(C::Measure(device, [&](N::Commands &commands) -> N::Result<void> {
    const std::array colours{N::ColourAttachment{
        .Image = {.Image = target.Colour.get()}, .Begin = N::Load::Clear, .End = N::Store::Store}};
    N::RenderPassDescription pass;
    pass.Colours = colours;
    pass.Depth = N::DepthAttachment{.Image = {.Image = target.Depth.get()},
                                    .Begin = N::Load::Clear,
                                    .End = N::Store::Store,
                                    .Clear = 0};
    auto begun = commands.BeginRenderPass(pass);
    if (!begun) { return begun; }
    N::DrawDescription draw;
    draw.State.Pipeline = &pipeline;
    draw.State.Area.Width = Side;
    draw.State.Area.Height = Side;
    draw.State.Clip.Width = Side;
    draw.State.Clip.Height = Side;
    for (const auto &patch : patches) {
      const std::array bindings{N::Binding{
          .Set = 1, .Slot = 0, .Value = N::Uniform{.Bytes = std::as_bytes(std::span(&patch, 1))}}};
      draw.State.Bindings = bindings;
      draw.Range.Count = 3 * (patch.Meta[0] - 2);
      auto drawn = commands.Draw(draw);
      if (!drawn) { return drawn; }
    }
    return commands.EndRenderPass();
  }));
  ImagePair result;
  result.Colour = Download(device, *target.Colour, *target.Readback);
  result.Depth = Download(device, *target.Depth, *target.Readback);
  result.HasGpuTime = sample.GpuNs.has_value();
  if (sample.GpuNs) { result.GpuNs = *sample.GpuNs; }
  return result;
}

struct Case {
  std::string Name;
  std::vector<F::SourceQuad> Sources;
  std::optional<uint32_t> RequiredPixels = std::nullopt;
  std::optional<F::Unknown> RequiredUnknown = std::nullopt;
  bool ExternalReference = false;
};

std::expected<std::array<float, 4>, std::string> ReadPoint(const nlohmann::json &point) {
  if (!point.is_array() || point.size() != 4) {
    return std::unexpected("invalid clip-space point");
  }
  std::array<float, 4> result{};
  for (size_t i = 0; i < result.size(); ++i) {
    if (!point[i].is_number()) { return std::unexpected("non-numeric clip coordinate"); }
    const double value = point[i].get<double>();
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) {
      return std::unexpected("coordinate outside finite float32");
    }
    result[i] = static_cast<float>(value);
  }
  return result;
}

std::expected<F::SourceQuad, std::string> ReadQuad(const nlohmann::json &source) {
  if (!source.is_object() || !source.contains("points") || !source["points"].is_array() ||
      source["points"].size() != 4) {
    return std::unexpected("source requires four corners");
  }
  F::SourceQuad quad;
  quad.Opaque = true;
  quad.Complete = true;
  for (size_t i = 0; i < quad.Points.size(); ++i) {
    auto point = ReadPoint(source["points"][i]);
    if (!point) { return std::unexpected(point.error()); }
    quad.Points[i] = *point;
  }
  return quad;
}

std::expected<Case, std::string> ReadCase(const nlohmann::json &entry) {
  constexpr size_t SourceLimit = 4096;
  constexpr size_t NameLimit = 128;
  if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string() ||
      !entry.contains("sources") || !entry["sources"].is_array() ||
      entry["sources"].size() > SourceLimit) {
    return std::unexpected("invalid case fields");
  }
  Case fixture{.Name = entry["name"].get<std::string>(), .Sources = {}, .ExternalReference = true};
  if (fixture.Name.empty() || fixture.Name.size() > NameLimit ||
      !std::ranges::all_of(
          fixture.Name, [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; })) {
    return std::unexpected("case name must be a filename-safe identifier");
  }
  for (const auto &source : entry["sources"]) {
    auto quad = ReadQuad(source);
    if (!quad) { return std::unexpected(quad.error()); }
    fixture.Sources.push_back(*quad);
  }
  return fixture;
}

std::expected<std::vector<Case>, std::string> ReadCases(const std::filesystem::path &path) {
  constexpr std::streamoff InputLimit = std::streamoff{8} * 1024 * 1024;
  constexpr size_t CaseLimit = 64;
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input || input.tellg() < 0 || input.tellg() > InputLimit) {
    return std::unexpected("case input missing or too large");
  }
  input.seekg(0);
  const auto data = nlohmann::json::parse(input, nullptr, false);
  if (data.is_discarded() || !data.is_object() || !data.contains("cases") ||
      !data["cases"].is_array() || data["cases"].size() > CaseLimit ||
      !data.contains("qualified_for_page_suppression") ||
      data["qualified_for_page_suppression"] != false) {
    return std::unexpected("input must declare calibration-only cases");
  }
  std::vector<Case> cases;
  for (const auto &entry : data["cases"]) {
    auto fixture = ReadCase(entry);
    if (!fixture) { return std::unexpected(fixture.error()); }
    cases.push_back(std::move(*fixture));
  }
  return cases;
}

struct PixelRectangle {
  std::array<float, 2> Min;
  std::array<float, 2> Max;
};

F::SourceQuad Rectangle(const PixelRectangle &bounds, float depth) {
  const auto left = bounds.Min[0];
  const auto top = bounds.Min[1];
  const auto right = bounds.Max[0];
  const auto bottom = bounds.Max[1];
  constexpr float half = static_cast<float>(Side) / 2;
  return {.Points = {{{left / half - 1, 1 - top / half, depth, 1},
                      {right / half - 1, 1 - top / half, depth, 1},
                      {right / half - 1, 1 - bottom / half, depth, 1},
                      {left / half - 1, 1 - bottom / half, depth, 1}}},
          .Opaque = true,
          .Complete = true};
}

std::vector<Case> Cases() {
  constexpr float leftEdge = 8;
  constexpr float rightEdge = 56;
  constexpr float middleEdge = 32;
  constexpr float farDepth = .25f;
  constexpr float nearDepth = .75f;
  constexpr uint32_t full = FullSquarePixels;
  constexpr uint32_t fullFrame = Side * Side;
  constexpr uint32_t hole = 2240;
  std::vector<Case> cases;
  cases.push_back({.Name = "closed-square",
                   .Sources = {Rectangle(
                       {.Min = {leftEdge, leftEdge}, .Max = {rightEdge, rightEdge}}, farDepth)},
                   .RequiredPixels = full});
  cases.push_back(
      {.Name = "whole-viewport",
       .Sources = {Rectangle(
           {.Min = {-leftEdge, -leftEdge}, .Max = {Side + leftEdge, Side + leftEdge}}, farDepth)},
       .RequiredPixels = fullFrame});
  cases.push_back(
      {.Name = "adjacent-patches",
       .Sources = {Rectangle({.Min = {leftEdge, leftEdge}, .Max = {middleEdge, rightEdge}},
                             farDepth),
                   Rectangle({.Min = {middleEdge, leftEdge}, .Max = {rightEdge, rightEdge}},
                             farDepth)},
       .RequiredPixels = full});
  cases.push_back(
      {.Name = "overlapping-depths",
       .Sources = {Rectangle({.Min = {leftEdge, leftEdge}, .Max = {rightEdge, rightEdge}},
                             farDepth),
                   Rectangle({.Min = {leftEdge * 2, leftEdge * 2},
                              .Max = {rightEdge - leftEdge, rightEdge - leftEdge}},
                             nearDepth)},
       .RequiredPixels = full});
  cases.push_back(
      {.Name = "real-hole",
       .Sources =
           {Rectangle({.Min = {leftEdge, leftEdge}, .Max = {rightEdge, middleEdge - 4}}, farDepth),
            Rectangle({.Min = {leftEdge, middleEdge + 4}, .Max = {rightEdge, rightEdge}}, farDepth),
            Rectangle({.Min = {leftEdge, middleEdge - 4}, .Max = {middleEdge - 4, middleEdge + 4}},
                      farDepth),
            Rectangle({.Min = {middleEdge + 4, middleEdge - 4}, .Max = {rightEdge, middleEdge + 4}},
                      farDepth)},
       .RequiredPixels = hole});
  for (int i = 0; i < 16; ++i) {
    const float shift = static_cast<float>(i) / 16;
    auto source = Rectangle({.Min = {leftEdge + shift, leftEdge + shift},
                             .Max = {rightEdge + shift, rightEdge + shift}},
                            farDepth);
    for (size_t corner = 0; corner < source.Points.size(); ++corner) {
      constexpr std::array<float, 4> weights{.37f, 1.13f, 2.77f, 8.91f};
      for (auto &value : source.Points[corner]) { value *= weights[corner]; }
    }
    cases.push_back({.Name = "projective-subpixel-" + std::to_string(i), .Sources = {source}});
  }
  auto diamond = Rectangle({.Min = {leftEdge, leftEdge}, .Max = {rightEdge, rightEdge}}, nearDepth);
  constexpr std::array<std::array<float, 2>, 4> xy{{{32, -20}, {84, 32}, {32, 84}, {-20, 32}}};
  for (size_t i = 0; i < 4; ++i) {
    diamond.Points[i][0] = xy[i][0] / (static_cast<float>(Side) / 2) - 1;
    diamond.Points[i][1] = 1 - xy[i][1] / (static_cast<float>(Side) / 2);
  }
  cases.push_back({.Name = "eight-corner-clip", .Sources = {diamond}});
  for (size_t i = 0; i < 5; ++i) {
    auto unknown = cases.front().Sources.front();
    if (i == 0) { unknown.Opaque = false; }
    if (i == 1) { unknown.Complete = false; }
    if (i == 2) { unknown.Points[0][2] = -1; }
    if (i == 3) { unknown.Points[0][3] = 0; }
    if (i == 4) { unknown.Points[0][2] = 2; }
    constexpr std::array reasons{F::Unknown::NonOpaque,
                                 F::Unknown::Incomplete,
                                 F::Unknown::ClipCrossing,
                                 F::Unknown::ClipCrossing,
                                 F::Unknown::ClipCrossing};
    cases.push_back({.Name = "unknown-" + std::to_string(i),
                     .Sources = {unknown},
                     .RequiredPixels = 0,
                     .RequiredUnknown = reasons[i]});
  }
  return cases;
}

std::vector<Case> AdversarialCases() {
  std::vector<Case> cases;
  const auto original = Rectangle({.Min = {8, 8}, .Max = {56, 56}}, .5f);
  const auto reject = [&](const std::string &name, const F::SourceQuad &source, F::Unknown why) {
    cases.push_back(
        {.Name = name, .Sources = {source}, .RequiredPixels = 0, .RequiredUnknown = why});
  };
  auto source = original;
  source.Points[0][0] = std::numeric_limits<float>::quiet_NaN();
  reject("nan-coordinate", source, F::Unknown::Nonfinite);
  source = original;
  source.Points[2][1] = std::numeric_limits<float>::infinity();
  reject("infinite-coordinate", source, F::Unknown::Nonfinite);
  source = original;
  source.Points[1][3] = -1;
  reject("negative-w", source, F::Unknown::ClipCrossing);
  source = original;
  std::swap(source.Points[1], source.Points[2]);
  reject("bow-tie", source, F::Unknown::Nonconvex);
  source = original;
  source.Points[2][0] = -.5f;
  source.Points[2][1] = .5f;
  reject("concave", source, F::Unknown::Nonconvex);
  source = original;
  source.Points[1] = source.Points[0];
  reject("duplicate-corner", source, F::Unknown::Nonconvex);
  source = original;
  for (auto &point : source.Points) { point[2] = 0; }
  reject("zero-depth", source, F::Unknown::NoDepth);
  for (auto &point : source.Points) { point[2] = std::numeric_limits<float>::denorm_min(); }
  reject("subnormal-depth", source, F::Unknown::NoDepth);
  for (auto &point : source.Points) { point[2] = std::numeric_limits<float>::min(); }
  cases.push_back(
      {.Name = "minimum-normal-depth", .Sources = {source}, .RequiredPixels = FullSquarePixels});
  source = original;
  for (auto &point : source.Points) {
    point[0] = std::copysign(std::numeric_limits<float>::max(), point[0]);
    point[1] = std::copysign(std::numeric_limits<float>::max(), point[1]);
  }
  cases.push_back(
      {.Name = "maximum-finite-clipped", .Sources = {source}, .RequiredPixels = Side * Side});
  return cases;
}

nlohmann::json ProfileChecks() {
  constexpr std::array<std::array<uint32_t, 2>, 8> profiles{
      {{0, 64},
       {64, 0},
       {F::MaximumCoordinate + 1, 64},
       {64, F::MaximumCoordinate + 1},
       {F::MaximumCoordinate, F::MaximumCoordinate},
       {8192, 4096},
       {1920, 1080},
       {1280, 720}}};
  const auto source = Rectangle({.Min = {8, 8}, .Max = {56, 56}}, .5f);
  bool passed = true;
  for (const auto &profile : profiles) {
    const auto patch = F::Prepare(source, profile[0], profile[1]);
    const bool valid = profile[0] > 0 && profile[1] > 0 &&
                       std::max(profile[0], profile[1]) <= F::MaximumCoordinate;
    if (valid) {
      passed = passed && patch &&
               std::max(profile[0], profile[1]) * patch->Meta[3] <= F::MaximumCoordinate &&
               F::FullPixel(*patch, profile[0] / 2, profile[1] / 2);
    } else {
      passed = passed && !patch && patch.error() == F::Unknown::InvalidProfile;
    }
  }
  return {
      {"count", profiles.size()}, {"passed", passed}, {"scope", "CPU profile/grid bounds only"}};
}

bool Requirements(const Case &fixture, const nlohmann::json &row, const nlohmann::json &unknown) {
  if (fixture.RequiredPixels && row["accepted_pixels"] != *fixture.RequiredPixels) { return false; }
  return !fixture.RequiredUnknown ||
         (row["patches"] == 0 &&
          unknown == nlohmann::json::array({static_cast<int>(*fixture.RequiredUnknown)}));
}

bool SourceCovers(const F::Projected &source, const std::array<uint32_t, 2> &pixel, float depth) {
  if (!std::isfinite(depth) || source.FarDepth < F::Exact::Float32(depth)) { return false; }
  for (uint32_t corner = 0; corner < 4; ++corner) {
    if (!F::Inside(source.Corners,
                   {.X = F::Exact(pixel[0] + (corner & 1u)),
                    .Y = F::Exact(pixel[1] + ((corner >> 1u) & 1u))})) {
      return false;
    }
  }
  return true;
}

float ExpectedDepth(std::span<const F::FixedPatch> patches, uint32_t x, uint32_t y) {
  float result = 0;
  for (const auto &patch : patches) {
    if (F::FullPixel(patch, x, y)) { result = std::max(result, patch.Depth[0]); }
  }
  return result;
}

nlohmann::json
Check(const Case &fixture, std::span<const F::FixedPatch> patches, const ImagePair &images) {
  std::vector<F::Projected> sources;
  for (const auto &source : fixture.Sources) {
    auto projected = F::Project(source, Side, Side);
    if (projected) { sources.push_back(std::move(*projected)); }
  }
  uint64_t falsePixels = 0;
  uint64_t mismatches = 0;
  uint64_t invalid = 0;
  uint64_t accepted = 0;
  for (uint32_t y = 0; y < Side; ++y) {
    for (uint32_t x = 0; x < Side; ++x) {
      const float value = images.Colour[size_t{y} * Side + x];
      const float expected = ExpectedDepth(patches, x, y);
      mismatches += value != expected || images.Depth[size_t{y} * Side + x] != expected ? 1u : 0u;
      invalid += !std::isfinite(value) || value < 0 || value > 1 ? 1u : 0u;
      if (!(value > 0)) { continue; }
      ++accepted;
      const bool safe = std::ranges::any_of(
          sources, [&](const auto &source) { return SourceCovers(source, {x, y}, value); });
      falsePixels += safe ? 0u : 1u;
    }
  }
  return {{"accepted_pixels", accepted},
          {"false_source_coverage_or_depth", falsePixels},
          {"fixed_mask_or_d32_mismatch", mismatches},
          {"invalid_output", invalid}};
}

void Save(const std::filesystem::path &path, std::span<const float> values) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(values.data()),
             static_cast<std::streamsize>(values.size_bytes()));
  if (!file) { std::_Exit(2); }
}
}

int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) { return 2; }
  auto cases = Cases();
  for (auto &fixture : AdversarialCases()) { cases.push_back(std::move(fixture)); }
  if (argc == 4) {
    auto extra = ReadCases(argv[3]);
    if (!extra) {
      std::println(stderr, "{}", extra.error());
      return 2;
    }
    for (auto &fixture : *extra) { cases.push_back(std::move(fixture)); }
  }
  const std::filesystem::path folder(argv[1]);
  const auto vertex = Shader(folder / "patch.vert.spv");
  const auto fragment = Shader(folder / "patch.frag.spv");
  auto device = Take(N::CreateDevice({.Validation = true, .GpuTiming = true}));
  auto target = MakeTarget(*device);
  N::ColourTarget colour;
  colour.Texels = N::Format::R32Float;
  const std::array colours{colour};
  N::GraphicsDescription desc;
  desc.Vertex.Spirv = vertex;
  desc.Fragment = N::ShaderCode{.Spirv = fragment};
  desc.Colours = colours;
  desc.Depth = N::DepthTarget{};
  desc.Culling = N::Cull::None;
  auto pipeline = Take(device->CreateGraphicsPipeline(desc));
  nlohmann::json rows = nlohmann::json::array();
  const auto profiles = ProfileChecks();
  bool passed = profiles["passed"].get<bool>();
  const auto began = std::chrono::steady_clock::now();
  for (const auto &fixture : cases) {
    std::vector<F::FixedPatch> patches;
    nlohmann::json unknown = nlohmann::json::array();
    const auto started = std::chrono::steady_clock::now();
    for (const auto &source : fixture.Sources) {
      auto patch = F::Prepare(source, Side, Side);
      if (patch) {
        patches.push_back(*patch);
      } else {
        unknown.push_back(static_cast<int>(patch.error()));
      }
    }
    const double prepareSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    const auto images = Draw(*device, *pipeline, target, patches);
    std::ranges::reverse(patches);
    const auto reverse = Draw(*device, *pipeline, target, patches);
    auto row = Check(fixture, patches, images);
    const bool equal = std::ranges::equal(std::as_bytes(std::span(images.Colour)),
                                          std::as_bytes(std::span(reverse.Colour))) &&
                       std::ranges::equal(std::as_bytes(std::span(images.Depth)),
                                          std::as_bytes(std::span(reverse.Depth)));
    row["patches"] = patches.size();
    const bool required = Requirements(fixture, row, unknown);
    passed = passed && required && equal && row["false_source_coverage_or_depth"] == 0 &&
             row["fixed_mask_or_d32_mismatch"] == 0 && row["invalid_output"] == 0;
    row["name"] = fixture.Name;
    row["external_geometry_reference"] = fixture.ExternalReference;
    row["qualified_for_page_suppression"] = false;
    row["requirements_met"] = required;
    uint64_t triangles = 0;
    for (const auto &patch : patches) { triangles += patch.Meta[0] - 2; }
    row["draws_both_orders"] = 2 * patches.size();
    row["triangles_both_orders"] = 2 * triangles;
    row["logical_uniform_bytes_both_orders"] = 2 * patches.size() * F::UniformBytes;
    row["readback_bytes_both_orders"] = 4 * Bytes;
    row["unknown"] = unknown;
    row["order_invariant"] = equal;
    row["prepare_seconds"] = prepareSeconds;
    row["gpu_ns"] = images.GpuNs + reverse.GpuNs;
    row["missing_gpu_samples"] = (!images.HasGpuTime ? 1 : 0) + (!reverse.HasGpuTime ? 1 : 0);
    Save(folder / (fixture.Name + ".colour.f32"), images.Colour);
    Save(folder / (fixture.Name + ".depth.f32"), images.Depth);
    std::println("{}: {} pixels, {} patches, {} unknown",
                 fixture.Name,
                 row["accepted_pixels"].get<uint64_t>(),
                 patches.size(),
                 unknown.size());
    std::fflush(stdout);
    rows.push_back(std::move(row));
  }
  std::ofstream output(argv[2]);
  output
      << nlohmann::
             json{{"device", device->Limits().Name},
                  {"side_pixels", Side},
                  {"cases", rows},
                  {"profile_checks", profiles},
                  {"passed", passed},
                  {"wall_seconds",
                   std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count()},
                  {"scope",
                   "Exact input clip-space quad geometry and integer whole-pixel bounds. No "
                   "world-transform, arbitrary mesh qualification, frame/load or Metal claim."}}
                 .dump(2);
  return output && passed ? 0 : 1;
}
