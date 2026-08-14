/* THE TANGENT BASIS ON BOTH SIDES OF THE SURFACE (board:1127). `NormalFromMap.h` states glTF's
 * normal-mapping basis twice -- once in C++, once in the MSL the mapped fragment arm is spliced from
 * -- and until this file existed nothing evaluated the device half at a back-facing fragment. The
 * corpus could not: measured over the two tangent assets, 0 shaded fragments are back-facing, so a
 * left-handed back-face basis rendered every case identically and no picture could fall.
 *
 * WHAT THIS MEASURES IS THE FRAME AND NOT THE MAP. The same supplied normal, tangent, handedness,
 * sampled triple and scale go through both halves at both facings, and the ANGLE between the two
 * answers is the metric -- both halves return a unit direction, so an angle is the whole of the
 * disagreement and it needs no scale to be read against.
 *
 * THE TWO HALVES ARE SPELLED DIFFERENTLY, which is what makes the comparison worth taking. The C++
 * half negates the three axes one at a time, as the format's own renderer does
 * (`material_info.glsl:160`); the MSL half carries one sign out to the composed normal. A
 * transliteration would agree with itself and measure nothing.
 *
 * THE THIRD CLAIM IS THE ITEM'S OWN SENTENCE AND IT IS TAKEN ON THE DEVICE ALONE: a back-facing
 * fragment's normal is the front-facing one negated, bit for bit, over the same inputs. That is what
 * "the frame turns whole" means with no reference in the path at all -- a basis that negated two
 * axes of three cannot satisfy it, whatever the reference says.
 *
 * THE COMPARISON CARRIES ITS OWN NEGATIVE CONTROL, and the control is the defect this file is named
 * for rather than an invented one: the emitted MSL is mutated so the bitangent is negated a second
 * time on a back face, which reproduces exactly the `(-n, -t, +b)` basis the repaired text replaced.
 * The mutant must go red at every back-facing sample whose bitangent contributes, and it must move
 * no front-facing sample at all -- an instrument that also moved the front faces would be measuring
 * something wider than the back-face branch.
 *
 * THE ALLOWANCE IS DERIVED PER SAMPLE AND NOT CHOSEN. The device computes in f32 and the reference
 * in f64. The conditioning is the Gram-Schmidt: a supplied tangent close to the normal loses the
 * perpendicular part to cancellation, so the angular error of the basis grows as the reciprocal of
 * the fraction of the tangent that survives it. Every sample publishes that fraction and its own
 * allowance follows from it; the sweep's worst is printed, and the defect this file exists for is
 * seven orders above it. */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "Check.h"

#include "NormalFromMap.h"
#include "Readback.h"
#include "ShaderPrelude.h"

using outshine::Render::Direction;
using outshine::Render::Dot;
using outshine::Render::Facing;
using outshine::Render::kMslPrelude;
using outshine::Render::NormalFromMap;
using outshine::Render::NormalFromMapMsl;
using outshine::Render::Normalised;
using outshine::Render::Readback;
using outshine::Render::ReadState;
using outshine::Render::SuppliedFrame;

