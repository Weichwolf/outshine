#include "MetalProductRenderer.h"
#include "MetalGroupImpostor.h"
#include "MetalGroupImpostorShader.h"
#include "Units.h"

#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <future>
#include <optional>
#include <thread>
#include <utility>

namespace outshine::Render {
namespace {
using Clock = std::chrono::steady_clock;
constexpr size_t kMatrixWords = 16;
constexpr size_t kSurfaceShadingBytes = 80;
constexpr auto kClearColor = std::array{0.18, 0.25, 0.34};
constexpr size_t kChannels = 4;
using ClipMatrix = std::array<float, kMatrixWords>;
using SurfaceLightUniform = std::array<float, 16>;
using SurfaceColourRanges = std::array<uint32_t, 4>;

struct SurfaceShadingUniform {
  SurfaceLightUniform Light;
  std::array<float, 4> EyeAndImpostorWeight;
};

static_assert(sizeof(SurfaceVertex) == 6 * sizeof(float) &&
              alignof(SurfaceVertex) == alignof(float) &&
              offsetof(SurfaceVertex, Normal) == 3 * sizeof(float));
static_assert(sizeof(ClipMatrix) == 64 && sizeof(uint32_t) == 4);
static_assert(sizeof(SurfaceColour) == 4);
static_assert(sizeof(SurfaceShadingUniform) == kSurfaceShadingBytes &&
              offsetof(SurfaceShadingUniform, EyeAndImpostorWeight) == 64 &&
              sizeof(SurfaceColourRanges) == 16);
NSString *const kShader = @R"(
#include <metal_stdlib>
using namespace metal;
struct SurfaceVertex {
  packed_float3 position;
  packed_float3 normal;
};
struct Varyings {
  float4 position [[position]];
  float3 localPosition;
  float3 normal;
  half4 colour;
  uint material [[flat]];
};
struct SurfaceLight {
  float4 sun;
  float4 direct;
  float4 sky;
  float4 ground;
  float4 eyeAndImpostorWeight;
};
enum SurfaceMaterial : uint { Fallback, Preview, TerrainMaterial, Overlay };
vertex Varyings tile_vertex(uint index [[vertex_id]],
                            const device SurfaceVertex *vertices [[buffer(0)]],
                            constant float4x4 &clip [[buffer(1)]],
                            const device uchar4 *colours [[buffer(2)]],
                            constant uint4 &colourRanges [[buffer(3)]]) {
  SurfaceVertex point = vertices[index];
  const half4 colour = index < colourRanges.x ? half4(0.0h) :
      half4(colours[index - colourRanges.x]) / 255.0h;
  const uint material = index < colourRanges.x ? Fallback :
      (index >= colourRanges.y ? Overlay : (index >= colourRanges.z ? TerrainMaterial : Preview));
  return {clip * float4(point.position, 1.0), point.position, point.normal, colour, material};
}
float3 filmic(float3 x) {
  x = min(x, 16.0);
  return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}
float3 groundRadiance(Varyings in, float3 normal, constant SurfaceLight &light, float3 ambient) {
  const float3 albedo = float3(in.colour.rgb);
  constexpr float f0 = 0.04;
  const float nl = saturate(dot(normal, light.sun.xyz));
  if (nl <= 0.0) { return (1.0 - f0) * albedo * ambient; }
  const float3 toEye = light.eyeAndImpostorWeight.xyz - in.localPosition;
  const float3 view = toEye * rsqrt(max(dot(toEye, toEye), 1e-12));
  const float3 axis = light.sun.xyz + view;
  const float3 halfway = axis * rsqrt(max(dot(axis, axis), 1e-12));
  const float nv = max(1e-6, saturate(dot(normal, view)));
  const float nh = saturate(dot(normal, halfway));
  const float grazing = 1.0 - saturate(dot(view, halfway));
  const float squareGrazing = grazing * grazing;
  const float fresnel = f0 + (1.0 - f0) * squareGrazing * squareGrazing * grazing;
  const float roughness = max(0.04, float(in.colour.a));
  const float alpha = roughness * roughness;
  const float a2 = alpha * alpha;
  const float denominator = (1.0 - nh) * (1.0 + nh) + nh * nh * a2;
  const float distribution = a2 / (denominator * denominator);
  const float visibility = 0.5 / (nl * sqrt(nv * nv * (1.0 - a2) + a2) +
                                  nv * sqrt(nl * nl * (1.0 - a2) + a2));
  const float3 diffuse = (1.0 - fresnel) * albedo;
  return (diffuse + fresnel * distribution * visibility) * (light.direct.xyz * nl) +
      diffuse * ambient;
}
float4 tileColour(Varyings in, constant SurfaceLight &light) {
  float3 normal = normalize(in.normal);
  if (in.material >= TerrainMaterial && light.sun.w > 0.0) {
    const float3 ambient = mix(light.ground.xyz, light.sky.xyz, 0.5 + 0.5 * normal.y);
    if (in.material == TerrainMaterial) {
      return float4(filmic(groundRadiance(in, normal, light, ambient)), 1.0);
    }
    const float3 direct = light.direct.xyz * max(0.0, dot(normal, light.sun.xyz));
    return float4(filmic(float3(in.colour.rgb) * (ambient + direct)), 1.0);
  }
  const float3 previewLight = normalize(float3(-0.55, 0.65, 0.45));
  float diffuse = 0.35 + 0.65 * max(0.0, dot(normal, previewLight));
  const float3 reflectance = in.material != Fallback ? float3(in.colour.rgb) :
      mix(float3(0.26, 0.22, 0.17), float3(0.13, 0.20, 0.10), smoothstep(0.45, 0.92, normal.y));
  return float4(diffuse * reflectance, 1.0);
}
float lodNoise(float2 pixel) {
  return fract(52.9829189 * fract(dot(floor(pixel), float2(0.06711056, 0.00583715))));
}
void keepTileFragment(Varyings in, constant SurfaceLight &light) {
  const float weight = light.eyeAndImpostorWeight.w;
  if (in.material == Overlay && weight > 0.0 && lodNoise(in.position.xy) < weight) {
    discard_fragment();
  }
}
fragment float4 tile_fragment(Varyings in [[stage_in]],
                              constant SurfaceLight &light [[buffer(0)]]) {
  keepTileFragment(in, light);
  return tileColour(in, light);
}
struct TileFragment {
  float4 colour [[color(0), raster_order_group(0)]];
  float depth [[color(1), raster_order_group(0)]];
};
fragment TileFragment tile_composite_fragment(Varyings in [[stage_in]],
                                               constant SurfaceLight &light [[buffer(0)]]) {
  keepTileFragment(in, light);
  return {tileColour(in, light), in.position.z};
}
struct PanoramaUniform {
  float4 forward;
  float4 rightSpan;
  float4 upSpan;
  float4 display;
  float4 eyeOffsetM;
  float4 nextEyeAndWeight;
  float4 coverage;
};
struct PanoramaRaster {
  float4 position [[position]];
  float2 ndc;
};
vertex PanoramaRaster panorama_vertex(uint index [[vertex_id]]) {
  constexpr float2 triangle[] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
  float2 ndc = triangle[index];
  return {float4(ndc, 0, 1), ndc};
}
struct PanoramaFragment {
  float4 colour [[color(0)]];
  float depth [[depth(any)]];
};
struct PanoramaRay {
  float3 direction;
  float distanceM;
  bool opaqueAtAnchor;
};
PanoramaRay reprojectPanorama(float3 ray, texturecube<float> depth,
                             float3 eye, float focalPixels) {
  constexpr sampler pointDepth(coord::normalized, filter::nearest);
  float radius = depth.sample(pointDepth, ray).r;
  const bool opaqueAtAnchor = radius > 0.0;
  const float eye2 = dot(eye, eye);
  if (radius <= 0.0 || eye2 == 0.0) {
    return {ray, max(radius, 0.0), opaqueAtAnchor};
  }
  const float toward = dot(ray, eye);
  const float across2 = max(eye2 - toward * toward, 0.0);
  const float pixelsTimesBaseline = sqrt(across2) * focalPixels;
  float3 lookup = ray;
  bool stable = false;
  for (uint iteration = 0; iteration < 3; ++iteration) {
    const float discriminant = radius * radius - across2;
    if (discriminant <= 0.0) {
      return {ray, 0.0, opaqueAtAnchor};
    }
    const float distance = sqrt(discriminant) - toward;
    if (distance <= 0.0) {
      return {ray, 0.0, opaqueAtAnchor};
    }
    lookup = normalize(eye + distance * ray);
    const float next = depth.sample(pointDepth, lookup).r;
    if (next <= 0.0) {
      return {ray, 0.0, opaqueAtAnchor};
    }
    const float inverseChange = abs(next - radius) / (next * radius);
    stable = inverseChange * pixelsTimesBaseline < 0.5;
    radius = next;
  }
  const float discriminant = radius * radius - across2;
  return stable && discriminant > 0.0
      ? PanoramaRay{lookup, max(sqrt(discriminant) - toward, 0.0), opaqueAtAnchor}
      : PanoramaRay{ray, 0.0, opaqueAtAnchor};
}
struct PanoramaPixel {
  float3 colour;
  float3 ray;
  float distanceM;
  float forwardM;
  bool opaqueAtAnchor;
};
float anchorColourWeight(PanoramaRay first, PanoramaRay next, float travel,
                         float3 ray, constant PanoramaUniform &view) {
  const float firstM = first.distanceM;
  const float nextM = next.distanceM;
  if (firstM <= 0.0 || nextM <= 0.0) {
    if (firstM != nextM) { return float(nextM > firstM); }
    if (first.opaqueAtAnchor != next.opaqueAtAnchor) { return float(!next.opaqueAtAnchor); }
    return first.opaqueAtAnchor ? step(0.5, travel) : travel;
  }
  const float3 baseline = view.nextEyeAndWeight.xyz - view.eyeOffsetM.xyz;
  const float baselinePixelsM = length(cross(baseline, ray)) * view.display.w;
  const float disparityPx = baselinePixelsM * abs(firstM - nextM) / (firstM * nextM);
  const float separation = smoothstep(0.5, 1.0, disparityPx);
  return mix(travel, float(nextM < firstM), separation);
}
PanoramaPixel panoramaPixel(PanoramaRaster in, constant PanoramaUniform &view,
                            texturecube<half> colour, texturecube<float> distanceM,
                            texturecube<half> nextColour, texturecube<float> nextDistanceM) {
  constexpr sampler linearColour(coord::normalized, filter::linear);
  float3 ray = normalize(view.forward.xyz + in.ndc.x * view.rightSpan.xyz +
                         in.ndc.y * view.upSpan.xyz);
  const PanoramaRay projected =
      reprojectPanorama(ray, distanceM, view.eyeOffsetM.xyz, view.display.w);
  float distance = projected.distanceM;
  bool opaqueAtAnchor = projected.opaqueAtAnchor;
  float3 linear = float3(colour.sample(linearColour, projected.direction).rgb);
  const float weight = view.nextEyeAndWeight.w;
  if (weight > 0.0) {
    const PanoramaRay next =
        reprojectPanorama(ray, nextDistanceM, view.nextEyeAndWeight.xyz, view.display.w);
    const float nextWeight = anchorColourWeight(projected, next, weight, ray, view);
    opaqueAtAnchor = (nextWeight < 1.0 && projected.opaqueAtAnchor) ||
                     (nextWeight > 0.0 && next.opaqueAtAnchor);
    linear = mix(linear, float3(nextColour.sample(linearColour, next.direction).rgb), nextWeight);
    if (next.distanceM > 0.0) {
      distance = distance > 0.0 ? min(distance, next.distanceM) : next.distanceM;
    }
  }
  float forwardM = distance * dot(view.forward.xyz, ray);
  return {filmic(linear * view.display.z), ray, distance, forwardM, opaqueAtAnchor};
}
float deviceDepth(float forwardM, constant PanoramaUniform &view) {
  return forwardM > 0.0 ? saturate(view.display.x / forwardM - view.display.y) : 0.0;
}
fragment PanoramaFragment panorama_fragment(PanoramaRaster in [[stage_in]],
                                             constant PanoramaUniform &view [[buffer(0)]],
                                             texturecube<half> colour [[texture(0)]],
                                             texturecube<float> distanceM [[texture(1)]],
                                             texturecube<half> nextColour [[texture(2)]],
                                             texturecube<float> nextDistanceM [[texture(3)]]) {
  const auto pixel = panoramaPixel(in, view, colour, distanceM, nextColour, nextDistanceM);
  return {float4(pixel.colour, 1.0), deviceDepth(pixel.forwardM, view)};
}
float majorAxis(float3 point) {
  const float3 a = abs(point);
  return max(a.x, max(a.y, a.z));
}
float overlapWeight(float nearAxial, float farAxial, float3 coverage) {
  const float nearExit = smoothstep(coverage.x, coverage.z, nearAxial);
  if (nearExit >= 1.0) { return 1.0; }
  const float farEntry = smoothstep(coverage.x, coverage.z, farAxial);
  const float nearOpacity = 1.0 - nearExit;
  const float shared = farEntry / (nearOpacity + farEntry);
  const float separation = nearAxial - farAxial;
  const float orderedFar = farEntry * (separation > 0.0 ? 1.0 : nearExit);
  const float orderedNear = nearOpacity * (separation > 0.0 ? 1.0 - farEntry : 1.0);
  const float ordered = orderedFar / (orderedNear + orderedFar);
  const float separated = smoothstep(coverage.y, 2.0 * coverage.y, abs(separation));
  return mix(shared, ordered, separated);
}
float coverageWeight(const PanoramaPixel &far, constant PanoramaUniform &view, float nearDepth) {
  if (nearDepth <= 0.0) { return 1.0; }
  const float clip = view.coverage.x;
  const float end = view.coverage.z;
  const bool unresolved = far.forwardM <= 0.0;
  if (unresolved && !(end > clip && far.opaqueAtAnchor)) { return 0.0; }
  const float width = view.coverage.y;
  if (width <= 0.0) { return nearDepth > deviceDepth(far.forwardM, view) ? 0.0 : 1.0; }
  const float nearForward = view.display.x / (nearDepth + view.display.y);
  const float nearDistance = nearForward / dot(view.forward.xyz, far.ray);
  const float3 eye = mix(view.eyeOffsetM.xyz, view.nextEyeAndWeight.xyz, view.nextEyeAndWeight.w);
  const float nearAxial = majorAxis(eye + nearDistance * far.ray);
  if (unresolved) { return smoothstep(clip, end, nearAxial); }
  const float farAxial = majorAxis(eye + far.distanceM * far.ray);
  if (end > clip) { return overlapWeight(nearAxial, farAxial, view.coverage.xyz); }
  return max(smoothstep(clip, clip + width, nearAxial),
             smoothstep(width, 2.0 * width, nearAxial - farAxial));
}
fragment float4 panorama_composite_fragment(
    PanoramaRaster in [[stage_in]],
    constant PanoramaUniform &view [[buffer(0)]],
    texturecube<half> colour [[texture(0)]],
    texturecube<float> distanceM [[texture(1)]],
    texturecube<half> nextColour [[texture(2)]],
    texturecube<float> nextDistanceM [[texture(3)]],
    float4 nearColour [[color(0), raster_order_group(0)]],
    float nearDepth [[color(1), raster_order_group(0)]]) {
  const auto pixel = panoramaPixel(in, view, colour, distanceM, nextColour, nextDistanceM);
  const float weight = coverageWeight(pixel, view, nearDepth);
  if (weight <= 0.0) { discard_fragment(); }
  return float4(mix(nearColour.rgb, pixel.colour, weight), 1.0);
}
)";

double ElapsedMs(Clock::time_point since) {
  return std::chrono::duration<double, std::milli>(Clock::now() - since).count();
}

std::expected<ClipMatrix, std::string> MetalClip(Mat4 clip) {
  ClipMatrix packed{};
  for (size_t column = 0; column < kMatrixWords; column += 4) {
    clip[column + 2] = (clip[column + 3] - clip[column + 2]) * 0.5;
  }
  for (size_t word = 0; word < packed.size(); ++word) {
    packed[word] = static_cast<float>(clip[word]);
    if (!std::isfinite(packed[word])) { return std::unexpected("nonfinite Metal clip matrix"); }
  }
  return packed;
}

MTLTextureDescriptor *Texture(MTLPixelFormat format, MTLStorageMode storage) {
  auto *const desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                         width:MetalProductFrameWidth
                                                        height:MetalProductFrameHeight
                                                     mipmapped:NO];
  desc.storageMode = storage;
  desc.usage = MTLTextureUsageRenderTarget;
  return desc;
}

