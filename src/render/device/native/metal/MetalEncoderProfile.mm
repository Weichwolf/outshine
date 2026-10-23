#include "MetalEncoderProfile.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <SDL3/SDL_stdinc.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <limits>
#include <print>
#include <span>
#include <string_view>
#include <system_error>

namespace outshine::Render::Native::Metal {

namespace {
constexpr size_t kMaxEncoders = 128;
constexpr size_t kSamplesPerEncoder = 4;
constexpr size_t kMaxSubmissions = 256;
constexpr double kNanosecondsPerSecond = 1'000'000'000.0;
static_assert(sizeof(MTLCounterResultTimestamp) == sizeof(uint64_t) &&
              alignof(MTLCounterResultTimestamp) == alignof(uint64_t) &&
              offsetof(MTLCounterResultTimestamp, timestamp) == 0);

struct ClockSample {
  uint64_t Gpu = 0;
  double CpuNs = 0;
};

struct ProfiledSubmission {
  uint64_t Serial = 0;
  uint64_t Dropped = 0;
  double GpuNs = 0;
  double CpuBeginNs = 0, CpuEndNs = 0;
  uint64_t GpuBegin = 0, GpuEnd = 0;
  std::vector<ProfiledEncoder> Encoders;
};

ClockSample SampleGpuClock(id<MTLDevice> device) {
  const auto began = std::chrono::steady_clock::now();
  MTLTimestamp cpu = 0;
  MTLTimestamp gpu = 0;
  [device sampleTimestamps:&cpu gpuTimestamp:&gpu];
  const auto ended = std::chrono::steady_clock::now();
  const auto middle = began + (ended - began) / 2;
  return {.Gpu = gpu,
          .CpuNs = std::chrono::duration<double, std::nano>(middle.time_since_epoch()).count()};
}

void Quoted(std::ostream &stream, std::string_view text) {
  constexpr std::string_view hex = "0123456789abcdef";
  stream << '"';
  for (const char character : text) {
    const auto byte = static_cast<unsigned char>(character);
    if (character == '"' || character == '\\') {
      stream << '\\' << character;
    } else if (byte < 0x20) {
      stream << "\\u00" << hex[byte >> 4] << hex[byte & 0x0f];
    } else {
      stream << character;
    }
  }
  stream << '"';
}

void Number(std::ostream &stream, double value) {
  if (std::isfinite(value)) {
    stream << value;
  } else {
    stream << "null";
  }
}

void WriteEncoder(std::ostream &stream, const ProfiledEncoder &encoder) {
  stream << R"({"kind":")" << (encoder.Kind == EncoderKind::Render ? "render" : "compute")
         << R"(","label":)";
  Quoted(stream, encoder.Label);
  stream << R"(,"width":)" << encoder.Width << R"(,"height":)" << encoder.Height << R"(,"colours":)"
         << encoder.Colours << R"(,"depth":)" << encoder.Depth << R"(,"available":)"
         << encoder.Available << R"(,"ticks":[)";
  for (size_t at = 0; at < encoder.Ticks.size(); ++at) {
    if (at != 0) { stream << ','; }
    stream << encoder.Ticks[at];
  }
  stream << "]}";
}

void WriteSubmission(std::ostream &stream, const ProfiledSubmission &submission) {
  stream << R"({"serial":)" << submission.Serial << R"(,"dropped_encoders":)" << submission.Dropped
         << R"(,"submission_gpu_ns":)";
  Number(stream, submission.GpuNs);
  stream << R"(,"calibration_cpu_ns":[)";
  Number(stream, submission.CpuBeginNs);
  stream << ',';
  Number(stream, submission.CpuEndNs);
  stream << R"(],"calibration_gpu":[)" << submission.GpuBegin << ',' << submission.GpuEnd
         << R"(],"encoders":[)";
  for (size_t at = 0; at < submission.Encoders.size(); ++at) {
    if (at != 0) { stream << ','; }
    WriteEncoder(stream, submission.Encoders[at]);
  }
  stream << "]}";
}
}

struct EncoderProfileSession {
  id<MTLDevice> Device = nil;
  id<MTLCounterSet> CounterSet = nil;
  std::filesystem::path Output;
  std::mutex Mutex;
  std::vector<ProfiledSubmission> Submissions;
  uint64_t Dropped = 0;
  uint64_t AllocationFailures = 0;
  double CollectCpuNs = 0;

