#include "Input.h"
#include "FixedPatch.h"
#include "GpuSupport.h"
#include "SpirvFile.h"
#include "device/native/Device.h"
#include <nlohmann/json.hpp>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace N = outshine::Render::Native;
namespace C = outshine::Diagnostics::Calibration;
namespace F = FixedCoverage;
namespace S = SourceRaster;
using Json = nlohmann::json;

namespace {
constexpr uint64_t DepthBytes = uint64_t{S::Side} * S::Side * sizeof(float);

template <class T> T Take(N::Result<T> result) {
  if (!result) {
    std::println(stderr, "{}", result.error().Message);
    std::_Exit(2);
  }
  return std::move(*result);
}

void Take(N::Result<void> result) {
  if (!result) {
    std::println(stderr, "{}", result.error().Message);
    std::_Exit(2);
  }
}

std::unique_ptr<N::Buffer> Upload(N::Device &device, std::span<const S::Quad> quads) {
  const auto bytes = std::as_bytes(quads);
  auto buffer = Take(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = N::BufferUse::Storage | N::BufferUse::CopyDestination}));
  auto commands = Take(device.Begin());
  Take(C::Upload(device, *commands, *buffer, bytes));
  Take(C::Finish(device, std::move(commands)));
  return buffer;
}

std::unique_ptr<N::GraphicsPipeline> Pipeline(N::Device &device,
                                              const std::filesystem::path &path) {
  const auto shader = outshine::Render::ReadSpirv(path.string());
  if (!shader) { std::_Exit(2); }
  N::GraphicsDescription description;
  description.Vertex.Spirv = *shader;
  description.Depth = N::DepthTarget{};
  description.Culling = N::Cull::None;
  return Take(device.CreateGraphicsPipeline(description));
}

void Render(N::Device &device,
            const N::GraphicsPipeline &pipeline,
            const N::Buffer &quads,
            const N::Texture &depth,
            uint32_t selected) {
  auto commands = Take(device.Begin());
  N::RenderPassDescription pass;
  pass.Depth = N::DepthAttachment{
      .Image = {.Image = &depth}, .Begin = N::Load::Clear, .End = N::Store::Store, .Clear = 0};
  Take(commands->BeginRenderPass(pass));
  const std::array bindings{N::Binding{
      .Set = 0,
      .Slot = 0,
      .Value = N::StorageBuffer{.Data = &quads, .Range = {.Bytes = quads.Description().Bytes}}}};
  N::DrawDescription draw;
  draw.State.Pipeline = &pipeline;
  draw.State.Bindings = bindings;
  draw.State.Area.Width = S::Side;
  draw.State.Area.Height = S::Side;
  draw.State.Clip.Width = S::Side;
  draw.State.Clip.Height = S::Side;
  draw.Range.Count = 6;
  draw.Range.FirstInstance = selected < S::CaseCount ? selected : 0;
  draw.Range.Instances = selected < S::CaseCount ? 1 : S::CaseCount;
  Take(commands->Draw(draw));
  Take(commands->EndRenderPass());
  Take(C::Finish(device, std::move(commands)));
}

std::vector<float> Download(N::Device &device, const N::Texture &depth) {
  auto buffer = Take(device.CreateBuffer({.Bytes = DepthBytes,
                                          .Use = N::BufferUse::CopyDestination,
                                          .Placement = N::Memory::Readback}));
  auto commands = Take(device.Begin());
  N::TextureTransfer transfer;
  transfer.RowBytes = uint64_t{S::Side} * sizeof(float);
  transfer.Region.Width = S::Side;
  transfer.Region.Height = S::Side;
  Take(commands->Copy(depth, *buffer, transfer));
  Take(C::Finish(device, std::move(commands)));
  const auto mapped = Take(device.MapRead(*buffer, {.Bytes = DepthBytes}));
  std::vector<float> result(size_t{S::Side} * S::Side);
  std::memcpy(result.data(), mapped.data(), DepthBytes);
  Take(device.Unmap(*buffer));
  return result;
}

void Save(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  if (!out) { std::_Exit(2); }
}

struct AuditState {
  uint64_t Tested = 0;
  uint64_t Missing = 0;
  uint64_t Unsafe = 0;
  uint64_t Invalid = 0;
  Json MissingExamples = Json::array();
  Json DepthExamples = Json::array();
};

void Inspect(AuditState &state,
             const S::Quad &source,
             size_t caseIndex,
             const std::array<uint32_t, 2> &pixel,
             const std::array<float, 2> &depth) {
  const auto [actual, bound] = depth;
  ++state.Tested;
  state.Invalid += !std::isfinite(actual) || actual < 0 || actual > 1 ? 1u : 0u;
  state.Missing += actual == 0 ? 1u : 0u;
  if (!(actual < bound)) { return; }
  ++state.Unsafe;
  constexpr size_t ExampleLimit = 16;
  auto &examples = actual == 0 ? state.MissingExamples : state.DepthExamples;
  if (examples.size() == ExampleLimit) { return; }
  examples.push_back({{"case", caseIndex},
                      {"pixel", pixel},
                      {"clip", source.Points},
                      {"bound", bound},
                      {"actual", actual},
                      {"bound_bits", std::bit_cast<uint32_t>(bound)},
                      {"actual_bits", std::bit_cast<uint32_t>(actual)}});
}

