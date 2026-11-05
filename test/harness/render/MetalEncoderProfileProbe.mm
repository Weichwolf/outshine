#include "MetalEncoderProfile.h"
#include "Check.h"
#import <Foundation/Foundation.h>
#include <SDL3/SDL_stdinc.h>
#include <cmath>
#include <filesystem>
#include <limits>
#include <print>
#include <string>
#include <system_error>

namespace {
using namespace outshine::Render::Native::Metal;
using namespace outshine::Test;

std::filesystem::path Output(const std::filesystem::path &prefix, uint64_t identity) {
  return prefix.string() + "." + std::to_string(identity) + ".json";
}

struct Expected {
  size_t Encoders = 0;
  size_t Dropped = 0;
  NSString *Label = nil;
};

void ReadOutput(const std::filesystem::path &path, const Expected &expected) {
  NSString *const file = [NSString stringWithUTF8String:path.c_str()];
  CHECK(file != nil, "the explicit trace path is valid UTF-8");
  if (file == nil) { return; }
  NSData *const data = [NSData dataWithContentsOfFile:file];
  NSError *error = nil;
  NSDictionary *const document =
      data != nil ? [NSJSONSerialization JSONObjectWithData:data options:0 error:&error] : nil;
  CHECK(document != nil && error == nil, "the completed trace is valid JSON");
  if (document == nil) { return; }
  NSArray *const submissions = document[@"submissions"];
  CHECK(submissions.count == 1, "one completed command buffer is retained");
  if (submissions.count != 1) { return; }
  NSDictionary *const submission = submissions.firstObject;
  NSArray *const interval = submission[@"submission_gpu_interval_ns"];
  CHECK(interval.count == 2, "the completed command buffer preserves both GPU endpoints");
  if (interval.count == 2) {
    const double start = [interval[0] doubleValue];
    const double end = [interval[1] doubleValue];
    CHECK(start > 0 && end >= start, "native GPU endpoints are positive and ordered");
    const double duration = [submission[@"submission_gpu_ns"] doubleValue];
    const double rounding = 1.0 + std::numeric_limits<double>::epsilon() * (start + end + duration);
    CHECK(std::abs((end - start) - duration) <= rounding,
          "native GPU endpoints preserve duration within endpoint conversion rounding");
  }
  NSArray *const encoders = submission[@"encoders"];
  CHECK(encoders.count == expected.Encoders,
        "the bounded trace preserves the expected natural encoders");
  CHECK([submission[@"dropped_encoders"] unsignedLongLongValue] == expected.Dropped,
        "additional encoders are explicitly counted, never indexed beyond the buffer");
  CHECK([document[@"dropped_submissions"] unsignedLongLongValue] == 0,
        "the completed submission fits the retained trace");
  if (expected.Label != nil && encoders.count != 0) {
    CHECK([encoders.firstObject[@"label"] isEqualToString:expected.Label],
          "quoted UTF-8 and embedded control characters survive JSON encoding");
  }
}

void Exercise(id<MTLDevice> device, const std::filesystem::path &prefix, EncoderKind kind) {
  CHECK(SDL_SetEnvironmentVariable(
            SDL_GetEnvironment(), "OUTSHINE_METAL_ENCODER_PROFILE", prefix.c_str(), true),
        "the test owns an explicit system-temp trace prefix");
  const bool compute = kind == EncoderKind::Compute;
  constexpr uint64_t identity = 77;
  auto session = CreateEncoderProfileSession(device, identity);
  CHECK(session, "a supported Metal device accepts the diagnostic session");
  auto profile = CreateEncoderProfile(session);
  CHECK(profile, "the bounded timestamp sample buffer is allocated");
  if (!profile) { return; }
  const auto output = Output(prefix, identity);
  std::error_code error;
  std::filesystem::remove(output, error);
  CHECK(!error, "only the previous generated boundary trace is removed");
  session.reset();
  CHECK(!std::filesystem::exists(output), "the session stays alive while its recording owns it");
  const id<MTLCommandQueue> queue = [device newCommandQueue];
  const id<MTLCommandBuffer> commands = [queue commandBuffer];
  NSString *const label = [@"quoted\" \\\n\t snow☃" stringByAppendingFormat:@"%C", unichar{0}];
  constexpr size_t overCapacity = 130;
  if (compute) {
    MTLComputePassDescriptor *const pass = [MTLComputePassDescriptor computePassDescriptor];
    ProfileComputeEncoder(*profile, pass, label);
    const id<MTLComputeCommandEncoder> encoder =
        [commands computeCommandEncoderWithDescriptor:pass];
    [encoder endEncoding];
  } else {
    MTLTextureDescriptor *const image =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:1
                                                          height:1
                                                       mipmapped:NO];
    image.storageMode = MTLStorageModePrivate;
    image.usage = MTLTextureUsageRenderTarget;
    const id<MTLTexture> target = [device newTextureWithDescriptor:image];
    for (size_t at = 0; at < overCapacity; ++at) {
      MTLRenderPassDescriptor *const pass = [MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture = target;
      pass.colorAttachments[0].loadAction = MTLLoadActionClear;
      pass.colorAttachments[0].storeAction = MTLStoreActionStore;
      ProfileRenderEncoder(profile.get(), pass, "retention-boundary");
      const id<MTLRenderCommandEncoder> encoder =
          [commands renderCommandEncoderWithDescriptor:pass];
      [encoder endEncoding];
    }
  }
  [commands commit];
  [commands waitUntilCompleted];
  CHECK(commands.status == MTLCommandBufferStatusCompleted,
        "sampling limits preserve every real Metal encoder without validation failure");
  if (commands.status != MTLCommandBufferStatusCompleted) { return; }
  CollectEncoderProfile(profile, commands, 1);
  CHECK(!std::filesystem::exists(output), "GPU completion does not destroy the recording owner");
  profile.reset();
  constexpr size_t capacity = 128;
  ReadOutput(output,
             {.Encoders = compute ? 1u : capacity,
              .Dropped = compute ? 0u : overCapacity - capacity,
              .Label = compute ? label : nil});
}

void RetainLatest(id<MTLDevice> device, const std::filesystem::path &prefix) {
  CHECK(SDL_SetEnvironmentVariable(
            SDL_GetEnvironment(), "OUTSHINE_METAL_ENCODER_PROFILE", prefix.c_str(), true),
        "the retention probe owns its output prefix");
  constexpr uint64_t identity = 78;
  constexpr uint64_t submitted = 300;
  constexpr uint64_t retained = 256;
  const auto output = Output(prefix, identity);
  std::error_code removed;
  std::filesystem::remove(output, removed);
  CHECK(!removed, "an old retention trace cannot satisfy this run");
  auto session = CreateEncoderProfileSession(device, identity);
  CHECK(session, "the retention probe has a timestamp session");
  if (!session) { return; }
  const id<MTLCommandQueue> queue = [device newCommandQueue];
  for (uint64_t serial = 1; serial <= submitted; ++serial) {
    @autoreleasepool {
      const auto profile = CreateEncoderProfile(session);
      CHECK(profile, "each retained submission has a real timestamp buffer");
      if (!profile) { return; }
      MTLComputePassDescriptor *const pass = [MTLComputePassDescriptor computePassDescriptor];
      ProfileComputeEncoder(*profile, pass, @"retention");
      const id<MTLCommandBuffer> commands = [queue commandBuffer];
      const id<MTLComputeCommandEncoder> encoder =
          [commands computeCommandEncoderWithDescriptor:pass];
      [encoder endEncoding];
      [commands commit];
      [commands waitUntilCompleted];
      CHECK(commands.status == MTLCommandBufferStatusCompleted, "the submission completed");
      if (commands.status != MTLCommandBufferStatusCompleted) { return; }
      CollectEncoderProfile(profile, commands, serial);
    }
  }
  session.reset();
  NSString *const file = [NSString stringWithUTF8String:output.c_str()];
  NSData *const data = [NSData dataWithContentsOfFile:file];
  NSError *error = nil;
  NSDictionary *const document =
      data != nil ? [NSJSONSerialization JSONObjectWithData:data options:0 error:&error] : nil;
  CHECK(document != nil && error == nil, "the wrapped trace is valid JSON");
  NSArray *const submissions = document[@"submissions"];
  CHECK(submissions.count == retained, "retention stays bounded after wrapping");
  CHECK([document[@"dropped_submissions"] unsignedLongLongValue] == submitted - retained,
        "evicted preload submissions remain explicit");
  for (size_t at = 0; at < submissions.count; ++at) {
    CHECK([submissions[at][@"serial"] unsignedLongLongValue] == submitted - retained + at + 1,
          "the latest completed submissions retain their order across wraparound");
  }
}
}

int main(int argc, char *const *argv) {
  using namespace outshine::Test;
  if (argc != 2) { return 2; }
  @autoreleasepool {
    const id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    CHECK(device != nil, "the explicit Metal probe requires its actual device");
    if (device == nil) { return Report(); }
    const auto invalid =
        std::filesystem::temp_directory_path().root_path() / "encoder-profile-rejected";
    CHECK(SDL_SetEnvironmentVariable(
              SDL_GetEnvironment(), "OUTSHINE_METAL_ENCODER_PROFILE", invalid.c_str(), true),
          "the boundary test supplies an unsafe output prefix");
    CHECK(!CreateEncoderProfileSession(device, 1), "non-temp diagnostic output is rejected");
    Exercise(device, std::filesystem::path(argv[1]) / "quota", EncoderKind::Render);
    Exercise(device, std::filesystem::path(argv[1]) / "labels", EncoderKind::Compute);
    RetainLatest(device, std::filesystem::path(argv[1]) / "retention");
  }
  return Report();
}
