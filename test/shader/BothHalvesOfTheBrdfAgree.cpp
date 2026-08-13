/* THE TIE BETWEEN THE TWO HALVES OF THE BRDF. `stages/MetalRoughBrdf.h` states glTF's metal-rough
 * model twice -- once in C++, once in the WGSL the fragment shader is spliced from -- and until this
 * file existed nothing evaluated both. The white furnace integrates the C++ half, so its verdict was
 * about a function the renderer does not run; the shader was pinned to it only through
 * `directional-light`'s hue check, which is scale-invariant and would not catch a scaled D.
 *
 * WHAT THIS MEASURES IS THE ARRANGEMENT OF THE TERMS, which is the half the emitted constants cannot
 * protect. The same sample set goes through both halves and every channel of both terms is compared.
 *
 * THE COMPARISON CARRIES ITS OWN NEGATIVE CONTROL. A tie that cannot see the defect the asset is
 * named for is the instrument this round already rejected once, so the emitted WGSL is mutated here
 * -- the distribution scaled by two -- and the same comparison must go red over it. The mutation is
 * a textual substitution into the emitted text and it names its site: if the site stops existing,
 * this refuses rather than passing over a mutation that was never applied.
 *
 * THE ALLOWANCE IS DERIVED PER SAMPLE AND NOT CHOSEN. The device computes in f32 and the reference
 * in f64, so the two cannot agree exactly, and one flat tolerance over the whole domain would be
 * decided by the worst-conditioned corner. The conditioning is knowable: the distribution's
 * denominator is `nh^2 (a2 - 1) + 1`, a difference of two quantities near 1 whose f32 absolute error
 * is therefore about eps whatever the result is, and D depends on its square -- so the relative
 * error of D is about `2 * eps / denominator`, which is 3e-7 at a rough surface and 15 % at the
 * sharpest lobe in the sweep. That is the physics of f32 at a Dirac-adjacent lobe rather than a
 * concession, and it is published per sweep: `AllowedRelative` returns it and the report prints how
 * many samples it exceeds one part in a hundred at. Every mutation this tie is for is a factor, not
 * a last-bit difference: a scaled D is 100 % at every sample, three orders above the worst
 * allowance and seven above the typical one. */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <webgpu/webgpu_cpp.h>

#include "Check.h"

#include "MetalRoughBrdf.h"
#include "Readback.h"

using outshine::Render::BrdfGeometry;
using outshine::Render::BrdfTerms;
using outshine::Render::kBrdfPi;
using outshine::Render::MetalRoughBrdf;
using outshine::Render::MetalRoughBrdfWGSL;
using outshine::Render::Readback;
using outshine::Render::ReadState;

