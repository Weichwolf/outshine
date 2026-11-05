#include "MetalInternal.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace outshine::Render::Native::Metal {
namespace {
constexpr std::string_view kIndexedKernel = R"(
#include <metal_stdlib>
#include <metal_command_buffer>
using namespace metal;
struct DrawArgs { uint count, instances, first; int base; uint instance; };
struct Commands { command_buffer value [[id(0)]]; };
struct Parameters { uint capacity, groups, unit, pad; ulong offset; };
static_assert(sizeof(DrawArgs) == 20 && alignof(DrawArgs) == 4);
static_assert(__builtin_offsetof(DrawArgs, instances) == 4);
static_assert(__builtin_offsetof(DrawArgs, first) == 8);
static_assert(__builtin_offsetof(DrawArgs, base) == 12);
static_assert(__builtin_offsetof(DrawArgs, instance) == 16);
static_assert(sizeof(Parameters) == 24 && alignof(Parameters) == 8);
static_assert(__builtin_offsetof(Parameters, groups) == 4);
static_assert(__builtin_offsetof(Parameters, unit) == 8);
static_assert(__builtin_offsetof(Parameters, offset) == 16);
static_assert(sizeof(uint2) == 8 && alignof(uint2) == 8);
kernel void indexedCommands(device const DrawArgs *args [[buffer(0)]],
                            device Commands &commands [[buffer(1)]],
                            device const uchar *indices [[buffer(2)]],
                            constant uint2 *spans [[buffer(3)]],
                            device uint2 *ranges [[buffer(4)]],
                            constant Parameters &p [[buffer(5)]], uint group [[thread_position_in_grid]]) {
  if (group >= p.groups) return;
  const uint2 span = spans[group];
  uint kept = 0;
  for (uint one = 0; one < span.y; ++one) {
    const uint id = span.x + one;
    const DrawArgs a = args[id];
    if (a.count == 0 || a.instances == 0) continue;
    render_command command(commands.value, span.x + kept++);
    command.reset();
    if (p.unit == 4) {
      const device uint *first = reinterpret_cast<device const uint *>(indices + p.offset) + a.first;
      command.draw_indexed_primitives(primitive_type::triangle, a.count, first, a.instances, a.base, a.instance);
    } else {
      const device ushort *first = reinterpret_cast<device const ushort *>(indices + p.offset) + a.first;
      command.draw_indexed_primitives(primitive_type::triangle, a.count, first, a.instances, a.base, a.instance);
    }
  }
  ranges[group] = uint2(span.x, kept);
}
)";

struct Parameters {
  uint32_t Capacity, Groups, Unit, Pad = 0;
  uint64_t Offset;
};

static_assert(sizeof(Parameters) == 24 && alignof(Parameters) == 8);
static_assert(offsetof(Parameters, Capacity) == 0 && offsetof(Parameters, Groups) == 4);
static_assert(offsetof(Parameters, Unit) == 8 && offsetof(Parameters, Pad) == 12);
static_assert(offsetof(Parameters, Offset) == 16);

Result<std::shared_ptr<IndexedKernel>> Kernel(id<MTLDevice> device) {
  NSError *error = nil;
  MTLCompileOptions *const options = [MTLCompileOptions new];
  options.mathMode = MTLMathModeSafe;
  const id<MTLLibrary> library = [device newLibraryWithSource:Text(kIndexedKernel)
                                                      options:options
                                                        error:&error];
  const id<MTLFunction> function = [library newFunctionWithName:@"indexedCommands"];
  const id<MTLComputePipelineState> pipeline =
      function != nil ? [device newComputePipelineStateWithFunction:function error:&error] : nil;
  if (pipeline == nil) { return Failure(ErrorKind::Driver, error.localizedDescription.UTF8String); }
  return std::make_shared<IndexedKernel>(IndexedKernel{
      .Pipeline = pipeline, .Arguments = [function newArgumentEncoderWithBufferIndex:1]});
}
}

Result<std::unique_ptr<IndexedCommands>> MetalDevice::CreateIndexedCommands(uint32_t capacity) {
  if (!OnOwner() || capacity == 0 || !Limits_.PreparedIndexedCommands ||
      uint64_t{capacity} * sizeof(IndexedCommandSpan) > Limits_.MaxStorageBufferBytes) {
    return Failure(ErrorKind::Invalid, "invalid Metal indexed command capacity or owner");
  }
  if (!IndexedKernel_) {
    auto made = Kernel(Native_);
    if (!made) { return std::unexpected(made.error()); }
    IndexedKernel_ = std::move(*made);
  }
  MTLIndirectCommandBufferDescriptor *const description = [MTLIndirectCommandBufferDescriptor new];
  description.commandTypes = MTLIndirectCommandTypeDrawIndexed;
  description.inheritPipelineState = true;
  description.inheritBuffers = true;
  auto state = std::make_shared<IndexedCommandState>();
  state->Device = Identity_;
  state->Capacity = capacity;
  state->Kernel = IndexedKernel_;
  state->Commands = [Native_ newIndirectCommandBufferWithDescriptor:description
                                                    maxCommandCount:capacity
                                                            options:MTLResourceStorageModePrivate];
  state->Argument = [Native_ newBufferWithLength:IndexedKernel_->Arguments.encodedLength
                                         options:MTLResourceStorageModeShared];
  state->Ranges = [Native_ newBufferWithLength:uint64_t{capacity} * sizeof(IndexedCommandSpan)
                                       options:MTLResourceStorageModePrivate];
  if (state->Commands == nil || state->Argument == nil || state->Ranges == nil) {
    return Failure(ErrorKind::OutOfMemory, "Metal indexed command storage allocation failed");
  }
  [IndexedKernel_->Arguments setArgumentBuffer:state->Argument offset:0];
  [IndexedKernel_->Arguments setIndirectCommandBuffer:state->Commands atIndex:0];
  return std::make_unique<MetalIndexedCommands>(std::move(state));
}

