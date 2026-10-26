#include "GpuSupport.h"
#include "SpirvFile.h"
#include "Readback.h"
#include <nlohmann/json.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ios>
#include <string_view>
#include <fstream>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>
namespace Native = outshine::Render::Native;
namespace Calibration = outshine::Diagnostics::Calibration;

namespace {
using Bytes = std::vector<std::byte>;
constexpr uint32_t kWidth = 1280;
constexpr uint32_t kHeight = 720;
constexpr uint64_t kDepthBytes = uint64_t{kWidth} * kHeight * sizeof(float);
constexpr std::streamoff kInputLimit = 512ll * 1024 * 1024;
constexpr size_t kJobCountOffset = 112;
constexpr uint32_t kSelectionThreads = 64;
constexpr uint32_t kJobsPerCompactGroup = 4;
constexpr std::array<std::string_view, 3> kModes{
    "baseline_raster", "select_compact", "select_compact_raster"};

struct RasterInputs {
  const Native::Buffer &Positions;
  const Native::Buffer &Models;
  const Bytes &View;
  const Native::Texture &Depth;
};

[[noreturn]] void Fail(const std::string &message) {
  std::println(stderr, "{}", message);
  std::_Exit(1);
}

template <typename T> T Require(Native::Result<T> result) {
  if (!result) { Fail(result.error().Message); }
  return std::move(*result);
}

void Check(Native::Result<void> result) {
  if (!result) { Fail(result.error().Message); }
}

Bytes Read(const std::string &name) {
  std::ifstream file(name, std::ios::binary | std::ios::ate);
  if (!file) { Fail(name); }
  const auto size = file.tellg();
  if (size < 0 || size > kInputLimit) { Fail(name); }
  Bytes out(static_cast<size_t>(size));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(out.size()));
  if (!file) { Fail(name); }
  return out;
}