namespace {

/* WHAT THE DEVICE SAID WENT WRONG, and it is a counter at namespace scope for the reason `Check.h`
 * names for its own: an uncaptured-error callback outlives every scope a test could hold it in. */
int DeviceErrors = 0;

std::string Spelled(wgpu::StringView view) {
  return view.data ? std::string(view.data, view.length) : std::string();
}

/* ONE SHADING POINT AS BOTH HALVES TAKE IT: the two colours the model is evaluated with, the squared
 * alpha, and the four clamped cosines. Every value here is already rounded to f32, because the
 * device gets f32 and a reference evaluated on wider inputs would be measuring the upload. */
struct SamplePoint {
  std::array<double, 3> DiffuseColour{};
  std::array<double, 3> F0{};
  double A2 = 0;
  BrdfGeometry At{};
};

/* THE UPLOAD LAYOUT, ELEVEN FLOATS PER SAMPLE, and the shader reads the stride from this constant
 * rather than from a second spelling of the number -- the same rule the model's own constants follow
 * (`MetalRoughBrdf.h`). */
constexpr uint32_t kInputFloats = 11;
constexpr uint32_t kOutputFloats = 6;   /* diffuse rgb, then specular rgb */

constexpr double kFloatEpsilon = 5.9604644775390625e-08;   /* 2^-24, half an ulp at 1.0 */
/* [SET] The number of rounded operations the widest path through the model performs, generously
 * counted: five in the visibility term including its two square roots, four in the Fresnel weight,
 * three in the distribution outside its denominator, and the products that combine them. */
constexpr double kRoundingSteps = 16.0;
/* [SET] Below this magnitude a term is noise in either precision, and a relative difference over it
 * would be a division by rounding. The model's terms run from about 1e-2 to 1e5 in these sweeps. */
constexpr double kNoiseFloor = 1.0e-6;

double AsFloat(double value) { return static_cast<double>(static_cast<float>(value)); }

/* The f32 relative error this sample's own conditioning admits. `a2 = 0` takes the arm with no
 * distribution at all, where the specular term is exactly zero in both halves. */
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

/* Hammersley, so the sweep is the same set on every run and a difference between two rounds is a
 * change in the subject rather than in a seed. */
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

/* THE MATERIALS THE MODEL IS EVALUATED WITH, chosen so no channel can stand in for another: a
 * chromatic dielectric would hide a Fresnel written on the wrong channel, and the perfect reflector
 * is the configuration the white furnace states its claim in. These are the model's OWN inputs and
 * not a metalness remapping -- `shade()` in `SubjectDraw.cpp` performs that remapping and it is a
 * different subject from the one this file ties. */
struct Material {
  std::array<double, 3> DiffuseColour;
  std::array<double, 3> F0;
};

const Material kMaterials[] = {
    {{0.8, 0.8, 0.8}, {0.04, 0.04, 0.04}},
    {{0.2, 0.5, 0.9}, {0.04, 0.04, 0.04}},
    {{0.0, 0.0, 0.0}, {0.955, 0.638, 0.538}},
    {{0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}},
};
/* Both ends included: 0 is the delta-lobe arm the model states for itself, 1 is glTF's maximum. */
constexpr double kRoughnessSweep[] = {0.0, 0.05, 0.15, 0.3, 0.5, 0.75, 1.0};
/* Head-on down to 87 degrees off the normal, which is where Schlick's Fresnel approaches one. */
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
    floats.push_back((float)point.A2);
    floats.push_back((float)point.At.Nl);
    floats.push_back((float)point.At.Nv);
    floats.push_back((float)point.At.Nh);
    floats.push_back((float)point.At.Vh);
  }
  return floats;
}

/* The entry point the model's own text is evaluated through. It declares no term of its own: it
 * unpacks a sample, calls `metalRoughBrdf` and writes what came back. */
std::string TieShader(const std::string &model) {
  char stride[128];
  std::snprintf(stride, sizeof stride, "const kIn : u32 = %uu;\nconst kOut : u32 = %uu;\n",
                kInputFloats, kOutputFloats);
  return model + std::string(stride) + R"(
@group(0) @binding(0) var<storage, read> samples : array<f32>;
@group(0) @binding(1) var<storage, read_write> results : array<f32>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id : vec3u) {
  let base = id.x * kIn;
  if (base + kIn > arrayLength(&samples)) { return; }
  let diffuseColour = vec3f(samples[base], samples[base + 1u], samples[base + 2u]);
  let f0 = vec3f(samples[base + 3u], samples[base + 4u], samples[base + 5u]);
  let out = metalRoughBrdf(diffuseColour, f0, samples[base + 6u], samples[base + 7u],
                           samples[base + 8u], samples[base + 9u], samples[base + 10u]);
  let slot = id.x * kOut;
  results[slot] = out.diffuse.x;
  results[slot + 1u] = out.diffuse.y;
  results[slot + 2u] = out.diffuse.z;
  results[slot + 3u] = out.specular.x;
  results[slot + 4u] = out.specular.y;
  results[slot + 5u] = out.specular.z;
}
)";
}

/* THE NEGATIVE CONTROL'S SITE, named rather than guessed: the numerator of the distribution. An
 * empty return means the site is gone, and then the mutation was never applied -- which this refuses
 * over rather than passing. */
