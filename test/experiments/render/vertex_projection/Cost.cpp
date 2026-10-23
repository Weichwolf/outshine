#include "Support.h"
#include "Input.h"
#include "GpuSupport.h"
#include <fstream>
#include <memory>
#include <span>
#include <algorithm>
#include <cstring>
#include <array>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>
#include <filesystem>
#include <ios>
#include <cstdlib>
#include <cmath>
#include <string>
#include <optional>
#include <string_view>
namespace N = outshine::Render::Native;
namespace C = outshine::Diagnostics::Calibration;
using Json = nlohmann::json;
using Projection::Floats;
using Projection::Input;
using Projection::Shader;
using Projection::Take;

namespace {
constexpr uint32_t TargetWidth = 1280;
constexpr uint32_t TargetHeight = 720;
constexpr uint64_t DepthBytes = uint64_t{TargetWidth} * TargetHeight * sizeof(float);
static_assert(sizeof(std::array<float, 3>) == 12);

struct CostMesh {
  Input Uniform;
  std::unique_ptr<N::Buffer> Vertices;
  std::unique_ptr<N::Buffer> Indices;
  uint32_t Count = 0;
};

std::unique_ptr<N::Buffer>
Buffer(N::Device &device, N::BufferUse use, std::span<const std::byte> bytes) {
  auto result = Take(
      device.CreateBuffer({.Bytes = bytes.size(), .Use = use | N::BufferUse::CopyDestination}));
  auto commands = Take(device.Begin());
  Take(C::Upload(device, *commands, *result, bytes));
  Take(C::Finish(device, std::move(commands)));
  return result;
}

std::vector<uint32_t> Indices(const Json &faces, size_t vertexCount) {
  std::vector<uint32_t> indices;
  for (const auto &face : faces) {
    if (!face.is_array() || face.size() != 3) { std::_Exit(2); }
    for (const auto &at : face) {
      if (!at.is_number_integer() || at < 0 || at >= vertexCount) { std::_Exit(2); }
      indices.push_back(at.get<uint32_t>());
    }
  }
  return indices;
}

Json Number(std::optional<double> number) {
  return number ? Json(*number) : Json(nullptr);
}

std::vector<CostMesh> Meshes(N::Device &device, const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  constexpr std::streamoff Limit = std::streamoff{16} * 1024 * 1024;
  if (!file || file.tellg() < 0 || file.tellg() > Limit) { std::_Exit(2); }
  file.seekg(0);
  const auto source = Json::parse(file, nullptr, false);
  if (source.is_discarded() || !source.is_object() ||
      !source.contains("qualified_for_page_suppression") ||
      source["qualified_for_page_suppression"] != false || !source.contains("meshes") ||
      !source["meshes"].is_array() || source["meshes"].empty() || source["meshes"].size() > 512 ||
      !source.contains("mvp_f32") || !source.contains("shift_f32")) {
    std::_Exit(2);
  }
  std::vector<CostMesh> result;
  for (const auto &mesh : source["meshes"]) {
    if (!mesh.is_object() || !mesh.contains("model_f32") || !mesh.contains("positions_f32") ||
        !mesh["positions_f32"].is_array() || mesh["positions_f32"].empty() ||
        !mesh.contains("faces") || !mesh["faces"].is_array() || mesh["faces"].empty()) {
      std::_Exit(2);
    }
    CostMesh out;
    out.Uniform.Model = Floats<16>(mesh["model_f32"]);
    out.Uniform.View = Floats<16>(source["mvp_f32"]);
    const auto shift = Floats<3>(source["shift_f32"]);
    std::ranges::copy(shift, out.Uniform.Shift.begin());
    std::vector<std::array<float, 3>> points;
    for (const auto &point : mesh["positions_f32"]) { points.push_back(Floats<3>(point)); }
    auto indices = Indices(mesh["faces"], points.size());
    out.Count = static_cast<uint32_t>(indices.size());
    out.Vertices = Buffer(device, N::BufferUse::Vertex, std::as_bytes(std::span(points)));
    out.Indices = Buffer(device, N::BufferUse::Index, std::as_bytes(std::span(indices)));
    result.push_back(std::move(out));
  }
  return result;
}

std::unique_ptr<N::GraphicsPipeline> Pipeline(N::Device &device,
                                              const std::filesystem::path &path) {
  const auto vertex = Shader(path);
  const std::array streams{N::VertexStream{.Stride = sizeof(std::array<float, 3>)}};
  const std::array attributes{N::VertexAttribute{}};
  N::GraphicsDescription description;
  description.Vertex.Spirv = vertex;
  description.Streams = streams;
  description.Attributes = attributes;
  description.Depth = N::DepthTarget{};
  description.Culling = N::Cull::Back;
  return Take(device.CreateGraphicsPipeline(description));
}

C::Sample Render(N::Device &device,
                 N::Texture &target,
                 const N::GraphicsPipeline &pipeline,
                 const std::vector<CostMesh> &meshes,
                 uint32_t instances) {
  return Take(C::Measure(device, [&](N::Commands &commands) -> N::Result<void> {
    N::RenderPassDescription pass;
    pass.Depth = N::DepthAttachment{
        .Image = {.Image = &target}, .Begin = N::Load::Clear, .End = N::Store::Store};
    auto begun = commands.BeginRenderPass(pass);
    if (!begun) { return begun; }
    for (const auto &mesh : meshes) {
      const std::array bindings{
          N::Binding{.Set = 1,
                     .Slot = 0,
                     .Value = N::Uniform{.Bytes = std::as_bytes(std::span(&mesh.Uniform, 1))}}};
      const std::array vertices{N::VertexBuffer{
          .Data = mesh.Vertices.get(), .Range = {.Bytes = mesh.Vertices->Description().Bytes}}};
      N::IndexedDrawDescription draw;
      draw.State.Pipeline = &pipeline;
      draw.State.Bindings = bindings;
      draw.State.Vertices = vertices;
      draw.State.Area.Width = TargetWidth;
      draw.State.Area.Height = TargetHeight;
      draw.State.Clip.Width = TargetWidth;
      draw.State.Clip.Height = TargetHeight;
      draw.Indices = {.Data = mesh.Indices.get(),
                      .Range = {.Bytes = mesh.Indices->Description().Bytes}};
      draw.Range.Count = mesh.Count;
      draw.Range.Instances = instances;
      auto drawn = commands.DrawIndexed(draw);
      if (!drawn) { return drawn; }
    }
    return commands.EndRenderPass();
  }));
}

Json Snapshot(N::Device &device, N::Texture &target, const std::filesystem::path &path) {
  constexpr uint64_t bytes = DepthBytes;
  auto readback = Take(device.CreateBuffer(
      {.Bytes = bytes, .Use = N::BufferUse::CopyDestination, .Placement = N::Memory::Readback}));
  auto commands = Take(device.Begin());
  N::TextureTransfer transfer;
  transfer.RowBytes = uint64_t{TargetWidth} * sizeof(float);
  transfer.Region.Width = TargetWidth;
  transfer.Region.Height = TargetHeight;
  Take(commands->Copy(target, *readback, transfer));
  Take(C::Finish(device, std::move(commands)));
  auto mapped = Take(device.MapRead(*readback, {.Bytes = bytes}));
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(mapped.data()), static_cast<std::streamsize>(bytes));
  if (!out) { std::_Exit(2); }
  uint64_t covered = 0;
  for (size_t i = 0; i < mapped.size(); i += sizeof(float)) {
    float value = 0;
    std::memcpy(&value, mapped.data() + i, sizeof(float));
    if (!std::isfinite(value) || value < 0 || value > 1) { std::_Exit(2); }
    covered += value > 0 ? 1u : 0u;
  }
  Take(device.Unmap(*readback));
  if (covered == 0) { std::_Exit(2); }
  return {{"file", path.filename().string()}, {"covered_pixels", covered}};
}
}

