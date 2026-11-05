#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <array>
#include <cstdio>
#include <print>

namespace {
constexpr auto kSource = R"(
#include <metal_stdlib>
using namespace metal;
struct V { float4 p [[position]]; };
vertex V vs(uint id [[vertex_id]]) { return {float4(float(id),0,0,1)}; }
fragment float4 bufferOnly(V v [[stage_in]], constant float4 &colour [[buffer(0)]]) { return colour; }
fragment float4 textureRead(V v [[stage_in]], texture2d<float> image [[texture(0)]]) { return image.read(uint2(0)); }
fragment float4 constSampler(V v [[stage_in]], texture2d<float> image [[texture(0)]]) { constexpr sampler sampleImage(filter::linear); return image.sample(sampleImage,float2(0.5)); }
fragment float4 boundSampler(V v [[stage_in]], texture2d<float> image [[texture(0)]], sampler sampleImage [[sampler(0)]]) { return image.sample(sampleImage,float2(0.5)); }
struct Material { texture2d<float> image [[id(0)]]; sampler sampleImage [[id(1)]]; };
fragment float4 argumentBuffer(V v [[stage_in]], constant Material &material [[buffer(0)]]) { return material.image.sample(material.sampleImage,float2(0.5)); }
)";
}

int main() {
  @autoreleasepool {
    const id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) { return 1; }
    NSString *const source = [NSString stringWithUTF8String:kSource];
    if (source == nil) { return 1; }
    NSError *error = nil;
    const id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
    if (library == nil) {
      std::println(stderr, "{}", error.localizedDescription.UTF8String);
      return 1;
    }
    uint32_t mismatches = 0;
    for (NSString *entry in
         @[ @"bufferOnly", @"textureRead", @"constSampler", @"boundSampler", @"argumentBuffer" ]) {
      for (const bool indirect : std::array{false, true}) {
        MTLRenderPipelineDescriptor *const description = [MTLRenderPipelineDescriptor new];
        description.vertexFunction = [library newFunctionWithName:@"vs"];
        description.fragmentFunction = [library newFunctionWithName:entry];
        description.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
        description.supportIndirectCommandBuffers = indirect;
        error = nil;
        const id<MTLRenderPipelineState> pipeline =
            [device newRenderPipelineStateWithDescriptor:description error:&error];
        const bool expected = !indirect || [entry isEqualToString:@"bufferOnly"] ||
                              [entry isEqualToString:@"argumentBuffer"];
        mismatches += (pipeline != nil) != expected ? 1u : 0u;
        std::println("{} indirect={} accepted={} expected={} message={}",
                     entry.UTF8String,
                     indirect,
                     pipeline != nil,
                     expected,
                     error != nil ? error.localizedDescription.UTF8String : "none");
      }
    }
    std::println("device={} mismatches={}", device.name.UTF8String, mismatches);
    return mismatches == 0 ? 0 : 1;
  }
}