  ~EncoderProfileSession() {
    std::ofstream stream(Output, std::ios::trunc);
    if (!stream) {
      std::println(stderr, "Metal encoder profile: cannot write {}", Output.c_str());
      return;
    }
    stream << std::boolalpha << std::setprecision(std::numeric_limits<double>::max_digits10)
           << R"({"schema":1,"scope":"natural_encoder_boundaries",)"
           << R"("counter_ticks_require_calibration":true,"dropped_submissions":)" << Dropped
           << R"(,"allocation_failures":)" << AllocationFailures << R"(,"collect_cpu_ns":)";
    Number(stream, CollectCpuNs);
    stream << R"(,"submissions":[)";
    for (size_t at = 0; at < Submissions.size(); ++at) {
      if (at != 0) { stream << ','; }
      WriteSubmission(stream, Submissions[at]);
    }
    stream << "]}\n";
    stream.close();
    if (!stream) {
      std::println(stderr, "Metal encoder profile: incomplete output {}", Output.c_str());
      return;
    }
    std::println(stderr,
                 "Metal encoder profile: {} submissions={} dropped={}",
                 Output.c_str(),
                 Submissions.size(),
                 Dropped);
  }
};

std::shared_ptr<EncoderProfileSession> CreateEncoderProfileSession(id<MTLDevice> device,
                                                                   uint64_t identity) {
  const char *const prefix =
      SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "OUTSHINE_METAL_ENCODER_PROFILE");
  if (prefix == nullptr || *prefix == '\0') { return {}; }
  std::error_code error;
  const auto temporaryPath = std::filesystem::temp_directory_path(error);
  if (error) { return {}; }
  const auto temporary = std::filesystem::weakly_canonical(temporaryPath, error);
  if (error) { return {}; }
  auto output = std::filesystem::weakly_canonical(
      std::string(prefix) + "." + std::to_string(identity) + ".json", error);
  if (error || !output.is_absolute() || !output.native().starts_with(temporary.native() + "/")) {
    std::println(stderr, "Metal encoder profile: use an absolute system-temp prefix");
    return {};
  }
  id<MTLCounterSet> timestamp = nil;
  for (id<MTLCounterSet> candidate in device.counterSets) {
    if ([candidate.name isEqualToString:MTLCommonCounterSetTimestamp]) { timestamp = candidate; }
  }
  if (timestamp == nil ||
      ![device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary]) {
    std::println(stderr, "Metal encoder profile: stage timestamp sampling unavailable");
    return {};
  }
  auto session = std::make_shared<EncoderProfileSession>();
  session->Device = device;
  session->CounterSet = timestamp;
  session->Output = std::move(output);
  session->Submissions.reserve(kMaxSubmissions);
  return session;
}

std::shared_ptr<EncoderProfile>
CreateEncoderProfile(const std::shared_ptr<EncoderProfileSession> &session) {
  if (!session) { return {}; }
  MTLCounterSampleBufferDescriptor *const description = [MTLCounterSampleBufferDescriptor new];
  description.counterSet = session->CounterSet;
  description.storageMode = MTLStorageModeShared;
  description.sampleCount = kMaxEncoders * kSamplesPerEncoder;
  NSError *error = nil;
  const auto samples = [session->Device newCounterSampleBufferWithDescriptor:description
                                                                       error:&error];
  if (samples == nil) {
    const std::scoped_lock lock(session->Mutex);
    ++session->AllocationFailures;
    return {};
  }
  auto profile = std::make_shared<EncoderProfile>();
  profile->Session = session;
  profile->Samples = samples;
  profile->Encoders.reserve(kMaxEncoders);
  const auto sampled = SampleGpuClock(session->Device);
  profile->CalibrationGpu = sampled.Gpu;
  profile->CalibrationCpuNs = sampled.CpuNs;
  return profile;
}