struct ProductPipeline {
  id<MTLRenderPipelineState> Draw = nil;
  id<MTLDepthStencilState> Depth = nil;
};

struct ProductPass {
  id<MTLCommandQueue> Queue;
  ProductPipeline Tile;
  ProductPipeline Group;
  ProductPipeline Panorama;
  MTLRenderPassDescriptor *Pass;
  id<MTLTexture> Color;
};

std::string MetalError(NSError *error, const char *fallback) {
  const char *const reason = error.localizedDescription.UTF8String;
  return reason != nullptr ? reason : fallback;
}

enum class ProductDraw { Tile, Group, Panorama };

std::expected<ProductPipeline, std::string>
MakePipeline(id<MTLDevice> device, id<MTLLibrary> library, ProductDraw mode, bool composite) {
  const bool panorama = mode == ProductDraw::Panorama;
  const auto pipelineIndex = static_cast<size_t>(mode);
  NSError *error = nil;
  auto *const descriptor = [[MTLRenderPipelineDescriptor alloc] init];
  const std::array<NSString *, 3> vertices{@"tile_vertex", @"group_vertex", @"panorama_vertex"};
  const std::array<NSString *, 3> direct{@"tile_fragment", @"group_fragment", @"panorama_fragment"};
  const std::array<NSString *, 3> combined{
      @"tile_composite_fragment", @"group_composite_fragment", @"panorama_composite_fragment"};
  descriptor.vertexFunction = [library newFunctionWithName:vertices[pipelineIndex]];
  descriptor.fragmentFunction =
      [library newFunctionWithName:(composite ? combined : direct)[pipelineIndex]];
  descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm_sRGB;
  if (composite) { descriptor.colorAttachments[1].pixelFormat = MTLPixelFormatR32Float; }
  descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
  const id<MTLRenderPipelineState> pipeline =
      [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
  if (pipeline == nil) {
    return std::unexpected(MetalError(error, "Metal failed to create a product pipeline"));
  }
  auto *const depthSpec = [[MTLDepthStencilDescriptor alloc] init];
  depthSpec.depthCompareFunction =
      panorama ? MTLCompareFunctionGreaterEqual : MTLCompareFunctionGreater;
  if (panorama && composite) { depthSpec.depthCompareFunction = MTLCompareFunctionAlways; }
  depthSpec.depthWriteEnabled = !(panorama && composite);
  const id<MTLDepthStencilState> depth = [device newDepthStencilStateWithDescriptor:depthSpec];
  if (depth == nil) { return std::unexpected("Metal could not allocate product depth state"); }
  return ProductPipeline{.Draw = pipeline, .Depth = depth};
}

std::expected<ProductPass, std::string>
MakePass(id<MTLDevice> device, const MetalProductInputs &products, bool streaming) {
  NSError *error = nil;
  const bool hasGroups = streaming || !products.Groups.empty();
  const bool hasTiles = streaming || !products.Tiles.empty();
  NSString *const source =
      hasGroups ? [kShader stringByAppendingString:GroupImpostorShader] : kShader;
  const id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
  if (library == nil) {
    return std::unexpected(MetalError(error, "Metal failed to create product shaders"));
  }
  ProductPipeline tile;
  ProductPipeline group;
  ProductPipeline panorama;
  const bool composite = (hasTiles || hasGroups) && !products.Panoramas.empty();
  if (hasTiles) {
    const auto pipeline = MakePipeline(device, library, ProductDraw::Tile, composite);
    if (!pipeline) { return std::unexpected(pipeline.error()); }
    tile = *pipeline;
  }
  if (hasGroups) {
    const auto pipeline = MakePipeline(device, library, ProductDraw::Group, composite);
    if (!pipeline) { return std::unexpected(pipeline.error()); }
    group = *pipeline;
  }
  if (!products.Panoramas.empty()) {
    const auto pipeline = MakePipeline(device, library, ProductDraw::Panorama, composite);
    if (!pipeline) { return std::unexpected(pipeline.error()); }
    panorama = *pipeline;
  }
  auto *const pass = [MTLRenderPassDescriptor renderPassDescriptor];
  pass.colorAttachments[0].texture = [device
      newTextureWithDescriptor:Texture(MTLPixelFormatRGBA8Unorm_sRGB, MTLStorageModeShared)];
  pass.colorAttachments[0].loadAction = MTLLoadActionClear;
  pass.colorAttachments[0].clearColor =
      MTLClearColorMake(kClearColor[0], kClearColor[1], kClearColor[2], 1.0);
  pass.colorAttachments[0].storeAction = MTLStoreActionStore;
  if (composite) {
    auto *const attachment = pass.colorAttachments[1];
    attachment.texture =
        [device newTextureWithDescriptor:Texture(MTLPixelFormatR32Float, MTLStorageModeMemoryless)];
    if (attachment.texture == nil) {
      return std::unexpected("Metal could not allocate near depth");
    }
    attachment.loadAction = MTLLoadActionClear;
    attachment.clearColor = MTLClearColorMake(0, 0, 0, 0);
    attachment.storeAction = MTLStoreActionDontCare;
  }
  pass.depthAttachment.texture = [device
      newTextureWithDescriptor:Texture(MTLPixelFormatDepth32Float, MTLStorageModeMemoryless)];
  pass.depthAttachment.loadAction = MTLLoadActionClear;
  pass.depthAttachment.clearDepth = 0.0;
  pass.depthAttachment.storeAction = MTLStoreActionDontCare;
  ProductPass result{.Queue = [device newCommandQueue],
                     .Tile = tile,
                     .Group = group,
                     .Panorama = panorama,
                     .Pass = pass,
                     .Color = pass.colorAttachments[0].texture};
  if (result.Queue == nil || result.Color == nil || pass.depthAttachment.texture == nil) {
    return std::unexpected("Metal could not allocate the product pass");
  }
  return result;
}

struct TileMesh {
  id<MTLBuffer> Vertices = nil;
  id<MTLBuffer> Indices = nil;
  id<MTLBuffer> Colours = nil;
  size_t IndexCount = 0;
  size_t GroundIndexCount = 0;
  std::optional<size_t> HandoffGroup;
  SurfaceColourRanges ColourRanges{};
  SurfaceLightUniform Light{};
  Mat4 WorldFromTile;
};

Vec3f TileDirection(const Mat4 &placement, const Vec3 &direction) {
  const auto component = [&](size_t axis) {
    const size_t column = axis * 4;
    return static_cast<float>(placement[column] * direction[0] +
                              placement[column + 1] * direction[1] +
                              placement[column + 2] * direction[2]);
  };
  return {{component(0), component(1), component(2)}};
}

SurfaceShadingUniform FrameShading(const TileMesh &mesh, const Vec3 &eye, float weight) {
  SurfaceShadingUniform shading{.Light = mesh.Light, .EyeAndImpostorWeight = {0, 0, 0, weight}};
  if (mesh.ColourRanges[2] == 0 && mesh.Light[3] > 0) {
    const auto &p = mesh.WorldFromTile;
    const auto local = TileDirection(p, eye - Vec3{{p[12], p[13], p[14]}});
    shading.EyeAndImpostorWeight = {local[0], local[1], local[2], weight};
  }
  return shading;
}

size_t DrawTile(id<MTLRenderCommandEncoder> draw,
                const ProductPipeline &pipeline,
                const TileMesh &mesh,
                const ClipMatrix &clip,
                const Vec3 &eye,
                float weight) {
  const auto shading = FrameShading(mesh, eye, weight);
  const size_t indexCount = weight >= 1.0f ? mesh.GroundIndexCount : mesh.IndexCount;
  [draw setRenderPipelineState:pipeline.Draw];
  [draw setDepthStencilState:pipeline.Depth];
  [draw setVertexBuffer:mesh.Vertices offset:0 atIndex:0];
  [draw setVertexBytes:clip.data() length:sizeof(clip) atIndex:1];
  [draw setVertexBuffer:mesh.Colours == nil ? mesh.Vertices : mesh.Colours offset:0 atIndex:2];
  [draw setVertexBytes:mesh.ColourRanges.data() length:sizeof(mesh.ColourRanges) atIndex:3];
  [draw setFragmentBytes:&shading length:sizeof(shading) atIndex:0];
  [draw drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                   indexCount:indexCount
                    indexType:MTLIndexTypeUInt32
                  indexBuffer:mesh.Indices
            indexBufferOffset:0];
  return indexCount / 3;
}

std::expected<std::optional<double>, std::string> RetireFrame(id<MTLCommandBuffer> command) {
  [command waitUntilCompleted];
  if (command.status != MTLCommandBufferStatusCompleted) {
    return std::unexpected("Metal product command failed");
  }
  const double began = command.GPUStartTime;
  const double end = command.GPUEndTime;
  if (!std::isfinite(began) || began <= 0 || !std::isfinite(end) || end <= began) {
    return std::optional<double>{};
  }
  return (end - began) * kMsPerS;
}

template <typename Submit>
std::expected<void, std::string> MeasureCapture(size_t count,
                                                const Submit &submitAt,
                                                double frameSeconds,
                                                MetalProductFrames &frames) {
  std::array<id<MTLCommandBuffer>, MetalProductFramesInFlight> pending{};
  frames.Samples.resize(count);
  const auto finish = [&](size_t frame) -> std::expected<void, std::string> {
    auto &command = pending[frame % pending.size()];
    const auto elapsed = RetireFrame(command);
    if (!elapsed) { return std::unexpected(elapsed.error()); }
    frames.Samples[frame].GpuMs = *elapsed;
    command = nil;
    return {};
  };
  const auto windowAt = Clock::now();
  const auto available = std::chrono::duration<double>(Clock::time_point::max() - windowAt);
  if (frameSeconds > 0 && !(frameSeconds * static_cast<double>(count) < available.count())) {
    return std::unexpected("paced product capture exceeds the clock range");
  }
  for (size_t frame = 0; frame < count; ++frame) {
    if (frameSeconds > 0) {
      const auto offset = std::chrono::duration<double>(static_cast<double>(frame) * frameSeconds);
      std::this_thread::sleep_until(windowAt + std::chrono::duration_cast<Clock::duration>(offset));
    }
    const auto frameAt = Clock::now();
    auto &sample = frames.Samples[frame];
    const auto renderAt = Clock::now();
    if (frame >= pending.size()) {
      const auto retired = finish(frame - pending.size());
      if (!retired) { return retired; }
      sample.SyncWaitMs = ElapsedMs(renderAt);
    }
    const auto submitted = submitAt(frame);
    if (!submitted) { return std::unexpected(submitted.error()); }
    pending[frame % pending.size()] = *submitted;
    sample.RenderMs = ElapsedMs(renderAt);
    sample.WallMs = ElapsedMs(frameAt);
  }
  frames.WindowMs = ElapsedMs(windowAt);
  const auto flushAt = Clock::now();
  for (size_t frame = count > pending.size() ? count - pending.size() : 0; frame < count; ++frame) {
    if (const auto retired = finish(frame); !retired) { return retired; }
  }
  frames.FlushMs = ElapsedMs(flushAt);
  return {};
}

void FinishCapture(id<MTLDevice> device,
                   const ProductPass &pass,
                   MetalProductFrames &frames,
                   bool image) {
  frames.DeviceId = device.registryID;
  frames.ResourceBytes = device.currentAllocatedSize;
  frames.RecommendedResourceBytes = device.recommendedMaxWorkingSetSize;
  frames.UnifiedMemory = device.hasUnifiedMemory;
  if (image) {
    const size_t row = MetalProductFrameWidth * kChannels;
    frames.Rgba.resize(row * MetalProductFrameHeight);
    [pass.Color getBytes:frames.Rgba.data()
             bytesPerRow:row
              fromRegion:MTLRegionMake2D(0, 0, MetalProductFrameWidth, MetalProductFrameHeight)
             mipmapLevel:0];
  }
}

id<MTLTexture> UploadCube(id<MTLDevice> device,
                          MTLPixelFormat format,
                          size_t side,
                          size_t pixelBytes,
                          const std::string &pixels) {
  auto *const desc = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:format
                                                                           size:side
                                                                      mipmapped:NO];
  desc.storageMode = MTLStorageModeShared;
  desc.usage = MTLTextureUsageShaderRead;
  const id<MTLTexture> cube = [device newTextureWithDescriptor:desc];
  if (cube == nil) { return nil; }
  const size_t rowBytes = side * pixelBytes;
  const size_t sliceBytes = rowBytes * side;
  for (size_t face = 0; face < FarPanorama::FaceCount; ++face) {
    [cube replaceRegion:MTLRegionMake2D(0, 0, side, side)
            mipmapLevel:0
                  slice:face
              withBytes:pixels.data() + face * sliceBytes
            bytesPerRow:rowBytes
          bytesPerImage:sliceBytes];
  }
  return cube;
}

