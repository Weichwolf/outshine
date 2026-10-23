#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_METAL_METALENCODERPROFILE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_METAL_METALENCODERPROFILE_H

#import <Metal/Metal.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Render::Native::Metal {

struct EncoderProfileSession;

enum class EncoderKind { Compute, Render };

struct ProfiledEncoder {
  EncoderKind Kind = EncoderKind::Compute;
  uint32_t Width = 0, Height = 0, Colours = 0;
  bool Depth = false;
  std::string Label;
  std::array<uint64_t, 4> Ticks{};
  bool Available = false;
};

struct EncoderProfile {
  std::shared_ptr<EncoderProfileSession> Session;
  id<MTLCounterSampleBuffer> Samples = nil;
  std::vector<ProfiledEncoder> Encoders;
  uint64_t Dropped = 0;
  uint64_t CalibrationGpu = 0;
  double CalibrationCpuNs = 0;
};

[[nodiscard]] std::shared_ptr<EncoderProfileSession>
CreateEncoderProfileSession(id<MTLDevice> device, uint64_t identity);
[[nodiscard]] std::shared_ptr<EncoderProfile>
CreateEncoderProfile(const std::shared_ptr<EncoderProfileSession> &session);
void ProfileRenderEncoder(EncoderProfile *profile,
                          MTLRenderPassDescriptor *description,
                          std::string_view label);
void ProfileComputeEncoder(EncoderProfile &profile,
                           MTLComputePassDescriptor *description,
                           NSString *label);
void CollectEncoderProfile(const std::shared_ptr<EncoderProfile> &profile,
                           id<MTLCommandBuffer> commands,
                           uint64_t serial);

}
#endif