int main(int argc, char **argv) {
  if (argc != 4 && (argc != 5 || std::string_view(argv[4]) != "--projection-contract")) {
    return 2;
  }
  constexpr uint32_t MaximumModes = 6;
  const uint32_t modes = argc == 5 ? MaximumModes : 4;
  auto device =
      Take(N::CreateDevice({.Validation = true, .GpuTiming = N::GpuTimingMode::Submission}));
  auto meshes = Meshes(*device, argv[2]);
  auto target = Take(
      device->CreateTexture({.Width = TargetWidth,
                             .Height = TargetHeight,
                             .Texels = N::Format::Depth32Float,
                             .Use = N::TextureUse::DepthAttachment | N::TextureUse::CopySource}));
  std::array<std::unique_ptr<N::GraphicsPipeline>, MaximumModes> pipelines;
  for (size_t mode = 0; mode < modes; ++mode) {
    pipelines[mode] = Pipeline(
        *device, std::filesystem::path(argv[1]) / ("cost-" + std::to_string(mode) + ".vert.spv"));
  }
  Json rows = Json::array();
  Json snapshots = Json::array();
  uint64_t missing = 0;
  for (uint32_t instances : {1u, 64u}) {
    for (uint32_t round = 0; round < 13; ++round) {
      for (uint32_t step = 0; step < modes; ++step) {
        const auto mode = (step + round) % modes;
        const auto sample = Render(*device, *target, *pipelines[mode], meshes, instances);
        if (round == 0 && instances == 1) {
          snapshots.push_back(Snapshot(*device,
                                       *target,
                                       std::filesystem::path(argv[1]) /
                                           ("depth-" + std::to_string(mode) + ".d32")));
        }
        missing += sample.GpuNs.has_value() ? 0u : 1u;
        rows.push_back({{"instances", instances},
                        {"round", round},
                        {"mode", mode},
                        {"gpu_ns", Number(sample.GpuNs)},
                        {"encode_ns", Number(sample.EncodeNs)},
                        {"wall_ns", sample.WallNs}});
      }
    }
  }
  uint64_t indices = 0;
  uint64_t bytes = 0;
  for (const auto &mesh : meshes) {
    indices += mesh.Count;
    bytes += mesh.Vertices->Description().Bytes + mesh.Indices->Description().Bytes;
  }
  std::ofstream out(argv[3]);
  out << Json{{"device", device->Limits().Name},
              {"meshes", meshes.size()},
              {"indices_per_instance", indices},
              {"geometry_buffer_bytes", bytes},
              {"depth_attachment_bytes", DepthBytes},
              {"mode_count", modes},
              {"readback_bytes", modes * DepthBytes},
              {"uniform_bytes_per_pass", meshes.size() * sizeof(Input)},
              {"snapshots", snapshots},
              {"missing_gpu_samples", missing},
              {"samples", rows},
              {"qualified_for_page_suppression", false},
              {"world_fps_evidence", false}}
             .dump(2);
  return out && missing == 0 ? 0 : 1;
}