std::string WithDoubledDistribution(const std::string &model) {
  const std::string site = "return a2 / (kPi";
  const size_t found = model.find(site);
  if (found == std::string::npos) { return std::string(); }
  return std::string(model).replace(found, site.size(), "return 2.0 * a2 / (kPi");
}

struct Instrument {
  wgpu::Instance Instance;
  wgpu::Device Device;
  wgpu::Queue Queue;
};

/* `TimedWaitAny` is what makes `WaitAny(UINT64_MAX)` legal; without it it returns Error before it
 * looks at the future. */
wgpu::Instance MakeInstance(void) {
  wgpu::InstanceDescriptor descriptor{};
  static constexpr auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
  descriptor.requiredFeatureCount = 1;
  descriptor.requiredFeatures = &kTimedWaitAny;
  return wgpu::CreateInstance(&descriptor);
}

Instrument BringUp(void) {
  Instrument made;
  made.Instance = MakeInstance();
  if (!made.Instance) { return made; }
  wgpu::Adapter adapter;
  wgpu::RequestAdapterOptions options{};
  made.Instance.WaitAny(
      made.Instance.RequestAdapter(
          &options, wgpu::CallbackMode::WaitAnyOnly,
          [&adapter](wgpu::RequestAdapterStatus status, wgpu::Adapter got, wgpu::StringView) {
            if (status == wgpu::RequestAdapterStatus::Success) { adapter = got; }
          }),
      UINT64_MAX);
  if (!adapter) { return made; }
  wgpu::DeviceDescriptor descriptor{};
  descriptor.SetUncapturedErrorCallback(
      [](const wgpu::Device &, wgpu::ErrorType type, wgpu::StringView message) {
        ++DeviceErrors;
        std::printf("NOTE device error %d: %s\n", (int)type, Spelled(message).c_str());
      });
  made.Instance.WaitAny(
      adapter.RequestDevice(
          &descriptor, wgpu::CallbackMode::WaitAnyOnly,
          [&made](wgpu::RequestDeviceStatus status, wgpu::Device got, wgpu::StringView) {
            if (status == wgpu::RequestDeviceStatus::Success) { made.Device = got; }
          }),
      UINT64_MAX);
  if (made.Device) { made.Queue = made.Device.GetQueue(); }
  return made;
}

wgpu::Buffer MakeBuffer(const wgpu::Device &device, uint64_t bytes, wgpu::BufferUsage usage) {
  wgpu::BufferDescriptor descriptor{};
  descriptor.size = bytes;
  descriptor.usage = usage;
  return device.CreateBuffer(&descriptor);
}

wgpu::ComputePipeline MakePipeline(const wgpu::Device &device, const std::string &wgsl) {
  wgpu::ShaderSourceWGSL source{};
  source.code = wgsl.c_str();
  wgpu::ShaderModuleDescriptor module{};
  module.nextInChain = &source;
  wgpu::ComputePipelineDescriptor descriptor{};
  descriptor.compute.module = device.CreateShaderModule(&module);
  descriptor.compute.entryPoint = "main";
  return device.CreateComputePipeline(&descriptor);
}

/* [SET] A cap rather than a wait: a map that never lands is a defect to report, not a run to hang.
 * One dispatch of a few thousand invocations retires in a handful of polls. */
constexpr int kPollCap = 100000;

