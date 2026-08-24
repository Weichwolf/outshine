#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "Check.h"

#include "MetalRoughBrdf.h"
#include "Readback.h"
#include "ShaderPrelude.h"

using outshine::Render::BrdfGeometry;
using outshine::Render::BrdfTerms;
using outshine::Render::kBrdfPi;
using outshine::Render::MslPrelude;

namespace {
[[nodiscard]] std::string Prelude() {
  std::string why;
  return MslPrelude(why);
}
} // namespace
using outshine::Render::MetalRoughBrdf;
using outshine::Render::MetalRoughBrdfMsl;
using outshine::Render::Readback;
using outshine::Render::ReadState;

namespace {

int DeviceErrors = 0;

bool Refused(const void *made, const char *what) {
  if (made) { return false; }
  ++DeviceErrors;
  std::printf("NOTE device refused %s: %s\n", what, SDL_GetError());
  return true;
}

struct SamplePoint {
  std::array<double, 3> DiffuseColour{};
  std::array<double, 3> F0{};

  double F90 = 1.0;
  double A2 = 0;
  BrdfGeometry At{};
};

constexpr uint32_t kInputFloats = 12;
constexpr uint32_t kOutputFloats = 6;

constexpr double kFloatEpsilon = 5.9604644775390625e-08;

constexpr double kRoundingSteps = 16.0;

constexpr double kNoiseFloor = 1.0e-6;

double AsFloat(double value) { return static_cast<double>(static_cast<float>(value)); }

double AllowedRelative(double a2, double nh) {
  if (a2 <= 0.0) { return kRoundingSteps * kFloatEpsilon; }
  const double denominator = nh * nh * (a2 - 1.0) + 1.0;
  return kRoundingSteps * kFloatEpsilon * (1.0 + 2.0 / denominator);
}

struct Vector {
  double X = 0, Y = 0, Z = 0;
};

double Dot(const Vector &left, const Vector &right) {
  return left.X * right.X + left.Y * right.Y + left.Z * right.Z;
}

Vector Normalised(const Vector &of) {
  const double length = std::sqrt(Dot(of, of));
  return {of.X / length, of.Y / length, of.Z / length};
}

Vector CosineDirection(uint32_t index, uint32_t count) {
  uint32_t bits = index;
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  const double first = (index + 0.5) / count;
  const double second = bits * 2.3283064365386963e-10;
  const double radius = std::sqrt(first);
  const double angle = 2.0 * kBrdfPi * second;
  return {radius * std::cos(angle), radius * std::sin(angle), std::sqrt(std::fmax(0.0, 1.0 - first))};
}

struct Material {
  std::array<double, 3> DiffuseColour;
  std::array<double, 3> F0;