std::vector<uint32_t> Words(const Bytes &bytes) {
  if (bytes.size() % sizeof(uint32_t) != 0) { Fail("unaligned word buffer"); }
  std::vector<uint32_t> words(bytes.size() / sizeof(uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

std::vector<uint32_t> Shader(const std::string &path) {
  auto words = outshine::Render::ReadSpirv(path);
  if (!words) { Fail(words.error()); }
  return std::move(*words);
}

std::unique_ptr<Native::Buffer>
Upload(Native::Device &device,
       Native::Commands &commands,
       const Bytes &bytes,
       Native::UseMask<Native::BufferUse> use = Native::BufferUse::Storage) {
  auto buffer = Require(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = use | Native::BufferUse::CopyDestination}));
  Check(Calibration::Upload(device, commands, *buffer, bytes));
  return buffer;
}

std::unique_ptr<Native::Buffer>
Buffer(Native::Device &device, size_t bytes, Native::UseMask<Native::BufferUse> use) {
  return Require(device.CreateBuffer({.Bytes = bytes, .Use = use | Native::BufferUse::CopySource}));
}

Bytes DepthPixels(Native::Device &device, const Native::Texture &depth) {
  auto buffer = Require(device.CreateBuffer({.Bytes = kDepthBytes,
                                             .Use = Native::BufferUse::CopyDestination,
                                             .Placement = Native::Memory::Readback}));
  auto commands = Require(device.Begin());
  Check(commands->Copy(depth,
                       *buffer,
                       {.RowBytes = uint64_t{kWidth} * sizeof(float),
                        .Region = {.Width = kWidth, .Height = kHeight}}));
  Check(Calibration::Finish(device, std::move(commands)));
  const auto data = Require(device.MapRead(*buffer, {.Bytes = kDepthBytes}));
  Bytes out(data.begin(), data.end());
  Check(device.Unmap(*buffer));
  return out;
}

Native::Binding Storage(uint32_t set, uint32_t slot, const Native::Buffer &buffer) {
  return {.Set = set,
          .Slot = slot,
          .Value = Native::StorageBuffer{.Data = &buffer,
                                         .Range = {.Bytes = buffer.Description().Bytes}}};
}

Bytes Download(Native::Device &device, const Native::Buffer &source) {
  auto buffer = Require(device.CreateBuffer({.Bytes = source.Description().Bytes,
                                             .Use = Native::BufferUse::CopyDestination,
                                             .Placement = Native::Memory::Readback}));
  auto commands = Require(device.Begin());
  Check(commands->Copy(source, *buffer, {.Bytes = source.Description().Bytes}));
  Check(Calibration::Finish(device, std::move(commands)));
  const auto data = Require(device.MapRead(*buffer, {.Bytes = source.Description().Bytes}));
  Bytes out(data.begin(), data.end());
  Check(device.Unmap(*buffer));
  return out;
}

struct Pipelines {
  std::unique_ptr<Native::ComputePipeline> Cull, Pack, Scan, Compact;
  std::unique_ptr<Native::GraphicsPipeline> Raster;
};

struct Variant {
  std::string Label;
  uint32_t Groups = 0, Rows = 0;
  Bytes View, Jobs, Expected;
  std::unique_ptr<Native::Buffer> Bounds, JobData, Batches, InitialArgs, BaselineArgs,
      BaselineIndices, Selection, Kept, Slots, Args, Indices;
  std::vector<Native::Binding> CullBindings, PackBindings, ScanBindings, CompactBindings;
};

Variant Prepare(Native::Device &device,
                Native::Commands &commands,
                const std::string &folder,
                const std::string &label,
                const Native::Buffer &models,
                const Native::Buffer &pyramid,
                const Native::Buffer &source) {
  Variant v;
  v.Label = label;
  const auto limit = label.substr(label.find('-') + 1);
  v.View = Read(folder + "/view-" + limit + ".bin");
  if (v.View.size() != 256) { Fail("CullView must contain 256 bytes"); }
  std::memcpy(&v.Groups, v.View.data() + kJobCountOffset, 4);
  v.Jobs = Read(folder + "/" + label + "-jobs.bin");
  v.Expected = Read(folder + "/" + label + "-expected.bin");
  const auto batches = Read(folder + "/" + label + "-batches.bin");
  v.Rows = static_cast<uint32_t>(batches.size() / 8);
  v.Bounds = Upload(device, commands, Read(folder + "/" + label + "-groups.bin"));
  v.JobData = Upload(device, commands, v.Jobs);
  v.Batches = Upload(device, commands, batches);
  const auto args = Read(folder + "/" + label + "-args.bin");
  v.InitialArgs = Upload(device, commands, args, Native::BufferUse::CopySource);
  v.Args = Upload(device,
                  commands,
                  args,
                  Native::BufferUse::Storage | Native::BufferUse::Indirect |
                      Native::BufferUse::CopySource);
  v.BaselineArgs = Upload(device,
                          commands,
                          Read(folder + "/" + label + "-baseline-args.bin"),
                          Native::BufferUse::Indirect);
  const auto baseline = Read(folder + "/" + label + "-baseline-index.bin");
  v.BaselineIndices = Upload(device, commands, baseline, Native::BufferUse::Index);
  v.Selection = Buffer(device, v.Groups * 16ull, Native::BufferUse::Storage);
  v.Kept = Buffer(device, v.Groups * 4ull, Native::BufferUse::Storage);
  v.Slots = Buffer(device, v.Groups * 4ull, Native::BufferUse::Storage);
  v.Indices =
      Buffer(device, baseline.size(), Native::BufferUse::Storage | Native::BufferUse::Index);
  v.CullBindings = {Storage(0, 0, *v.Bounds),
                    Storage(0, 1, models),
                    Storage(0, 2, pyramid),
                    Storage(1, 0, *v.Selection),
                    {.Set = 2, .Slot = 0, .Value = Native::Uniform{.Bytes = v.View}}};
  v.PackBindings = {Storage(0, 0, *v.Selection), Storage(1, 0, *v.Kept)};
  v.ScanBindings = {Storage(0, 0, *v.Kept),
                    Storage(0, 1, *v.Batches),
                    Storage(1, 0, *v.Slots),
                    Storage(1, 1, *v.Args)};
  v.CompactBindings = {Storage(0, 0, *v.JobData),
                       Storage(0, 1, source),
                       Storage(0, 2, *v.Slots),
                       Storage(0, 3, *v.Args),
                       Storage(1, 0, *v.Indices),
                       {.Set = 2, .Slot = 0, .Value = Native::Uniform{.Bytes = v.View}}};
  return v;
}

Native::Result<void>
Select(Native::Commands &commands, const Pipelines &pipelines, const Variant &v) {
  auto a = commands.Dispatch(*pipelines.Cull,
                             v.CullBindings,
                             {.X = (v.Groups + kSelectionThreads - 1) / kSelectionThreads});
  if (!a) { return a; }
  a = commands.Dispatch(*pipelines.Pack,
                        v.PackBindings,
                        {.X = (v.Groups + kSelectionThreads - 1) / kSelectionThreads});
  if (!a) { return a; }
  a = commands.Dispatch(*pipelines.Scan, v.ScanBindings, {.X = v.Rows});
  if (!a) { return a; }
  return commands.Dispatch(*pipelines.Compact,
                           v.CompactBindings,
                           {.X = (v.Groups + kJobsPerCompactGroup - 1) / kJobsPerCompactGroup});
}

Native::Result<void> Raster(Native::Commands &commands,
                            const Pipelines &pipelines,
                            const Variant &v,
                            const RasterInputs &input,
                            bool baseline) {
  auto a =
      commands.BeginRenderPass({.Colours = {},
                                .Depth = Native::DepthAttachment{.Image = {.Image = &input.Depth},
                                                                 .Begin = Native::Load::Clear,
                                                                 .End = Native::Store::Store,
                                                                 .Clear = 0}});
  if (!a) { return a; }
  const std::array bindings{
      Storage(0, 0, input.Models),
      Native::Binding{.Set = 1, .Slot = 0, .Value = Native::Uniform{.Bytes = input.View}}};
  const std::array vertices{
      Native::VertexBuffer{.Stream = 0,
                           .Data = &input.Positions,
                           .Range = {.Bytes = input.Positions.Description().Bytes}}};
  const auto &indices = baseline ? *v.BaselineIndices : *v.Indices;
  const auto &args = baseline ? *v.BaselineArgs : *v.Args;
  a = commands.DrawIndexedIndirect(
      {.State = {.Pipeline = pipelines.Raster.get(),
                 .Bindings = bindings,
                 .Vertices = vertices,
                 .Area = {.Width = kWidth, .Height = kHeight},
                 .Clip = {.Width = kWidth, .Height = kHeight}},
       .Indices = {.Data = &indices, .Range = {.Bytes = indices.Description().Bytes}},
       .Range = {.Data = &args, .Count = v.Rows}});
  if (!a) { return a; }
  return commands.EndRenderPass();
}

Native::Result<Calibration::Sample> MeasureVariant(Native::Device &device,
                                                   const Pipelines &pipelines,
                                                   const Variant &variant,
                                                   const RasterInputs &input,
                                                   std::string_view mode) {
  return Calibration::Measure(device, [&](Native::Commands &into) -> Native::Result<void> {
    if (mode != "baseline_raster") {
      const auto selected = Select(into, pipelines, variant);
      if (!selected) { return selected; }
    }
    if (mode == "select_compact") { return {}; }
    return Raster(into, pipelines, variant, input, mode == "baseline_raster");
  });
}

nlohmann::json Verify(Native::Device &device, const Variant &v, const Bytes &source) {
  const auto selection = Download(device, *v.Selection);
  const auto args = Download(device, *v.Args);
  const auto output = Download(device, *v.Indices);
  const auto kept = Words(selection);
  const auto jobs = Words(v.Jobs);
  const auto draws = Words(args);
  const auto expected = Words(v.Expected);
  if (kept.size() != v.Groups * 4ull || jobs.size() != v.Groups * 4ull ||
      draws.size() != v.Rows * 5ull || expected.size() != v.Groups) {
    Fail("verification buffer lengths");
  }
  uint64_t unsafe = 0;
  uint64_t total = 0;
  uint32_t oldRow = 0;
  uint32_t cursor = 0;
  for (uint32_t group = 0; group < v.Groups; ++group) {
    const auto row = jobs[group * 4ull + 1];
    if (row != oldRow) {
      if (cursor != draws[oldRow * 5ull]) { Fail("batch index count"); }
      oldRow = row;
      cursor = 0;
    }
    const auto count = kept[group * 4ull] * 3;
    if (count != 0 && count != jobs[group * 4ull + 3]) { Fail("partial primitive group"); }
    unsafe += static_cast<uint64_t>(count == 0 && expected[group] != 0);
    if (count == 0) { continue; }
    const auto offset = (static_cast<uint64_t>(draws[row * 5ull + 2]) + cursor) * 4;
    const auto first = static_cast<uint64_t>(jobs[group * 4ull + 2]) * 4;
    if (offset + count * 4ull > output.size() || first + count * 4ull > source.size()) {
      Fail("compaction range");
    }
    if (std::memcmp(output.data() + offset, source.data() + first, count * 4ull) != 0) {
      Fail("compacted original indices differ");
    }
    cursor += count;
    total += count;
  }
  if (cursor != draws[oldRow * 5ull]) { Fail("last batch count"); }
  return {{"triangles", total / 3},
          {"false_rejections_vs_double", unsafe},
          {"original_indices_exact", true}};
}
}

