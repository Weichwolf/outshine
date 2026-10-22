#include "GpuSupport.h"
#include "Support.h"
#include "Envelope.h"
#include "EnvelopeChecks.h"
#include "Input.h"
#include <expected>
#include <ratio>
#include "device/native/Device.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace N = outshine::Render::Native;
namespace C = outshine::Diagnostics::Calibration;
using Json = nlohmann::json;

namespace {
constexpr uint32_t Width = 64;
constexpr uint64_t TransferBytes = uint64_t{Width} * 4 * sizeof(float);
using Projection::Floats;
using Projection::Input;
using Projection::InvalidInput;
using Projection::Matrix;
using Projection::Shader;
using Projection::Take;
using Projection::Vector;

struct Case {
  std::string Name;
  Input Values;
  bool Native = false;
};

struct Capture {
  Vector Value{};
  double GpuNs = 0;
  bool HasGpuTime = false;
};

struct Probe {
  std::unique_ptr<N::Device> Device;
  std::unique_ptr<N::GraphicsPipeline> Pipeline;
  std::unique_ptr<N::Texture> Target;
  std::unique_ptr<N::Buffer> Readback;
};

Probe Create(const std::filesystem::path &folder) {
  Probe probe;
  probe.Device =
      Take(N::CreateDevice({.Validation = true, .GpuTiming = N::GpuTimingMode::Submission}));
  const auto vertex = Shader(folder / "projection.vert.spv");
  const auto fragment = Shader(folder / "projection.frag.spv");
  N::ColourTarget colour;
  colour.Texels = N::Format::Rgba32Float;
  const std::array colours{colour};
  N::GraphicsDescription description;
  description.Vertex.Spirv = vertex;
  description.Fragment = N::ShaderCode{.Spirv = fragment};
  description.Colours = colours;
  description.Culling = N::Cull::None;
  probe.Pipeline = Take(probe.Device->CreateGraphicsPipeline(description));
  probe.Target = Take(probe.Device->CreateTexture(
      {.Width = Width,
       .Height = 1,
       .Texels = N::Format::Rgba32Float,
       .Use = N::TextureUse::ColourAttachment | N::TextureUse::CopySource}));
  probe.Readback = Take(probe.Device->CreateBuffer({.Bytes = TransferBytes,
                                                    .Use = N::BufferUse::CopyDestination,
                                                    .Placement = N::Memory::Readback}));
  return probe;
}

Capture Draw(Probe &probe, const Input &input) {
  const auto timing = Take(C::Measure(*probe.Device, [&](N::Commands &commands) -> N::Result<void> {
    const std::array colours{N::ColourAttachment{
        .Image = {.Image = probe.Target.get()}, .Begin = N::Load::Clear, .End = N::Store::Store}};
    N::RenderPassDescription pass;
    pass.Colours = colours;
    auto begun = commands.BeginRenderPass(pass);
    if (!begun) { return begun; }
    const std::array bindings{N::Binding{
        .Set = 1, .Slot = 0, .Value = N::Uniform{.Bytes = std::as_bytes(std::span(&input, 1))}}};
    N::DrawDescription draw;
    draw.State.Pipeline = probe.Pipeline.get();
    draw.State.Bindings = bindings;
    draw.State.Area.Width = Width;
    draw.State.Area.Height = 1;
    draw.State.Clip.Width = Width;
    draw.State.Clip.Height = 1;
    draw.Range.Count = 3;
    auto drawn = commands.Draw(draw);
    if (!drawn) { return drawn; }
    return commands.EndRenderPass();
  }));
  auto commands = Take(probe.Device->Begin());
  N::TextureTransfer transfer;
  transfer.RowBytes = TransferBytes;
  transfer.Region.Width = Width;
  transfer.Region.Height = 1;
  Take(commands->Copy(*probe.Target, *probe.Readback, transfer));
  Take(C::Finish(*probe.Device, std::move(commands)));
  const auto bytes = Take(probe.Device->MapRead(*probe.Readback, {.Bytes = TransferBytes}));
  Capture capture;
  std::memcpy(capture.Value.data(), bytes.data(), sizeof(capture.Value));
  Take(probe.Device->Unmap(*probe.Readback));
  capture.HasGpuTime = timing.GpuNs.has_value();
  if (timing.GpuNs) { capture.GpuNs = *timing.GpuNs; }
  return capture;
}

Matrix Identity() {
  Matrix matrix{};
  for (size_t i = 0; i < 4; ++i) { matrix[i * 5] = 1; }
  return matrix;
}

Vector Multiply(const Matrix &m, const Vector &p, bool fused) {
  Vector result{};
  for (size_t row = 0; row < 4; ++row) {
    if (fused) {
      float sum = m[12 + row] * p[3];
      sum = std::fma(m[8 + row], p[2], sum);
      sum = std::fma(m[4 + row], p[1], sum);
      result[row] = std::fma(m[row], p[0], sum);
    } else {
      result[row] = m[row] * p[0] + m[4 + row] * p[1] + m[8 + row] * p[2] + m[12 + row] * p[3];
    }
  }
  return result;
}

Vector Reference(const Input &input) {
  Matrix model = input.Model;
  const auto mode = input.Options[0];
  if (mode == 1) {
    for (size_t i = 0; i < 3; ++i) { model[12 + i] += input.Shift[i]; }
    return Multiply(input.View, Multiply(model, input.Point, false), false);
  }
  auto world = Multiply(model, input.Point, mode == 2);
  for (size_t i = 0; i < 3; ++i) { world[i] += input.Shift[i]; }
  return Multiply(input.View, world, mode == 2);
}

std::vector<Case> SyntheticCases() {
  std::vector<Case> cases;
  constexpr int firstExponent = 20;
  constexpr int lastExponent = 25;
  for (int exponent = firstExponent; exponent <= lastExponent; ++exponent) {
    Case fixture{.Name = "shift-cancellation-" + std::to_string(exponent), .Values = {}};
    fixture.Values.Model = Identity();
    fixture.Values.View = Identity();
    fixture.Values.Model[12] = std::ldexp(1.f, exponent);
    fixture.Values.Point = {-fixture.Values.Model[12], 0, 0, 1};
    fixture.Values.Shift[0] = .5f;
    cases.push_back(fixture);
  }
  for (int offset = 1; offset <= 8; ++offset) {
    Case fixture{.Name = "fused-cancellation-" + std::to_string(offset), .Values = {}};
    fixture.Values.Model = Identity();
    fixture.Values.View = Identity();
    fixture.Values.Model[0] = std::nextafter(1.f, 2.f);
    constexpr float base = 8388608;
    fixture.Values.Point = {base + static_cast<float>(offset), 0, 0, 1};
    fixture.Values.Model[12] = -(base + static_cast<float>(offset) + 1);
    cases.push_back(fixture);
  }
  return cases;
}

std::vector<Case> NativeCases(const std::filesystem::path &path) {
  std::vector<Case> cases;
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  constexpr std::streamoff Limit = std::streamoff{16} * 1024 * 1024;
  if (!file || file.tellg() < 0 || file.tellg() > Limit) { std::_Exit(2); }
  file.seekg(0);
  const auto source = Json::parse(file, nullptr, false);
  constexpr size_t MeshLimit = 512;
  if (source.is_discarded() || !source.is_object() || !source.contains("meshes") ||
      !source["meshes"].is_array() || source["meshes"].size() > MeshLimit ||
      !source.contains("qualified_for_page_suppression") ||
      source["qualified_for_page_suppression"] != false || !source.contains("mvp_f32") ||
      !source.contains("shift_f32")) {
    InvalidInput("invalid calibration input");
  }
  const auto view = Floats<16>(source["mvp_f32"]);
  const auto shift = Floats<3>(source["shift_f32"]);
  for (const auto &mesh : source["meshes"]) {
    if (!mesh.is_object() || !mesh.contains("positions_f32") || !mesh["positions_f32"].is_array() ||
        !mesh.contains("model_f32") || !mesh.contains("id") || !mesh["id"].is_number_integer() ||
        mesh["id"] < 0 || mesh["id"] > std::numeric_limits<uint32_t>::max()) {
      InvalidInput("invalid mesh fields");
    }
    const auto model = Floats<16>(mesh["model_f32"]);
    const auto count = mesh["positions_f32"].size();
    if (count == 0) { continue; }
    for (const auto at : std::array<size_t, 3>{0, count / 2, count - 1}) {
      Case fixture{.Name = "mesh-" + std::to_string(mesh["id"].get<uint32_t>()) + "-vertex-" +
                           std::to_string(at),
                   .Values = {},
                   .Native = true};
      fixture.Values.Model = model;
      fixture.Values.View = view;
      const auto point = Floats<3>(mesh["positions_f32"][at]);
      for (size_t i = 0; i < 3; ++i) {
        fixture.Values.Shift[i] = shift[i];
        fixture.Values.Point[i] = point[i];
      }
      fixture.Values.Point[3] = 1;
      cases.push_back(fixture);
    }
  }
  return cases;
}

std::array<uint32_t, 4> Bits(const Vector &value) {
  std::array<uint32_t, 4> result{};
  for (size_t i = 0; i < 4; ++i) { result[i] = std::bit_cast<uint32_t>(value[i]); }
  return result;
}

struct Metrics {
  std::array<uint64_t, 4> Different{};
  std::array<uint64_t, 4> NativeDifferent{};
  std::array<uint64_t, 4> Components{};
  std::array<uint64_t, 4> NumericDifferent{};
  std::array<uint64_t, 4> UnsafeNativeDepthBounds{};
  std::array<uint64_t, 4> ComparableNativeDepthBounds{};
  std::array<double, 4> GpuNs{};
  uint64_t Missing = 0;
  uint64_t EnvelopeEscapes = 0;
  uint64_t EnclosedSamples = 0;
  uint64_t EnvelopeDepthComparable = 0;
  uint64_t EnvelopeDepthUnsafe = 0;
};

bool ComparableDepth(const Vector &point) {
  return std::isfinite(point[2]) && std::isfinite(point[3]) && point[3] > 0 && point[2] >= 0 &&
         point[2] <= point[3];
}

std::optional<bool> CornerBoundAbove(const Vector &cpu, const Vector &gpu) {
  if (!ComparableDepth(cpu) || !ComparableDepth(gpu)) { return {}; }
  auto bound = static_cast<float>(static_cast<double>(cpu[2]) / cpu[3]);
  if (static_cast<double>(bound) * cpu[3] > cpu[2]) { bound = std::nextafter(bound, 0.f); }
  if (static_cast<double>(bound) * cpu[3] > cpu[2]) { return {}; }
  return static_cast<double>(bound) * gpu[3] > gpu[2];
}

void CheckEnvelope(const Capture &gpu,
                   const Case &fixture,
                   const std::expected<Projection::Envelope, Projection::EnvelopeUnknown> &envelope,
                   Metrics &totals) {
  if (envelope) {
    ++totals.EnclosedSamples;
    for (size_t component = 0; component < 4; ++component) {
      const auto bounds = envelope->Clip[component];
      const double value = gpu.Value[component];
      totals.EnvelopeEscapes +=
          !std::isfinite(value) || value < bounds.Lower || value > bounds.Upper ? 1u : 0u;
    }
  }
  if (envelope) {
    const auto ndc = Projection::Divide(*envelope);
    if (fixture.Native && ndc && ComparableDepth(gpu.Value) && (*ndc)[2].Lower >= 0) {
      ++totals.EnvelopeDepthComparable;
      const auto lower = (*ndc)[2].Lower;
      auto depthBound = static_cast<float>(lower);
      if (depthBound > lower) { depthBound = std::nextafter(depthBound, 0.f); }
      totals.EnvelopeDepthUnsafe +=
          static_cast<double>(depthBound) * gpu.Value[3] > gpu.Value[2] ? 1u : 0u;
    }
  }
}

Json Observe(Probe &probe,
             const Case &fixture,
             uint32_t mode,
             Metrics &totals,
             const std::expected<Projection::Envelope, Projection::EnvelopeUnknown> &envelope) {
  auto input = fixture.Values;
  input.Options[0] = mode;
  const auto cpu = Reference(input);
  const auto gpu = Draw(probe, input);
  const auto cpuBits = Bits(cpu);
  const auto gpuBits = Bits(gpu.Value);
  const bool unequal = cpuBits != gpuBits;
  totals.Different[mode] += unequal ? 1u : 0u;
  totals.NativeDifferent[mode] += unequal && fixture.Native ? 1u : 0u;
  for (size_t i = 0; i < 4; ++i) { totals.Components[mode] += cpuBits[i] != gpuBits[i] ? 1u : 0u; }
  const bool numeric = !std::ranges::equal(cpu, gpu.Value);
  totals.NumericDifferent[mode] += numeric ? 1u : 0u;
  const auto depth = fixture.Native ? CornerBoundAbove(cpu, gpu.Value) : std::nullopt;
  totals.ComparableNativeDepthBounds[mode] += depth.has_value() ? 1u : 0u;
  totals.UnsafeNativeDepthBounds[mode] += depth.value_or(false) ? 1u : 0u;
  totals.GpuNs[mode] += gpu.GpuNs;
  totals.Missing += gpu.HasGpuTime ? 0u : 1u;
  CheckEnvelope(gpu, fixture, envelope, totals);
  return {{"name", fixture.Name},
          {"native_input", fixture.Native},
          {"mode", mode},
          {"cpu", cpu},
          {"gpu", gpu.Value},
          {"cpu_bits", cpuBits},
          {"gpu_bits", gpuBits},
          {"different", unequal},
          {"numeric_different", numeric},
          {"cpu_corner_bound_above_gpu_depth", depth ? Json(*depth) : Json(nullptr)}};
}

Json Enclosure(const Case &fixture,
               const std::expected<Projection::Envelope, Projection::EnvelopeUnknown> &envelope,
               double ns) {
  Json bounds = Json::array();
  if (envelope) {
    for (const auto &value : envelope->Clip) { bounds.push_back({value.Lower, value.Upper}); }
  }
  Json result{{"name", fixture.Name},
              {"native_input", fixture.Native},
              {"model", fixture.Values.Model},
              {"view", fixture.Values.View},
              {"shift", fixture.Values.Shift},
              {"point", fixture.Values.Point},
              {"clip", bounds},
              {"cpu_ns", ns},
              {"unknown", envelope ? Json(nullptr) : Json(static_cast<int>(envelope.error()))}};
  if (envelope) {
    const auto &work = envelope->Work;
    result["work"] = {{"inputs", work.Inputs},
                      {"products", work.Products},
                      {"sums", work.Sums},
                      {"outward_steps", work.OutwardSteps},
                      {"divisions", work.Divisions}};
    const auto divided = Projection::Divide(*envelope);
    if (divided) {
      result["ndc"] = Json::array();
      for (const auto &value : *divided) { result["ndc"].push_back({value.Lower, value.Upper}); }
    }
  }
  return result;
}

}