std::vector<float> RunOnDevice(const Instrument &on, const std::string &wgsl,
                               const std::vector<float> &input, size_t outputFloats) {
  const uint64_t inputBytes = input.size() * sizeof(float);
  const uint64_t outputBytes = outputFloats * sizeof(float);
  const wgpu::Buffer samples =
      MakeBuffer(on.Device, inputBytes, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
  const wgpu::Buffer results =
      MakeBuffer(on.Device, outputBytes, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
  on.Queue.WriteBuffer(samples, 0, input.data(), inputBytes);

  const wgpu::ComputePipeline pipeline = MakePipeline(on.Device, wgsl);
  if (!pipeline) { return {}; }
  const std::array<wgpu::BindGroupEntry, 2> bound{
      wgpu::BindGroupEntry{nullptr, 0, samples, 0, inputBytes, nullptr, nullptr},
      wgpu::BindGroupEntry{nullptr, 1, results, 0, outputBytes, nullptr, nullptr}};
  wgpu::BindGroupDescriptor group{};
  group.layout = pipeline.GetBindGroupLayout(0);
  group.entryCount = bound.size();
  group.entries = bound.data();

  wgpu::CommandEncoder encoder = on.Device.CreateCommandEncoder();
  wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
  pass.SetPipeline(pipeline);
  pass.SetBindGroup(0, on.Device.CreateBindGroup(&group));
  pass.DispatchWorkgroups((uint32_t)((outputFloats / kOutputFloats + 63) / 64), 1, 1);
  pass.End();
  wgpu::CommandBuffer commands = encoder.Finish();
  on.Queue.Submit(1, &commands);

  Readback read;
  read.FromBuffer(on.Device, on.Queue, results, outputBytes);
  ReadState state = ReadState::Pending;
  for (int poll = 0; poll < kPollCap && state == ReadState::Pending; ++poll) {
    state = read.Poll(on.Instance);
  }
  if (state != ReadState::Ready) {
    read.Release();
    return {};
  }
  std::vector<float> out(outputFloats);
  std::copy_n((const float *)read.Rows(), outputFloats, out.begin());
  read.Release();
  return out;
}

/* WHICH TERM A CHANNEL BELONGS TO. The counts the negative control is judged on are about the
 * specular half alone -- a scaled distribution leaves the diffuse term untouched, and a count over
 * both halves would report half of them agreeing and read like a partial failure. */
enum class Half { Diffuse, Specular };

struct Agreement {
  double WorstRatio = 0;        /* difference over what the sample's conditioning allows */
  double WorstRelative = 0;     /* dimensionless, at the worst channel */
  int Compared = 0;
  int Disagreeing = 0;
  int SpecularAbove = 0;        /* specular channels whose reference exceeds the noise floor */
  int SpecularDisagreeing = 0;
  int LooseSamples = 0;         /* samples whose allowance exceeds one part in a hundred */
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
    const BrdfTerms host = MetalRoughBrdf(point.DiffuseColour, point.F0, point.A2, point.At);
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

} // namespace

int main() {
  const std::vector<SamplePoint> points = SampleSet();
  const std::vector<float> input = Uploaded(points);
  const size_t outputFloats = points.size() * kOutputFloats;

  const Instrument on = BringUp();
  CHECK(on.Device != nullptr, "a device answers, so the WGSL half can be evaluated at all");
  if (!on.Device) { return outshine::Test::Report(); }

  const std::vector<float> asEmitted =
      RunOnDevice(on, TieShader(MetalRoughBrdfWGSL()), input, outputFloats);
  CHECK(asEmitted.size() == outputFloats,
        "the emitted WGSL compiles and returns one result per sample");
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

  const std::string mutated = WithDoubledDistribution(MetalRoughBrdfWGSL());
  CHECK(!mutated.empty(), "the mutation's site is still in the emitted WGSL, so the control applies");
  const std::vector<float> asMutated =
      mutated.empty() ? std::vector<float>() : RunOnDevice(on, TieShader(mutated), input, outputFloats);
  CHECK(asMutated.size() == outputFloats, "the mutated WGSL compiles and returns one result per sample");
  if (asMutated.size() == outputFloats) {
    const Agreement scaled = Compare(points, asMutated);
    ReportAgreement("distribution scaled by two", scaled);
    CHECK(scaled.SpecularAbove > 0, "the sweep carries specular terms above the noise floor at all");
    CHECK(scaled.SpecularDisagreeing == scaled.SpecularAbove,
          "a distribution scaled by two disagrees at every specular channel above the noise floor -- "
          "the tie can see a scaled D");
  }

  CHECK(DeviceErrors == 0, "the device reported no error over either dispatch");
  outshine::Test::Covers("I.26.12 the shading model's two halves: the WGSL the fragment shader is "
                         "spliced from evaluates the same arrangement of terms as the C++ the white "
                         "furnace integrates, over a shared sample set, on the device");
  return outshine::Test::Report();
}