int main(int argc, char *const *argv) {
  if (argc != 6) {
    Fail("visibility-pipeline <prepared> <source> <cull.spv> <shaderdir> <report>");
  }
  const std::string folder = argv[1];
  const std::string original = argv[2];
  const std::string shaders = argv[4];
  auto device = Require(Native::CreateDevice(
      {.Validation = true, .GpuTiming = outshine::Render::Native::GpuTimingMode::Submission}));
  Pipelines p;
  for (const auto &pair : {std::pair{&p.Cull, std::string(argv[3])},
                           std::pair{&p.Pack, shaders + "/pack.spv"},
                           std::pair{&p.Scan, shaders + "/scan.spv"},
                           std::pair{&p.Compact, shaders + "/compact32.spv"}}) {
    *pair.first = Require(device->CreateComputePipeline(Shader(pair.second)));
  }
  const auto vertex = Shader(shaders + "/raster.spv");
  const std::array streams{Native::VertexStream{.Slot = 0, .Stride = 12}};
  const std::array attributes{
      Native::VertexAttribute{.Location = 0, .Stream = 0, .Format = Native::VertexFormat::Float3}};
  p.Raster = Require(device->CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                     .Fragment = {},
                                                     .Streams = streams,
                                                     .Attributes = attributes,
                                                     .Colours = {},
                                                     .Depth = Native::DepthTarget{},
                                                     .Culling = Native::Cull::None,
                                                     .Label = "visibility-depth-cost"}));
  auto commands = Require(device->Begin());
  const auto sourceBytes = Read(original + "/indices.bin");
  const auto source = Upload(*device, *commands, sourceBytes);
  const auto positions =
      Upload(*device, *commands, Read(original + "/vertices.bin"), Native::BufferUse::Vertex);
  const auto models = Upload(*device, *commands, Read(folder + "/models.bin"));
  const auto pyramid = Upload(*device, *commands, Read(folder + "/pyramid.bin"));
  const auto view = Read(folder + "/raster-view.bin");
  const auto depth = Require(device->CreateTexture(
      {.Width = kWidth,
       .Height = kHeight,
       .Texels = Native::Format::Depth32Float,
       .Use = Native::TextureUse::DepthAttachment | Native::TextureUse::CopySource}));
  std::vector<Variant> variants;
  for (const auto *policy : {"source", "block"}) {
    for (const auto *limit : {"0", "64", "32", "16", "8"}) {
      variants.push_back(Prepare(*device,
                                 *commands,
                                 folder,
                                 std::string(policy) + "-" + limit,
                                 *models,
                                 *pyramid,
                                 *source));
    }
  }
  Check(Calibration::Finish(*device, std::move(commands)));
  nlohmann::json report{{"gpu", device->Limits().Name},
                        {"scope",
                         "Standalone selected-input bound selection+packing+production scan+wave "
                         "compact+position-only depth raster; final full-scene depth supplied "
                         "free; no PBR/parent-selection/two-phase/frame admission"},
                        {"variants", nlohmann::json::array()}};
  const RasterInputs input{
      .Positions = *positions, .Models = *models, .View = view, .Depth = *depth};
  for (const auto &v : variants) {
    nlohmann::json row{{"label", v.Label}, {"groups", v.Groups}, {"batches", v.Rows}};
    for (const auto mode : kModes) { row[mode] = nlohmann::json::array(); }
    report["variants"].push_back(row);
  }
  for (size_t round = 0; round < 12; ++round) {
    for (size_t one = 0; one < variants.size(); ++one) {
      const auto index = round % 2 == 0 ? one : variants.size() - 1 - one;
      const auto &v = variants[index];
      auto &row = report["variants"][index];
      for (size_t m = 0; m < kModes.size(); ++m) {
        const auto mode = kModes[(m + round) % kModes.size()];
        const auto sample = Require(MeasureVariant(*device, p, v, input, mode));
        row[mode].push_back(sample.GpuNs ? nlohmann::json(*sample.GpuNs) : nlohmann::json(nullptr));
      }
    }
    std::ofstream file(argv[5]);
    file << report.dump(2) << '\n';
    std::println("round {} complete", round);
  }
  Bytes baselineDepth;
  for (size_t index = 0; index < variants.size(); ++index) {
    auto &v = variants[index];
    auto capture = Require(device->Begin());
    Check(Raster(*capture, p, v, input, true));
    Check(Calibration::Finish(*device, std::move(capture)));
    const auto pixels = DepthPixels(*device, *depth);
    if (baselineDepth.empty()) {
      baselineDepth = pixels;
    } else if (pixels != baselineDepth) {
      Fail("baseline depth differs after regrouping");
    }
    auto proof = Verify(*device, v, sourceBytes);
    proof["baseline_depth_exact"] = true;
    report["variants"][index]["verification"] = proof;
    std::ofstream file(argv[5]);
    file << report.dump(2) << '\n';
    std::println("{} verified", v.Label);
  }
  return 0;
}