  double F90;
};

const Material kMaterials[] = {
    {{0.8, 0.8, 0.8}, {0.04, 0.04, 0.04}, 1.0},
    {{0.2, 0.5, 0.9}, {0.04, 0.04, 0.04}, 0.5},
    {{0.8, 0.8, 0.8}, {0.0, 0.0, 0.0}, 0.0},
    {{0.0, 0.0, 0.0}, {0.955, 0.638, 0.538}, 1.0},
    {{0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, 1.0},
};

constexpr double kRoughnessSweep[] = {0.0, 0.05, 0.15, 0.3, 0.5, 0.75, 1.0};

constexpr double kViewCosineSweep[] = {1.0, 0.85, 0.5, 0.2, 0.05};
constexpr uint32_t kLightDirections = 32;

std::vector<SamplePoint> SampleSet(void) {
  std::vector<SamplePoint> points;
  for (const Material &material : kMaterials) {
    for (const double roughness : kRoughnessSweep) {
      for (const double nv : kViewCosineSweep) {
        const Vector view{std::sqrt(std::fmax(0.0, 1.0 - nv * nv)), 0, nv};
        for (uint32_t at = 0; at < kLightDirections; ++at) {
          const Vector light = CosineDirection(at, kLightDirections);
          const Vector half = Normalised({light.X + view.X, light.Y + view.Y, light.Z + view.Z});
          SamplePoint point;
          for (size_t channel = 0; channel < 3; ++channel) {
            point.DiffuseColour[channel] = AsFloat(material.DiffuseColour[channel]);
            point.F0[channel] = AsFloat(material.F0[channel]);
          }
          point.F90 = AsFloat(material.F90);
          const double alpha = roughness * roughness;
          point.A2 = AsFloat(alpha * alpha);
          point.At.Nl = AsFloat(light.Z);
          point.At.Nv = AsFloat(std::fmax(nv, 1.0e-6));
          point.At.Nh = AsFloat(std::fmax(half.Z, 0.0));
          point.At.Vh = AsFloat(std::fmax(Dot(view, half), 0.0));
          points.push_back(point);
        }
      }
    }
  }
  return points;
}

std::vector<float> Uploaded(const std::vector<SamplePoint> &points) {
  std::vector<float> floats;
  floats.reserve(points.size() * kInputFloats);
  for (const SamplePoint &point : points) {
    for (const double channel : point.DiffuseColour) { floats.push_back((float)channel); }
    for (const double channel : point.F0) { floats.push_back((float)channel); }
    floats.push_back((float)point.F90);
    floats.push_back((float)point.A2);
    floats.push_back((float)point.At.Nl);
    floats.push_back((float)point.At.Nv);
    floats.push_back((float)point.At.Nh);
    floats.push_back((float)point.At.Vh);
  }
  return floats;
}

std::string TieShader(const std::string &model) {
  char stride[128];
  std::snprintf(stride, sizeof stride, "constant uint kIn = %uu;\nconstant uint kOut = %uu;\n",
                kInputFloats, kOutputFloats);
  return Prelude() + model + std::string(stride) + R"(
struct Span { uint floats; };

kernel void tie(uint3 id [[thread_position_in_grid]],
                constant Span &span [[buffer(0)]],
                const device float *samples [[buffer(1)]],
                device float *results [[buffer(2)]]) {
  uint base = id.x * kIn;
  if (base + kIn > span.floats) { return; }
  float3 diffuseColour = float3(samples[base], samples[base + 1u], samples[base + 2u]);
  float3 f0 = float3(samples[base + 3u], samples[base + 4u], samples[base + 5u]);
  Brdf terms = metalRoughBrdf(diffuseColour, f0, samples[base + 6u], samples[base + 7u],
                              samples[base + 8u], samples[base + 9u], samples[base + 10u],
                              samples[base + 11u]);
  uint slot = id.x * kOut;
  results[slot] = terms.diffuse.x;
  results[slot + 1u] = terms.diffuse.y;
  results[slot + 2u] = terms.diffuse.z;
  results[slot + 3u] = terms.specular.x;
  results[slot + 4u] = terms.specular.y;
  results[slot + 5u] = terms.specular.z;
}
)";
}

std::string WithDoubledDistribution(const std::string &model) {
  const std::string site = "return a2 / (kPi";
  const size_t found = model.find(site);
  if (found == std::string::npos) { return std::string(); }
  return std::string(model).replace(found, site.size(), "return 2.0 * a2 / (kPi");
}

class Instrument {
public:
  ~Instrument() {
    if (Device) {
      SDL_DestroyGPUDevice(Device);
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
  }
  Instrument() {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { return; }
    Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
    if (!Device) { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
  }
  Instrument(const Instrument &) = delete;
  Instrument &operator=(const Instrument &) = delete;

  SDL_GPUDevice *Device = nullptr;
};

SDL_GPUBuffer *MakeBuffer(SDL_GPUDevice *device, SDL_GPUBufferUsageFlags usage, const float *from,
                          uint32_t bytes) {
  SDL_GPUBufferCreateInfo wanted{};
  wanted.usage = usage;
  wanted.size = bytes;
  SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &wanted);
  if (Refused(buffer, "a storage buffer")) { return nullptr; }
  if (!from) { return buffer; }

  SDL_GPUTransferBufferCreateInfo staging{};
  staging.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  staging.size = bytes;
  SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &staging);
  if (Refused(transfer, "an upload buffer")) { return buffer; }
  std::memcpy(SDL_MapGPUTransferBuffer(device, transfer, false), from, bytes);
  SDL_UnmapGPUTransferBuffer(device, transfer);
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTransferBufferLocation source{transfer, 0};
  SDL_GPUBufferRegion into{buffer, 0, bytes};
  SDL_UploadToGPUBuffer(copy, &source, &into, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  return buffer;
}

std::vector<float> RunOnDevice(const Instrument &on, const std::string &msl,
                               const std::vector<float> &input, size_t outputFloats) {
  const uint32_t inputBytes = (uint32_t)(input.size() * sizeof(float));
  const uint32_t outputBytes = (uint32_t)(outputFloats * sizeof(float));

  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(msl.c_str());
  wanted.code_size = msl.size();
  wanted.entrypoint = "tie";
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.num_uniform_buffers = 1;
  wanted.num_readonly_storage_buffers = 1;
  wanted.num_readwrite_storage_buffers = 1;
  wanted.threadcount_x = 64;
  wanted.threadcount_y = 1;
  wanted.threadcount_z = 1;
  SDL_GPUComputePipeline *pipeline = SDL_CreateGPUComputePipeline(on.Device, &wanted);
  if (Refused(pipeline, "the tie's compute pipeline")) { return {}; }

  SDL_GPUBuffer *samples =
      MakeBuffer(on.Device, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, input.data(), inputBytes);
  SDL_GPUBuffer *results =
      MakeBuffer(on.Device, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, nullptr, outputBytes);
  std::vector<float> out;
  if (samples && results) {
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(on.Device);
    SDL_GPUStorageBufferReadWriteBinding written{};
    written.buffer = results;
    SDL_GPUComputePass *pass = SDL_BeginGPUComputePass(commands, nullptr, 0, &written, 1);
    SDL_BindGPUComputePipeline(pass, pipeline);
    SDL_BindGPUComputeStorageBuffers(pass, 0, &samples, 1);
    const uint32_t floats = (uint32_t)input.size();
    SDL_PushGPUComputeUniformData(commands, 0, &floats, sizeof floats);
    SDL_DispatchGPUCompute(pass, (uint32_t)((outputFloats / kOutputFloats + 63) / 64), 1, 1);
    SDL_EndGPUComputePass(pass);
    SDL_SubmitGPUCommandBuffer(commands);

    Readback read;
    if (read.FromBuffer(on.Device, results, outputBytes) == ReadState::Ready) {
      out.resize(outputFloats);
      std::memcpy(out.data(), read.Rows(), outputBytes);
    } else {
      ++DeviceErrors;
    }
  }
  SDL_ReleaseGPUBuffer(on.Device, samples);
  SDL_ReleaseGPUBuffer(on.Device, results);
  SDL_ReleaseGPUComputePipeline(on.Device, pipeline);
  return out;
}

enum class Half { Diffuse, Specular };

struct Agreement {
  double WorstRatio = 0;
  double WorstRelative = 0;
  int Compared = 0;
  int Disagreeing = 0;
  int SpecularAbove = 0;
  int SpecularDisagreeing = 0;
  int LooseSamples = 0;
};

void Offer(Agreement &into, Half half, double reference, double measured, double allowedRelative) {
  const double difference = std::fabs(measured - reference);
  const double allowed = allowedRelative * std::fabs(reference) + kNoiseFloor * kFloatEpsilon;
  const double ratio = difference / allowed;
  const double magnitude = std::fmax(std::fabs(reference), kNoiseFloor);
  ++into.Compared;
  into.WorstRatio = std::fmax(into.WorstRatio, ratio);
  into.WorstRelative = std::fmax(into.WorstRelative, difference / magnitude);
  if (ratio > 1.0) { ++into.Disagreeing; }
  if (half != Half::Specular || std::fabs(reference) <= kNoiseFloor) { return; }
  ++into.SpecularAbove;
  if (ratio > 1.0) { ++into.SpecularDisagreeing; }
}

Agreement Compare(const std::vector<SamplePoint> &points, const std::vector<float> &measured) {
  Agreement found;
  for (size_t at = 0; at < points.size(); ++at) {
    const SamplePoint &point = points[at];
    const BrdfTerms host = MetalRoughBrdf(point.DiffuseColour, point.F0, point.F90, point.A2, point.At);
    const double allowedRelative = AllowedRelative(point.A2, point.At.Nh);
    if (allowedRelative > 0.01) { ++found.LooseSamples; }
    const size_t slot = at * kOutputFloats;
    for (size_t channel = 0; channel < 3; ++channel) {
      Offer(found, Half::Diffuse, host.Diffuse[channel], measured[slot + channel], allowedRelative);
      Offer(found, Half::Specular, host.Specular[channel], measured[slot + 3 + channel],
            allowedRelative);
    }
  }
  return found;
}

void ReportAgreement(const char *what, const Agreement &found) {
  std::printf("TIE %s: %d channels compared, %d disagreeing, worst ratio %.9g, worst relative "
              "%.9g\n",
              what, found.Compared, found.Disagreeing, found.WorstRatio, found.WorstRelative);
}

}

int main() {
  const std::vector<SamplePoint> points = SampleSet();
  const std::vector<float> input = Uploaded(points);
  const size_t outputFloats = points.size() * kOutputFloats;

  const Instrument on;
  CHECK(on.Device != nullptr, "a device answers, so the MSL half can be evaluated at all");
  if (!on.Device) { return outshine::Test::Report(); }

  const std::vector<float> asEmitted =
      RunOnDevice(on, TieShader(MetalRoughBrdfMsl()), input, outputFloats);
  CHECK(asEmitted.size() == outputFloats,
        "the emitted MSL compiles and returns one result per sample");
  if (asEmitted.size() != outputFloats) { return outshine::Test::Report(); }

  const Agreement emitted = Compare(points, asEmitted);
  ReportAgreement("as emitted", emitted);
  outshine::Test::Note("samples in the sweep", (double)points.size(), "count");
  outshine::Test::Note("worst difference over the allowance", emitted.WorstRatio, "dimensionless");
  outshine::Test::Note("worst relative difference", emitted.WorstRelative, "dimensionless");
  outshine::Test::Note("samples whose f32 allowance exceeds 1 part in 100",
                       (double)emitted.LooseSamples, "count");
  CHECK(emitted.Disagreeing == 0,
        "every channel of both terms agrees with the C++ half inside the f32 error the sample's own "
        "conditioning admits");

  const std::string mutated = WithDoubledDistribution(MetalRoughBrdfMsl());
  CHECK(!mutated.empty(), "the mutation's site is still in the emitted MSL, so the control applies");
  const std::vector<float> asMutated =
      mutated.empty() ? std::vector<float>() : RunOnDevice(on, TieShader(mutated), input, outputFloats);
  CHECK(asMutated.size() == outputFloats, "the mutated MSL compiles and returns one result per sample");
  if (asMutated.size() == outputFloats) {
    const Agreement scaled = Compare(points, asMutated);
    ReportAgreement("distribution scaled by two", scaled);
    CHECK(scaled.SpecularAbove > 0, "the sweep carries specular terms above the noise floor at all");
    CHECK(scaled.SpecularDisagreeing == scaled.SpecularAbove,
          "a distribution scaled by two disagrees at every specular channel above the noise floor -- "
          "the tie can see a scaled D");
  }

  CHECK(DeviceErrors == 0, "the device reported no error over either dispatch");
  outshine::Test::Covers("I.26.12 the shading model's two halves: the MSL the fragment shader is "
                         "spliced from evaluates the same arrangement of terms as the C++ the white "
                         "furnace integrates, over a shared sample set, on the device");
  return outshine::Test::Report();
}