struct PanoramaTextures {
  id<MTLTexture> Colour;
  id<MTLTexture> Depth;
};

using PanoramaUniform = std::array<std::array<float, 4>, 7>;
static_assert(sizeof(PanoramaUniform) == size_t{7} * 4 * sizeof(float));

PanoramaUniform PackView(const PanoramaView &view) {
  const auto vector = [](const Vec3 &axis, float w = 0.0f) {
    return std::array{
        static_cast<float>(axis[0]), static_cast<float>(axis[1]), static_cast<float>(axis[2]), w};
  };
  return {vector(view.Forward),
          vector(view.RightSpan),
          vector(view.UpSpan),
          {static_cast<float>(view.DepthScaleM),
           static_cast<float>(view.DepthBias),
           static_cast<float>(view.Exposure),
           static_cast<float>(view.FocalPixels)},
          vector(view.EyeOffsetM),
          vector(view.NextEyeOffsetM, static_cast<float>(view.NextWeight)),
          {static_cast<float>(view.AxialNearM),
           static_cast<float>(view.CoverageFadeM),
           static_cast<float>(view.AxialOverlapEndM),
           0}};
}

void DrawPanorama(id<MTLRenderCommandEncoder> draw,
                  const ProductPipeline &pipeline,
                  const PanoramaTextures &textures,
                  const PanoramaTextures &next,
                  const PanoramaView &view) {
  const auto packed = PackView(view);
  [draw setRenderPipelineState:pipeline.Draw];
  [draw setDepthStencilState:pipeline.Depth];
  [draw setFragmentTexture:textures.Colour atIndex:0];
  [draw setFragmentTexture:textures.Depth atIndex:1];
  [draw setFragmentTexture:next.Colour atIndex:2];
  [draw setFragmentTexture:next.Depth atIndex:3];
  [draw setFragmentBytes:packed.data() length:sizeof(packed) atIndex:0];
  [draw drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
}

SurfaceLightUniform PackLight(const Mat4 &worldFromTile, const PanoramaLighting *light) {
  if (light == nullptr) { return {}; }
  const auto toward =
      TileDirection(worldFromTile, {{light->ToSun[0], light->ToSun[1], light->ToSun[2]}});
  return {toward[0],
          toward[1],
          toward[2],
          1,
          light->Direct[0],
          light->Direct[1],
          light->Direct[2],
          0,
          light->Sky[0],
          light->Sky[1],
          light->Sky[2],
          0,
          light->Ground[0],
          light->Ground[1],
          light->Ground[2],
          0};
}

struct ProductResources {
  std::vector<TileMesh> Meshes;
  std::vector<PanoramaTextures> Panoramas;
  std::vector<GroupResources> Groups;
  size_t Bytes = 0;
};

std::expected<TileMesh, std::string> UploadTile(id<MTLDevice> device,
                                                const MetalSurface &tile,
                                                const PanoramaLighting *light,
                                                size_t &bytes) {
  const auto vertices = std::as_bytes(std::span(tile.Product.Vertices));
  const auto indices = std::as_bytes(std::span(tile.Product.Indices));
  const auto colours = std::as_bytes(std::span(tile.Product.Colours));
  const auto count = static_cast<uint32_t>(tile.Product.Vertices.size());
  const auto firstColour = count - static_cast<uint32_t>(tile.Product.Colours.size());
  const uint32_t firstOverlay =
      tile.Product.GridSide > 0 ? tile.Product.GridSide * tile.Product.GridSide : count;
  const uint32_t firstLit = tile.Product.GroundRoughness ? 0 : firstOverlay;
  TileMesh mesh{.Vertices = [device newBufferWithBytes:vertices.data()
                                                length:vertices.size()
                                               options:MTLResourceStorageModeShared],
                .Indices = [device newBufferWithBytes:indices.data()
                                               length:indices.size()
                                              options:MTLResourceStorageModeShared],
                .Colours = colours.empty()
                               ? nil
                               : [device newBufferWithBytes:colours.data()
                                                     length:colours.size()
                                                    options:MTLResourceStorageModeShared],
                .IndexCount = tile.Product.Indices.size(),
                .GroundIndexCount = tile.Product.GroundIndices,
                .HandoffGroup = tile.HandoffGroup,
                .ColourRanges = {firstColour, firstOverlay, firstLit, 0},
                .Light = PackLight(tile.WorldFromTile, firstLit < count ? light : nullptr),
                .WorldFromTile = tile.WorldFromTile};
  if (mesh.Vertices == nil || mesh.Indices == nil || (!colours.empty() && mesh.Colours == nil)) {
    return std::unexpected("Metal tile upload failed");
  }
  bytes += vertices.size() + indices.size() + colours.size();
  return mesh;
}

std::expected<GroupResources, std::string> UploadLitGroup(id<MTLDevice> device,
                                                          const MetalImpostorGroup &group,
                                                          const PanoramaLighting *light,
                                                          size_t &bytes) {
  auto uploaded = UploadMetalGroup(device, group, bytes);
  if (uploaded) { uploaded->Light = PackLight(group.WorldFromTile, light); }
  return uploaded;
}

std::expected<ProductResources, std::string> UploadProducts(id<MTLDevice> device,
                                                            const MetalProductInputs &products,
                                                            const PanoramaLighting *light) {
  ProductResources resources;
  resources.Meshes.reserve(products.Tiles.size());
  for (const auto &tile : products.Tiles) {
    if (tile.HandoffGroup && *tile.HandoffGroup >= products.Groups.size()) {
      return std::unexpected("surface handoff references an unavailable group");
    }
    const auto mesh = UploadTile(device, tile, light, resources.Bytes);
    if (!mesh) { return std::unexpected(mesh.error()); }
    resources.Meshes.push_back(*mesh);
  }
  resources.Panoramas.reserve(products.Panoramas.size());
  for (const auto &panorama : products.Panoramas) {
    const PanoramaTextures textures = {.Colour = UploadCube(device,
                                                            MTLPixelFormatRGBA16Float,
                                                            panorama.Side,
                                                            FarPanorama::ColourBytesPerTexel,
                                                            panorama.Colour),
                                       .Depth = UploadCube(device,
                                                           MTLPixelFormatR32Float,
                                                           panorama.Side,
                                                           FarPanorama::DepthBytesPerTexel,
                                                           panorama.Depth)};
    if (textures.Colour == nil || textures.Depth == nil) {
      return std::unexpected("Metal panorama upload failed");
    }
    resources.Panoramas.push_back(textures);
    resources.Bytes += panorama.Colour.size() + panorama.Depth.size();
  }
  resources.Groups.reserve(products.Groups.size());
  for (const auto &group : products.Groups) {
    auto uploaded = UploadLitGroup(device, group, light, resources.Bytes);
    if (!uploaded) { return std::unexpected(uploaded.error()); }
    resources.Groups.push_back(std::move(*uploaded));
  }
  return resources;
}

bool MatchesSurface(const MetalSurface &active, const MetalPlacedSurface &replacement) {
  const auto &front = active.Product;
  const auto &back = replacement.Product;
  return front.Coverage == back.Coverage && front.Revision == back.Revision &&
         active.WorldFromTile == replacement.WorldFromTile && front.Indices == back.Indices &&
         front.GroundRoughness == back.GroundRoughness && front.Colours == back.Colours &&
         std::ranges::equal(std::as_bytes(std::span(front.Vertices)),
                            std::as_bytes(std::span(back.Vertices)));
}

struct PreparedSurfaceReplacement {
  std::vector<TileMesh> Meshes;
  std::vector<GroupResources> Groups;
  MetalSurfaceTransition Transition;
  size_t Bytes = 0;
};

PreparedSurfaceReplacement PrepareReplacement(id<MTLDevice> device,
                                              const MetalSurfaceStream &stream,
                                              std::span<const MetalSurface> active,
                                              std::span<const TileMesh> resident,
                                              const PanoramaLighting *light) {
  @autoreleasepool {
    const auto start = Clock::now();
    auto candidate = stream.Load();
    PreparedSurfaceReplacement prepared;
    if (!candidate || candidate->Tiles.empty()) {
      prepared.Transition.Error = candidate ? "empty replacement surface cut" : candidate.error();
      return prepared;
    }
    const auto upload = Clock::now();
    prepared.Meshes.reserve(candidate->Tiles.size());
    for (const auto &tile : candidate->Tiles) {
      if (tile.HandoffGroup && *tile.HandoffGroup >= candidate->Groups.size()) {
        prepared.Transition.Error = "replacement surface handoff references an unavailable group";
        return prepared;
      }
      const auto same = std::ranges::find_if(
          active, [&](const MetalSurface &surface) { return MatchesSurface(surface, tile); });
      if (same != active.end()) {
        prepared.Meshes.push_back(resident[static_cast<size_t>(same - active.begin())]);
        prepared.Meshes.back().HandoffGroup = tile.HandoffGroup;
        ++prepared.Transition.ReusedTiles;
      } else {
        auto mesh = UploadTile(device,
                               {.Product = tile.Product,
                                .WorldFromTile = tile.WorldFromTile,
                                .HandoffGroup = tile.HandoffGroup},
                               light,
                               prepared.Bytes);
        if (!mesh) {
          prepared.Transition.Error = mesh.error();
          return prepared;
        }
        prepared.Meshes.push_back(*mesh);
      }
      prepared.Transition.Triangles += tile.Product.Indices.size() / 3;
    }
    prepared.Groups.reserve(candidate->Groups.size());
    for (const auto &group : candidate->Groups) {
      auto uploaded = UploadLitGroup(device,
                                     {.Views = group.Views,
                                      .WorldFromTile = group.WorldFromTile,
                                      .CentreM = group.CentreM,
                                      .HalfExtentM = group.HalfExtentM,
                                      .Handoff = group.Handoff,
                                      .FractionalCoverage = group.FractionalCoverage},
                                     light,
                                     prepared.Bytes);
      if (!uploaded) {
        prepared.Transition.Error = uploaded.error();
        return prepared;
      }
      prepared.Groups.push_back(std::move(*uploaded));
    }
    prepared.Transition.Tiles = prepared.Meshes.size();
    prepared.Transition.ReadyMs = ElapsedMs(start);
    prepared.Transition.UploadMs = ElapsedMs(upload);
    return prepared;
  }
}

void RecordReplacement(PreparedSurfaceReplacement &replacement, MetalProductFrames &frames) {
  frames.UploadedBytes += replacement.Bytes;
  frames.Transition = std::move(replacement.Transition);
}

float GroupWeight(const GroupResources &group, const Vec3 &eye) {
  if (!group.Handoff) { return 1.0f; }
  const auto centre = group.WorldFromTile.TransformPoint(group.CentreM);
  return group.Handoff->Weight(Length(eye - centre));
}

std::expected<id<MTLCommandBuffer>, std::string> SubmitProducts(const ProductPass &pass,
                                                                const ProductResources &resources,
                                                                const MetalProductView &view,
                                                                MetalProductFrameSample &sample) {
  @autoreleasepool {
    const id<MTLCommandBuffer> command = [pass.Queue commandBuffer];
    const id<MTLRenderCommandEncoder> draw = [command renderCommandEncoderWithDescriptor:pass.Pass];
    if (draw == nil) { return std::unexpected("Metal could not encode the product frame"); }
    for (const auto &mesh : resources.Meshes) {
      const auto clip = MetalClip(view.ClipFromWorld * mesh.WorldFromTile);
      if (!clip) {
        [draw endEncoding];
        return std::unexpected(clip.error());
      }
      const float weight =
          mesh.HandoffGroup ? GroupWeight(resources.Groups[*mesh.HandoffGroup], view.EyeM) : 0;
      sample.TileTriangles += DrawTile(draw, pass.Tile, mesh, *clip, view.EyeM, weight);
    }
    if (pass.Group.Draw != nil) {
      [draw setRenderPipelineState:pass.Group.Draw];
      [draw setDepthStencilState:pass.Group.Depth];
    }
    for (const auto &group : resources.Groups) {
      const float weight = GroupWeight(group, view.EyeM);
      if (weight <= 0.0f) { continue; }
      if (weight < 1.0f) { ++sample.BlendingGroups; }
      const auto clip = MetalClip(view.ClipFromWorld * group.WorldFromTile);
      if (!clip) {
        [draw endEncoding];
        return std::unexpected(clip.error());
      }
      const auto &p = group.WorldFromTile;
      const auto local = TileDirection(p, view.EyeM - Vec3{{p[12], p[13], p[14]}});
      const SurfaceShadingUniform shading{
          .Light = group.Light, .EyeAndImpostorWeight = {local[0], local[1], local[2], weight}};
      [draw setVertexBytes:clip->data() length:sizeof(*clip) atIndex:1];
      [draw setFragmentBytes:&shading length:sizeof(shading) atIndex:0];
      const uint32_t fractional = group.FractionalCoverage ? 1U : 0U;
      [draw setFragmentBytes:&fractional length:sizeof(fractional) atIndex:1];
      sample.GroupTriangles += EncodeMetalGroup(draw, group, {{local[0], local[1], local[2]}});
    }
    if (pass.Panorama.Draw != nil) {
      const auto &active = resources.Panoramas[view.PanoramaIndex];
      const auto &next = view.Panorama.NextWeight > 0 ? resources.Panoramas.back() : active;
      DrawPanorama(draw, pass.Panorama, active, next, view.Panorama);
    }
    [draw endEncoding];
    [command commit];
    return command;
  }
}

}