Result<void> MetalCommands::PrepareIndexedCommands(IndexedCommands &commands,
                                                   const IndexBuffer &indices,
                                                   const Buffer &arguments,
                                                   std::span<const IndexedCommandSpan> ranges) {
  const auto *into = dynamic_cast<const MetalIndexedCommands *>(&commands);
  const auto *index = dynamic_cast<const MetalBuffer *>(indices.Data);
  const auto *args = dynamic_cast<const MetalBuffer *>(&arguments);
  const uint64_t unit = indices.Type == IndexType::Uint32 ? 4u : 2u;
  if (!OnOwner() || Render || into == nullptr || index == nullptr || args == nullptr ||
      into->State->Device != Device || index->State->Device != Device ||
      args->State->Device != Device || !Has(index->Description().Use, BufferUse::Index) ||
      !Has(args->Description().Use, BufferUse::Indirect) ||
      (indices.Type != IndexType::Uint16 && indices.Type != IndexType::Uint32) ||
      !Fits(indices.Range, index->Description().Bytes) || indices.Range.Offset % unit != 0 ||
      indices.Range.Bytes % unit != 0 || ranges.empty() || ranges.size() > into->State->Capacity) {
    return Failure(ErrorKind::Invalid, "invalid Metal indexed preparation resources or owner");
  }
  uint64_t end = 0;
  for (const auto &range : ranges) {
    if (range.Count == 0 || range.First < end || range.First > into->State->Capacity ||
        range.Count > into->State->Capacity - range.First) {
      return Failure(ErrorKind::Invalid, "indexed command spans overlap or exceed capacity");
    }
    end = uint64_t{range.First} + range.Count;
  }
  if (end > args->Description().Bytes / kIndexedIndirectBytes) {
    return Failure(ErrorKind::Invalid, "indexed command spans exceed arguments");
  }
  for (const auto &prepared : PreparedIndexed) {
    if (prepared.State == into->State) {
      return Failure(ErrorKind::Invalid, "indexed commands prepared twice in one recording");
    }
  }
  std::vector<id<MTLResource>> hazards;
  const std::array reads{index->State, args->State};
  const auto held = PrepareReads(reads, hazards);
  if (!held) { return held; }
  if (Compute == nil && Profile) {
    MTLComputePassDescriptor *const pass = [MTLComputePassDescriptor computePassDescriptor];
    ProfileComputeEncoder(*Profile, pass, @"indexedCommands");
    Compute = [Native computeCommandEncoderWithDescriptor:pass];
  } else if (Compute == nil) {
    Compute = [Native computeCommandEncoder];
  }
  if (Compute == nil) { return Failure(ErrorKind::Driver, "Metal compute encoder unavailable"); }
  if (!hazards.empty()) {
    [Compute memoryBarrierWithResources:hazards.data() count:hazards.size()];
  }
  const auto &state = *into->State;
  [Compute setComputePipelineState:state.Kernel->Pipeline];
  [Compute setBuffer:args->State->Native offset:0 atIndex:0];
  [Compute setBuffer:state.Argument offset:0 atIndex:1];
  [Compute setBuffer:index->State->Native offset:0 atIndex:2];
  if (ranges.size_bytes() <= 4096) {
    [Compute setBytes:ranges.data() length:ranges.size_bytes() atIndex:3];
  } else {
    const id<MTLBuffer> spans = [Native.device newBufferWithBytes:ranges.data()
                                                           length:ranges.size_bytes()
                                                          options:MTLResourceStorageModeShared];
    if (spans == nil) {
      return Failure(ErrorKind::OutOfMemory, "indexed span staging allocation failed");
    }
    [Compute setBuffer:spans offset:0 atIndex:3];
  }
  [Compute setBuffer:state.Ranges offset:0 atIndex:4];
  const Parameters parameters{.Capacity = state.Capacity,
                              .Groups = static_cast<uint32_t>(ranges.size()),
                              .Unit = static_cast<uint32_t>(unit),
                              .Offset = indices.Range.Offset};
  [Compute setBytes:&parameters length:sizeof(parameters) atIndex:5];
  [Compute useResource:state.Commands usage:MTLResourceUsageWrite];
  [Compute dispatchThreads:MTLSizeMake(ranges.size(), 1, 1)
      threadsPerThreadgroup:MTLSizeMake(std::min<size_t>(32, ranges.size()), 1, 1)];
  UpdateAccess(index->State->Native, Access::Read);
  UpdateAccess(args->State->Native, Access::Read);
  PreparedIndexed.push_back({.State = into->State,
                             .Arguments = args->State,
                             .Indices = index->State,
                             .IndexBinding = indices,
                             .Spans = {ranges.begin(), ranges.end()}});
  return {};
}
}