uint64_t InspectCase(AuditState &state,
                     const S::Quad &source,
                     const F::FixedPatch &patch,
                     size_t caseIndex,
                     std::span<const float> depth) {
  uint64_t count = 0;
  const uint32_t left = static_cast<uint32_t>(caseIndex % S::Tiles) * S::TileSide;
  const uint32_t top = static_cast<uint32_t>(caseIndex / S::Tiles) * S::TileSide;
  for (uint32_t y = top; y < top + S::TileSide; ++y) {
    for (uint32_t x = left; x < left + S::TileSide; ++x) {
      if (!F::FullPixel(patch, x, y)) { continue; }
      ++count;
      Inspect(state, source, caseIndex, {x, y}, {depth[size_t{y} * S::Side + x], patch.Depth[0]});
    }
  }
  return count;
}

Json Audit(std::span<const S::Quad> sources, std::span<const float> depth, uint32_t selected) {
  AuditState state;
  uint64_t empty = 0;
  uint64_t unknown = 0;
  const size_t first = selected < S::CaseCount ? selected : 0;
  const size_t last = selected < S::CaseCount ? first + 1 : sources.size();
  for (size_t i = first; i < last; ++i) {
    const F::SourceQuad source{.Points = sources[i].Points, .Opaque = true, .Complete = true};
    const auto patch = F::Prepare(source, S::Side, S::Side);
    if (!patch) {
      ++unknown;
      continue;
    }
    empty += InspectCase(state, sources[i], *patch, i, depth) == 0 ? 1u : 0u;
  }
  return {{"source_quads", last - first},
          {"tested_whole_pixels", state.Tested},
          {"missing_source_samples", state.Missing},
          {"unsafe_depth_samples", state.Unsafe},
          {"unknown_quads", unknown},
          {"empty_quads", empty},
          {"invalid_output", state.Invalid},
          {"observed_bound_holds", state.Unsafe == 0 && state.Missing == 0},
          {"missing_examples", state.MissingExamples},
          {"nonzero_depth_examples", state.DepthExamples}};
}
}

int main(int argc, char *const *argv) {
  if (argc != 5) { return 2; }
  uint32_t seed = 0;
  const std::string_view seedText(argv[2]);
  const auto parsed = std::from_chars(seedText.data(), seedText.data() + seedText.size(), seed);
  if (parsed.ec != std::errc{} || parsed.ptr != seedText.data() + seedText.size()) { return 2; }
  uint32_t selected = 0;
  const std::string_view selectedText(argv[3]);
  const auto chosen =
      std::from_chars(selectedText.data(), selectedText.data() + selectedText.size(), selected);
  if (chosen.ec != std::errc{} || chosen.ptr != selectedText.data() + selectedText.size() ||
      selected > S::CaseCount) {
    return 2;
  }
  const std::filesystem::path output(argv[1]);
  if (std::string_view(argv[4]) != "0" && std::string_view(argv[4]) != "1") { return 2; }
  const bool normalized = std::string_view(argv[4]) == "1";
  const auto sources = S::Generate(seed, normalized);
  const auto device = Take(N::CreateDevice({.Validation = true}));
  const auto quads = Upload(*device, sources);
  const auto pipeline = Pipeline(*device, output / "source.vert.spv");
  const auto target = Take(
      device->CreateTexture({.Width = S::Side,
                             .Height = S::Side,
                             .Texels = N::Format::Depth32Float,
                             .Use = N::TextureUse::DepthAttachment | N::TextureUse::CopySource}));
  Render(*device, *pipeline, *quads, *target, selected);
  const auto depth = Download(*device, *target);
  auto result = Audit(sources, depth, selected);
  result["seed"] = seed;
  result["normalized"] = normalized;
  result["selected_case"] = selected < S::CaseCount ? Json(selected) : Json(nullptr);
  result["device"] = device->Limits().Name;
  result["profile"] = {S::Side, S::Side};
  result["logical_source_bytes"] = sources.size() * sizeof(S::Quad);
  result["logical_depth_bytes"] = DepthBytes;
  result["logical_readback_bytes"] = DepthBytes;
  result["qualified_for_page_suppression"] = false;
  result["scope"] = "Actual original triangles, supplied clip-space float32 points; finite raster "
                    "depth audit, no world transform or portable raster bound";
  Save(output / "source-quads.bin", std::as_bytes(std::span(sources)));
  Save(output / "source-depth.d32", std::as_bytes(std::span(depth)));
  std::ofstream report(output / "result.json");
  report << result.dump(2);
  return report && result["unknown_quads"] == 0 && result["empty_quads"] == 0 &&
                 result["invalid_output"] == 0
             ? 0
             : 1;
}