void ProfileRenderEncoder(EncoderProfile *profile,
                          MTLRenderPassDescriptor *description,
                          std::string_view label) {
  if (profile == nullptr) { return; }
  if (profile->Encoders.size() == kMaxEncoders) {
    ++profile->Dropped;
    return;
  }
  ProfiledEncoder encoder{};
  encoder.Kind = EncoderKind::Render;
  encoder.Label = label;
  constexpr size_t colourSlots = 8;
  for (size_t slot = 0; slot < colourSlots; ++slot) {
    if (description.colorAttachments[slot].texture != nil) { ++encoder.Colours; }
  }
  const auto image = encoder.Colours != 0 ? description.colorAttachments[0].texture
                                          : description.depthAttachment.texture;
  encoder.Width = static_cast<uint32_t>(image.width);
  encoder.Height = static_cast<uint32_t>(image.height);
  encoder.Depth = description.depthAttachment.texture != nil;
  const auto first = profile->Encoders.size() * kSamplesPerEncoder;
  auto *const attachment = description.sampleBufferAttachments[0];
  attachment.sampleBuffer = profile->Samples;
  attachment.startOfVertexSampleIndex = first;
  attachment.endOfVertexSampleIndex = first + 1;
  attachment.startOfFragmentSampleIndex = first + 2;
  attachment.endOfFragmentSampleIndex = first + 3;
  profile->Encoders.push_back(std::move(encoder));
}

void ProfileComputeEncoder(EncoderProfile &profile,
                           MTLComputePassDescriptor *description,
                           NSString *label) {
  if (profile.Encoders.size() == kMaxEncoders) {
    ++profile.Dropped;
    return;
  }
  ProfiledEncoder encoder{};
  if (label != nil && label.UTF8String != nullptr) {
    encoder.Label.assign(label.UTF8String, [label lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
  }
  const auto first = profile.Encoders.size() * kSamplesPerEncoder;
  auto *const attachment = description.sampleBufferAttachments[0];
  attachment.sampleBuffer = profile.Samples;
  attachment.startOfEncoderSampleIndex = first;
  attachment.endOfEncoderSampleIndex = first + 1;
  profile.Encoders.push_back(std::move(encoder));
}

void CollectEncoderProfile(const std::shared_ptr<EncoderProfile> &profile,
                           id<MTLCommandBuffer> commands,
                           uint64_t serial) {
  if (!profile || profile->Encoders.empty()) { return; }
  const auto began = std::chrono::steady_clock::now();
  const auto count = profile->Encoders.size() * kSamplesPerEncoder;
  NSData *const raw = [profile->Samples resolveCounterRange:NSMakeRange(0, count)];
  if (raw.length == count * sizeof(MTLCounterResultTimestamp)) {
    for (size_t at = 0; at < profile->Encoders.size(); ++at) {
      auto &encoder = profile->Encoders[at];
      std::memcpy(encoder.Ticks.data(),
                  static_cast<const char *>(raw.bytes) + at * kSamplesPerEncoder * sizeof(uint64_t),
                  kSamplesPerEncoder * sizeof(uint64_t));
      const size_t used = (encoder.Kind == EncoderKind::Render) ? kSamplesPerEncoder : 2;
      if (!(encoder.Kind == EncoderKind::Render)) {
        encoder.Ticks[2] = 0;
        encoder.Ticks[3] = 0;
      }
      const auto samples = std::span(encoder.Ticks).first(used);
      encoder.Available =
          std::ranges::none_of(
              samples,
              [](uint64_t sample) { return sample == 0 || sample == MTLCounterErrorValue; }) &&
          encoder.Ticks[1] >= encoder.Ticks[0] &&
          (!(encoder.Kind == EncoderKind::Render) || encoder.Ticks[3] >= encoder.Ticks[2]);
    }
  }
  const auto sampled = SampleGpuClock(profile->Session->Device);
  ProfiledSubmission submission{.Serial = serial,
                                .Dropped = profile->Dropped,
                                .GpuNs = (commands.GPUEndTime - commands.GPUStartTime) *
                                         kNanosecondsPerSecond,
                                .CpuBeginNs = profile->CalibrationCpuNs,
                                .CpuEndNs = sampled.CpuNs,
                                .GpuBegin = profile->CalibrationGpu,
                                .GpuEnd = sampled.Gpu,
                                .Encoders = std::move(profile->Encoders)};
  const auto session = profile->Session;
  const std::scoped_lock lock(session->Mutex);
  if (session->Submissions.size() < kMaxSubmissions) {
    session->Submissions.push_back(std::move(submission));
  } else {
    ++session->Dropped;
  }
  session->CollectCpuNs +=
      std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - began).count();
}

}