std::expected<MetalProductFrames, std::string>
RenderMetalProducts(const MetalProductInputs &products,
                    size_t count,
                    const MetalProductViewAt &viewAt,
                    bool readImage,
                    const MetalSurfaceStream &stream) {
  if (products.Tiles.empty() && products.Panoramas.empty() && products.Groups.empty()) {
    return std::unexpected("Metal product capture requires an input product");
  }
  @autoreleasepool {
    const auto began = Clock::now();
    const id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) { return std::unexpected("Metal device unavailable"); }
    const auto pass = MakePass(device, products, static_cast<bool>(stream.Load));
    if (!pass) { return std::unexpected(pass.error()); }
    MetalProductFrames frames;
    const char *const name = device.name.UTF8String;
    frames.Device = name != nullptr ? name : "Metal";
    frames.SetupMs = ElapsedMs(began);
    const auto upload = Clock::now();
    const auto *light = products.Panoramas.empty() || !products.Panoramas.front().Lighting
                            ? nullptr
                            : &*products.Panoramas.front().Lighting;
    auto resources = UploadProducts(device, products, light);
    if (!resources) { return std::unexpected(resources.error()); }
    frames.UploadMs = ElapsedMs(upload);
    frames.UploadedBytes = resources->Bytes;
    std::future<PreparedSurfaceReplacement> pending;
    if (stream.Load) {
      pending = std::async(std::launch::async, [&] {
        return PrepareReplacement(device, stream, products.Tiles, resources->Meshes, light);
      });
    }
    const auto submit = [&](size_t frame) -> std::expected<id<MTLCommandBuffer>, std::string> {
      if (frame > 0 && pending.valid() &&
          pending.wait_for(Clock::duration::zero()) == std::future_status::ready) {
        auto replacement = pending.get();
        if (replacement.Transition.Error.empty()) {
          resources->Meshes.swap(replacement.Meshes);
          resources->Groups.swap(replacement.Groups);
          replacement.Transition.FirstFrame = frame;
        }
        RecordReplacement(replacement, frames);
      }
      frames.Samples[frame].Refined =
          !stream.Load || (frames.Transition && frames.Transition->FirstFrame.has_value());
      const auto view = viewAt(frame);
      if (!view) { return std::unexpected(view.error()); }
      if (!resources->Panoramas.empty() && view->PanoramaIndex >= resources->Panoramas.size()) {
        return std::unexpected("Metal panorama view references an unavailable product");
      }
      return SubmitProducts(*pass, *resources, *view, frames.Samples[frame]);
    };
    if (const auto measured = MeasureCapture(count, submit, stream.FrameSeconds, frames);
        !measured) {
      return std::unexpected(measured.error());
    }
    if (pending.valid()) {
      auto replacement = pending.get();
      RecordReplacement(replacement, frames);
    }
    FinishCapture(device, *pass, frames, readImage);
    return frames;
  }
}
}