namespace {

int DeviceErrors = 0;

bool Refused(const void *made, const char *what) {
  if (made) { return false; }
  ++DeviceErrors;
  std::printf("NOTE device refused %s: %s\n", what, SDL_GetError());
  return true;
}

/* ONE SHADING POINT AS BOTH HALVES TAKE IT. Every value is already rounded to f32, because the
 * device gets f32 and a reference evaluated on wider inputs would be measuring the upload. */
struct SamplePoint {
  SuppliedFrame Supplied{};
  Direction Tap{};
  double Scale = 1.0;
  Facing At = Facing::Front;
};

constexpr uint32_t kInputFloats = 12;   /* normal 3, tangent 4, tap 3, scale 1, facing 1 */
constexpr uint32_t kOutputFloats = 3;

constexpr double kFloatEpsilon = 5.9604644775390625e-08;   /* 2^-24, half an ulp at 1.0 */
constexpr double kDegreesPerRadian = 57.295779513082320876798;
/* [SET] The rounded operations the longest path through the basis performs, generously counted: the
 * normalisation of the supplied normal, the dot product and the subtraction of the projection, the
 * second normalisation, the cross product, the three scaled axes and their sum, and the final
 * normalisation. */
constexpr double kRoundingSteps = 24.0;

double AsFloat(double value) { return static_cast<double>(static_cast<float>(value)); }

Direction AsFloat(const Direction &of) { return {AsFloat(of.X), AsFloat(of.Y), AsFloat(of.Z)}; }

/* HOW MUCH OF THE SUPPLIED TANGENT SURVIVES GRAM-SCHMIDT, between 0 and 1. It is the sine of the
 * angle between the supplied tangent and the supplied normal, and it is the sample's conditioning:
 * everything the basis derives from the perpendicular part inherits the reciprocal of it. */
double PerpendicularFraction(const SuppliedFrame &supplied) {
  const Direction normal = Normalised(supplied.Normal);
  const double along = Dot(normal, supplied.Tangent);
  const Direction perpendicular = {supplied.Tangent.X - normal.X * along,
                                   supplied.Tangent.Y - normal.Y * along,
                                   supplied.Tangent.Z - normal.Z * along};
  return std::sqrt(Dot(perpendicular, perpendicular) / Dot(supplied.Tangent, supplied.Tangent));
}

/* The angular error in radians this sample's own conditioning admits of the device's f32. */
double AllowedRadians(const SuppliedFrame &supplied) {
  return kRoundingSteps * kFloatEpsilon * (1.0 + 1.0 / PerpendicularFraction(supplied));
}

double AngleBetween(const Direction &left, const Direction &right) {
  const double cosine = std::fmin(1.0, std::fmax(-1.0, Dot(Normalised(left), Normalised(right))));
  return std::acos(cosine);
}

/* THE SWEEP. The supplied normals are neither unit nor axis-aligned and the tangents are neither
 * unit nor perpendicular, because that is how they arrive at a fragment -- a sweep of exact
 * orthonormal bases would never exercise the re-orthogonalisation the basis performs. BOTH
 * HANDEDNESSES ARE PRESENT: `w` is what a mirrored body carries, and a basis that ignored it would
 * agree with itself at `w = 1` forever. THE TAPS SPAN THE SIGN OF EVERY CHANNEL AND INCLUDE ONE
 * WITH NO BITANGENT COMPONENT AT ALL, which is the sample the back-face defect cannot move -- it
 * belongs in the set so that the count of moved samples is a count and not a total. */
const Direction kSuppliedNormals[] = {
    {0.0, 0.0, 1.0}, {0.2, -0.3, 0.93}, {-0.61, 0.55, 0.57}, {0.0, 0.98, 0.19}, {0.33, 0.33, -0.88},
};
const Direction kSuppliedTangents[] = {
    {1.0, 0.0, 0.0}, {0.9, 0.1, -0.15}, {0.42, 0.86, 0.29}, {-0.77, 0.21, 0.6},
};
constexpr double kHandedness[] = {1.0, -1.0};
const Direction kTaps[] = {
    {0.0, 0.0, 1.0},        /* flat, and the one sample no basis error can move */
    {0.31, -0.62, 0.72},    /* the map's green channel dominant and negative */
    {-0.44, 0.55, 0.71},
    {0.06, 0.94, 0.34},     /* almost entirely bitangent */
    {-0.83, -0.12, 0.55},
};
/* glTF's own default is 1; 0 is the flattening end of the parameter and 2 is a file exaggerating. */
constexpr double kScales[] = {0.0, 0.5, 1.0, 2.0};

/* THE TWO FACINGS SIT ADJACENT IN THE SET, so the exactness claim below compares two slots and not
 * two runs: the device evaluates the pair from bit-identical inputs in one dispatch. */
std::vector<SamplePoint> SampleSet(void) {
  std::vector<SamplePoint> points;
  for (const Direction &normal : kSuppliedNormals) {
    for (const Direction &tangent : kSuppliedTangents) {
      for (const double handedness : kHandedness) {
        for (const Direction &tap : kTaps) {
          for (const double scale : kScales) {
            SamplePoint point;
            point.Supplied.Normal = AsFloat(normal);
            point.Supplied.Tangent = AsFloat(tangent);
            point.Supplied.Handedness = handedness;
            point.Tap = AsFloat(tap);
            point.Scale = AsFloat(scale);
            point.At = Facing::Front;
            points.push_back(point);
            point.At = Facing::Back;
            points.push_back(point);
          }
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
    floats.push_back((float)point.Supplied.Normal.X);
    floats.push_back((float)point.Supplied.Normal.Y);
    floats.push_back((float)point.Supplied.Normal.Z);
    floats.push_back((float)point.Supplied.Tangent.X);
    floats.push_back((float)point.Supplied.Tangent.Y);
    floats.push_back((float)point.Supplied.Tangent.Z);
    floats.push_back((float)point.Supplied.Handedness);
    floats.push_back((float)point.Tap.X);
    floats.push_back((float)point.Tap.Y);
    floats.push_back((float)point.Tap.Z);
    floats.push_back((float)point.Scale);
    floats.push_back(point.At == Facing::Front ? 1.0f : 0.0f);
  }
  return floats;
}

/* The entry point the basis's own text is evaluated through. It declares no arithmetic of its own:
 * it unpacks a sample, calls `normalFromMap` and writes what came back. The sample count is a
 * uniform and not a length query, because MSL has none. */
std::string TieShader(const std::string &basis) {
  char stride[128];
  std::snprintf(stride, sizeof stride, "constant uint kIn = %uu;\nconstant uint kOut = %uu;\n",
                kInputFloats, kOutputFloats);
  return std::string(kMslPrelude) + basis + std::string(stride) + R"(
struct Span { uint floats; };

kernel void tie(uint3 id [[thread_position_in_grid]],
                constant Span &span [[buffer(0)]],
                const device float *samples [[buffer(1)]],
                device float *results [[buffer(2)]]) {
  uint base = id.x * kIn;
  if (base + kIn > span.floats) { return; }
  float3 vertexNormal = float3(samples[base], samples[base + 1u], samples[base + 2u]);
  float4 tangent = float4(samples[base + 3u], samples[base + 4u], samples[base + 5u],
                          samples[base + 6u]);
  float3 tap = float3(samples[base + 7u], samples[base + 8u], samples[base + 9u]);
  float3 mapped = normalFromMap(vertexNormal, tangent, tap, samples[base + 10u],
                                samples[base + 11u] > 0.5);
  uint slot = id.x * kOut;
  results[slot] = mapped.x;
  results[slot + 1u] = mapped.y;
  results[slot + 2u] = mapped.z;
}
)";
}

/* THE NEGATIVE CONTROL'S SITE, named rather than guessed: the bitangent. Negating it a second time
 * on a back face is the `(-n, -t, +b)` basis board:1127 is about, restored exactly. An empty return
 * means the site is gone, and then the mutation was never applied -- which this refuses over rather
 * than passing. */
std::string WithTheBitangentLeftUnturned(const std::string &basis) {
  const std::string site = "float3 b = cross(n, t) * tangent.w;";
  const size_t found = basis.find(site);
  if (found == std::string::npos) { return std::string(); }
  return std::string(basis).replace(found, site.size(),
                                    "float3 b = cross(n, t) * tangent.w * select(-1.0, 1.0, front);");
}

/* THE DEVICE, AND NOTHING ELSE: no swapchain, no window, no asset. */
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

Direction At(const std::vector<float> &measured, size_t sample) {
  const size_t slot = sample * kOutputFloats;
  return {measured[slot], measured[slot + 1], measured[slot + 2]};
}

/* THE COUNTS ARE SPLIT BY FACING, because the branch this file is about is one of the two: a
 * disagreement over both would be a defect in the basis itself and a different finding. */
struct Agreement {
  double WorstRadians = 0;
  double WorstAllowed = 0;
  int Compared = 0;
  int Disagreeing = 0;
  int BackFacing = 0;
  int BackFacingDisagreeing = 0;
  int FrontFacingDisagreeing = 0;
};

Agreement Compare(const std::vector<SamplePoint> &points, const std::vector<float> &measured) {
  Agreement found;
  for (size_t at = 0; at < points.size(); ++at) {
    const SamplePoint &point = points[at];
    const Direction reference = NormalFromMap(point.Supplied, point.Tap, point.Scale, point.At);
    const double off = AngleBetween(reference, At(measured, at));
    const double allowed = AllowedRadians(point.Supplied);
    const bool disagrees = off > allowed;
    ++found.Compared;
    found.WorstRadians = std::fmax(found.WorstRadians, off);
    found.WorstAllowed = std::fmax(found.WorstAllowed, allowed);
    if (disagrees) { ++found.Disagreeing; }
    if (point.At == Facing::Back) {
      ++found.BackFacing;
      if (disagrees) { ++found.BackFacingDisagreeing; }
    } else if (disagrees) {
      ++found.FrontFacingDisagreeing;
    }
  }
  return found;
}

void ReportAgreement(const char *what, const Agreement &found) {
  std::printf("TIE %s: %d samples compared, %d disagreeing (%d of %d back-facing, %d front-facing), "
              "worst angle %.9g deg, worst allowance %.9g deg\n",
              what, found.Compared, found.Disagreeing, found.BackFacingDisagreeing, found.BackFacing,
              found.FrontFacingDisagreeing, found.WorstRadians * kDegreesPerRadian,
              found.WorstAllowed * kDegreesPerRadian);
}

/* HOW MANY BACK-FACING SAMPLES A BASIS ERROR CAN MOVE AT ALL. A tap with no bitangent component, or
 * a scale of zero, composes the same direction under either basis -- so this is the population the
 * negative control is judged over, and quoting the whole back-facing count instead would report a
 * defect the sweep never asked those samples about. */
int MovableByTheBitangent(const std::vector<SamplePoint> &points) {
  int movable = 0;
  for (const SamplePoint &point : points) {
    if (point.At == Facing::Back && point.Tap.Y != 0.0 && point.Scale != 0.0) { ++movable; }
  }
  return movable;
}

} // namespace

int main() {
  const std::vector<SamplePoint> points = SampleSet();
  const std::vector<float> input = Uploaded(points);
  const size_t outputFloats = points.size() * kOutputFloats;

  const Instrument on;
  CHECK(on.Device != nullptr, "a device answers, so the MSL half can be evaluated at all");
  if (!on.Device) { return outshine::Test::Report(); }

  const std::vector<float> asEmitted =
      RunOnDevice(on, TieShader(NormalFromMapMsl()), input, outputFloats);
  CHECK(asEmitted.size() == outputFloats,
        "the emitted MSL compiles and returns one direction per sample");
  if (asEmitted.size() != outputFloats) { return outshine::Test::Report(); }

  const Agreement emitted = Compare(points, asEmitted);
  ReportAgreement("as emitted", emitted);
  outshine::Test::Note("samples in the sweep", (double)emitted.Compared, "count");
  outshine::Test::Note("back-facing samples in the sweep", (double)emitted.BackFacing, "count");
  outshine::Test::Note("worst angle between the halves", emitted.WorstRadians * kDegreesPerRadian,
                       "degrees");
  CHECK(emitted.BackFacing > 0, "the sweep evaluates the back-facing branch at all");
  CHECK(emitted.Disagreeing == 0,
        "the device's basis and the format's own statement of it name the same direction at every "
        "sample of both facings, inside the f32 error the sample's own conditioning admits");

  /* THE ITEM'S SENTENCE, ON THE DEVICE ALONE. The two facings of one configuration are adjacent, so
   * this compares two slots of one dispatch and no reference enters it. */
  int notNegated = 0;
  double worstAsymmetry = 0;
  for (size_t at = 0; at + 1 < points.size(); at += 2) {
    const Direction front = At(asEmitted, at);
    const Direction back = At(asEmitted, at + 1);
    const double off = std::fmax(std::fabs(front.X + back.X),
                                 std::fmax(std::fabs(front.Y + back.Y), std::fabs(front.Z + back.Z)));
    worstAsymmetry = std::fmax(worstAsymmetry, off);
    if (off != 0.0) { ++notNegated; }
  }
  outshine::Test::Note("worst |front + back| over the pairs", worstAsymmetry, "dimensionless");
  CHECK(notNegated == 0,
        "a back-facing fragment's mapped normal is the front-facing one negated, bit for bit, over "
        "the same inputs -- the whole frame turns and not two axes of three");

  const std::string mutated = WithTheBitangentLeftUnturned(NormalFromMapMsl());
  CHECK(!mutated.empty(), "the mutation's site is still in the emitted MSL, so the control applies");
  const std::vector<float> asMutated =
      mutated.empty() ? std::vector<float>() : RunOnDevice(on, TieShader(mutated), input, outputFloats);
  CHECK(asMutated.size() == outputFloats, "the mutated MSL compiles and returns one direction per sample");
  if (asMutated.size() == outputFloats) {
    const Agreement unturned = Compare(points, asMutated);
    ReportAgreement("bitangent left unturned on a back face", unturned);
    const int movable = MovableByTheBitangent(points);
    outshine::Test::Note("back-facing samples a bitangent error can move", (double)movable, "count");
    CHECK(movable > 0, "the sweep carries back-facing samples whose bitangent contributes at all");
    CHECK(unturned.BackFacingDisagreeing == movable,
          "a bitangent left unturned disagrees at every back-facing sample whose bitangent "
          "contributes -- the tie can see the basis board:1127 replaced");
    CHECK(unturned.FrontFacingDisagreeing == 0,
          "and it moves no front-facing sample, so what the tie sees is the back-face branch and "
          "nothing wider");
  }

  CHECK(DeviceErrors == 0, "the device reported no error over either dispatch");
  outshine::Test::Covers("board:1127 the tangent basis at a back-facing fragment: the mapped normal "
                         "is the front-facing one negated whole, and the device's basis agrees with "
                         "the format's own statement of it at both facings");
  return outshine::Test::Report();
}