int main(int argc, char *const *argv) {
  if (argc != 4) { return 2; }
  if (!Projection::CheckEnvelopeDomain()) { return 1; }
  auto cases = SyntheticCases();
  for (auto &native : NativeCases(argv[2])) { cases.push_back(std::move(native)); }
  auto probe = Create(argv[1]);
  Json rows = Json::array();
  Metrics totals;
  Json enclosures = Json::array();
  for (const auto &fixture : cases) {
    const auto began = std::chrono::steady_clock::now();
    const auto envelope = Projection::Enclose(fixture.Values);
    const auto ns =
        std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - began).count();
    enclosures.push_back(Enclosure(fixture, envelope, ns));
    for (uint32_t mode = 0; mode < 4; ++mode) {
      rows.push_back(Observe(probe, fixture, mode, totals, envelope));
    }
  }
  std::ofstream out(argv[3]);
  out << Json{{"device", probe.Device->Limits().Name},
              {"case_count", cases.size()},
              {"modes", 4},
              {"different_cases", totals.Different},
              {"different_native_cases", totals.NativeDifferent},
              {"different_components", totals.Components},
              {"numeric_different_cases", totals.NumericDifferent},
              {"cpu_corner_bounds_above_gpu_depth", totals.UnsafeNativeDepthBounds},
              {"comparable_native_depth_bounds", totals.ComparableNativeDepthBounds},
              {"draws", cases.size() * 4},
              {"submitted_vertices", cases.size() * 4 * 3},
              {"logical_uniform_bytes", cases.size() * 4 * sizeof(Input)},
              {"readback_bytes", cases.size() * 4 * TransferBytes},
              {"gpu_ns", totals.GpuNs},
              {"missing_gpu_samples", totals.Missing},
              {"rows", rows},
              {"enclosures", enclosures},
              {"envelope_domain_checks", true},
              {"envelope_value_bytes", sizeof(Projection::Envelope)},
              {"envelope_result_bytes",
               sizeof(std::expected<Projection::Envelope, Projection::EnvelopeUnknown>)},
              {"enclosed_samples", totals.EnclosedSamples},
              {"envelope_escapes", totals.EnvelopeEscapes},
              {"envelope_depth_comparable", totals.EnvelopeDepthComparable},
              {"envelope_depth_unsafe", totals.EnvelopeDepthUnsafe},
              {"enclosure_model",
               "Positive add/multiply normal forms; 20 monomials, degree 3. General "
               "compiler-rewrite completeness remains unproved."},
              {"qualified_for_page_suppression", false},
              {"scope",
               "Vertex-stage arithmetic diagnostic sharing extracted native position expressions. "
               "Transport through flat varying/RGBA32F; not an actual world shader output capture "
               "or a portable precision bound."}}
             .dump(2);
  return out && totals.Missing == 0 && totals.EnvelopeEscapes == 0 &&
                 totals.EnvelopeDepthUnsafe == 0 && totals.EnclosedSamples > 0
             ? 0
             : 1;
}
