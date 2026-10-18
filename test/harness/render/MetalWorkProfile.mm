#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <atomic>
#include <array>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cstdlib>
#include <cmath>
#include <print>
#include <limits>
#include <optional>

@interface OutshineProfilePass : NSObject
@property NSMutableSet<NSString *> *pipelines;
@property NSString *currentPipeline;
@property NSMutableDictionary<NSString *, NSNumber *> *work;
@property BOOL compute;
@property NSUInteger sampleOffset;
@property uint64_t groups;
@property uint64_t declaredThreads;
@property uint64_t unavailableDispatches;
@property uint64_t directIndices;
@property uint64_t calls;
@property uint64_t indirectCalls;
@property uint64_t indirectIndices;
@property uint64_t unavailableIndirect;
@end
@implementation OutshineProfilePass

- (instancetype)init {
  self = [super init];
  if (self != nullptr) {
    _pipelines = [NSMutableSet new];
    _work = [NSMutableDictionary new];
    _currentPipeline = @"unnamed";
  }
  return self;
}

@end
@interface OutshineProfileBuffer : NSObject
@property id<MTLBuffer> source;
@property id<MTLBuffer> readback;
@property NSUInteger bytes;
@end
@implementation OutshineProfileBuffer
@end
@interface OutshineProfileIndirect : NSObject
@property OutshineProfileBuffer *buffer;
@property OutshineProfilePass *pass;
@property NSString *pipeline;
@property NSUInteger offset;
@end
@implementation OutshineProfileIndirect
@end
@interface OutshineProfileCommand : NSObject
@property id<MTLCounterSampleBuffer> samples;
@property NSMutableArray<OutshineProfilePass *> *passes;
@property NSMutableDictionary<NSValue *, OutshineProfileBuffer *> *buffers;
@property NSMutableArray<OutshineProfileIndirect *> *indirect;
@property NSUInteger sampleCount;
@property uint64_t serial;
@property uint64_t omittedPasses;
@end
@implementation OutshineProfileCommand

- (instancetype)init {
  self = [super init];
  if (self != nullptr) {
    _passes = [NSMutableArray new];
    _buffers = [NSMutableDictionary new];
    _indirect = [NSMutableArray new];
  }
  return self;
}

@end

@interface OutshineProfileHooks : NSObject
- (void)drawIndexedPrimitives:(MTLPrimitiveType)type
                   indexCount:(NSUInteger)count
                    indexType:(MTLIndexType)indexType
                  indexBuffer:(id<MTLBuffer>)buffer
            indexBufferOffset:(NSUInteger)offset
                instanceCount:(NSUInteger)instances
                   baseVertex:(NSInteger)base
                 baseInstance:(NSUInteger)first;
- (void)drawPrimitives:(MTLPrimitiveType)type
           vertexStart:(NSUInteger)first
           vertexCount:(NSUInteger)count
         instanceCount:(NSUInteger)instances
          baseInstance:(NSUInteger)firstInstance;
- (void)dispatchThreadgroups:(MTLSize)groups threadsPerThreadgroup:(MTLSize)threads;
@end

namespace {
char traceKey, passKey, encoderTraceKey;
std::atomic<uint64_t> serial{0};
id<MTLCounterSet> counterSet;
IMP renderOriginal, commitOriginal, pipelineOriginal, indexedOriginal, indirectOriginal,
    drawOriginal;
IMP computeOriginal, computePipelineOriginal, dispatchOriginal;
bool encoderPatched = false;
bool computeEncoderPatched = false;

struct IndirectArguments {
  uint32_t count, instances, first;
  int32_t vertex;
  uint32_t instance;
};

constexpr size_t kIndirectArgumentBytes = 20;
static_assert(sizeof(IndirectArguments) == kIndirectArgumentBytes);
static_assert(alignof(IndirectArguments) == alignof(uint32_t));
static_assert(offsetof(IndirectArguments, count) == 0);
static_assert(offsetof(IndirectArguments, instances) == 4);
static_assert(offsetof(IndirectArguments, first) == 8);
static_assert(offsetof(IndirectArguments, vertex) == 12);
static_assert(offsetof(IndirectArguments, instance) == 16);
static_assert(sizeof(MTLCounterResultTimestamp) == sizeof(uint64_t));
static_assert(offsetof(MTLCounterResultTimestamp, timestamp) == 0);

void Pipeline(id self, SEL sel, id<MTLRenderPipelineState> state) {
  OutshineProfilePass *const pass = objc_getAssociatedObject(self, &passKey);
  if (pass != nullptr) {
    pass.currentPipeline = state.label != nil ? state.label : @"unnamed";
    [pass.pipelines addObject:pass.currentPipeline];
  }
  reinterpret_cast<void (*)(id, SEL, id<MTLRenderPipelineState>)>(pipelineOriginal)(
      self, sel, state);
}

void Indirect(id self,
              SEL sel,
              MTLPrimitiveType type,
              MTLIndexType indexType,
              id<MTLBuffer> index,
              NSUInteger indexOffset,
              id<MTLBuffer> args,
              NSUInteger offset) {
  OutshineProfilePass *const pass = objc_getAssociatedObject(self, &passKey);
  OutshineProfileCommand *const trace = objc_getAssociatedObject(self, &encoderTraceKey);
  if (pass != nullptr) {
    ++pass.calls;
    ++pass.indirectCalls;
  }
  if ((trace != nullptr) && trace.indirect.count < 65536 &&
      offset <= size_t{1024} * 1024 - sizeof(IndirectArguments) &&
      offset + sizeof(IndirectArguments) <= args.length) {
    NSValue *const key = [NSValue valueWithPointer:(__bridge void *)args];
    OutshineProfileBuffer *buffer = trace.buffers[key];
    if ((buffer == nullptr) && trace.buffers.count < 8) {
      buffer = [OutshineProfileBuffer new];
      buffer.source = args;
      trace.buffers[key] = buffer;
    }
    if (buffer != nullptr) {
      buffer.bytes = MAX(buffer.bytes, offset + sizeof(IndirectArguments));
      const auto call = [OutshineProfileIndirect new];
      call.pass = pass;
      call.pipeline = pass.currentPipeline;
      call.buffer = buffer;
      call.offset = offset;
      [trace.indirect addObject:call];
    } else {
      ++pass.unavailableIndirect;
    }
  } else if (pass != nullptr) {
    ++pass.unavailableIndirect;
  }
  reinterpret_cast<void (*)(id,
                            SEL,
                            MTLPrimitiveType,
                            MTLIndexType,
                            id<MTLBuffer>,
                            NSUInteger,
                            id<MTLBuffer>,
                            NSUInteger)>(indirectOriginal)(
      self, sel, type, indexType, index, indexOffset, args, offset);
}

IMP Patch(Class cls, SEL sel, IMP hook) {
  Method m = class_getInstanceMethod(cls, sel);
  if (m == nullptr) {
    std::println(stderr, "METAL_PASS missing {}", sel_getName(sel));
    std::abort();
  }
  IMP old = method_getImplementation(m);
  class_addMethod(cls, sel, old, method_getTypeEncoding(m));
  m = class_getInstanceMethod(cls, sel);
  if (m == nullptr) { std::abort(); }
  return method_setImplementation(m, hook);
}

IMP Patch(Class cls, SEL sel) {
  auto *const replacement = class_getInstanceMethod([OutshineProfileHooks class], sel);
  if (replacement == nullptr) { std::abort(); }
  return Patch(cls, sel, method_getImplementation(replacement));
}

OutshineProfileCommand *Trace(id self) {
  OutshineProfileCommand *trace = objc_getAssociatedObject(self, &traceKey);
  if (trace == nullptr) {
    trace = [OutshineProfileCommand new];
    trace.serial = ++serial;
    const auto d = [MTLCounterSampleBufferDescriptor new];
    d.counterSet = counterSet;
    d.storageMode = MTLStorageModeShared;
    d.sampleCount = 256;
    NSError *error = nil;
    trace.samples =
        [((id<MTLCommandBuffer>)self).device newCounterSampleBufferWithDescriptor:d error:&error];
    objc_setAssociatedObject(self, &traceKey, trace, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return trace;
}

id<MTLRenderCommandEncoder> Render(id self, SEL sel, MTLRenderPassDescriptor *descriptor) {
  OutshineProfileCommand *const trace = Trace(self);
  const NSUInteger ordinal = trace.passes.count;
  if (ordinal >= 64) {
    ++trace.omittedPasses;
    const auto encoder =
        reinterpret_cast<id<MTLRenderCommandEncoder> (*)(id, SEL, MTLRenderPassDescriptor *)>(
            renderOriginal)(self, sel, descriptor);
    if (encoder != nullptr) {
      objc_setAssociatedObject(encoder, &passKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
      objc_setAssociatedObject(encoder, &encoderTraceKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return encoder;
  }
  OutshineProfilePass *const pass = [OutshineProfilePass new];
  pass.sampleOffset = trace.sampleCount;
  trace.sampleCount += 4;
  [trace.passes addObject:pass];
  if ((trace.samples != nullptr) && ordinal < 64) {
    descriptor = [descriptor copy];
    const auto s = descriptor.sampleBufferAttachments[0];
    s.sampleBuffer = trace.samples;
    s.startOfVertexSampleIndex = pass.sampleOffset;
    s.endOfVertexSampleIndex = pass.sampleOffset + 1;
    s.startOfFragmentSampleIndex = pass.sampleOffset + 2;
    s.endOfFragmentSampleIndex = pass.sampleOffset + 3;
  }
  const auto encoder =
      reinterpret_cast<id<MTLRenderCommandEncoder> (*)(id, SEL, MTLRenderPassDescriptor *)>(
          renderOriginal)(self, sel, descriptor);
  if ((encoder != nullptr) && !encoderPatched) {
    const Class cls = object_getClass(encoder);
    pipelineOriginal = Patch(cls, @selector(setRenderPipelineState:), (IMP)Pipeline);
    indexedOriginal = Patch(cls,
                            @selector(drawIndexedPrimitives:indexCount:indexType:indexBuffer:
                                      indexBufferOffset:instanceCount:baseVertex:baseInstance:));
    indirectOriginal = Patch(cls,
                             @selector(drawIndexedPrimitives:indexType:indexBuffer:
                                       indexBufferOffset:indirectBuffer:indirectBufferOffset:),
                             (IMP)Indirect);
    drawOriginal =
        Patch(cls, @selector(drawPrimitives:vertexStart:vertexCount:instanceCount:baseInstance:));
    encoderPatched = true;
  }
  if (encoder != nullptr) {
    objc_setAssociatedObject(encoder, &passKey, pass, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(encoder, &encoderTraceKey, trace, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return encoder;
}

void ComputePipeline(id self, SEL sel, id<MTLComputePipelineState> state) {
  OutshineProfilePass *const pass = objc_getAssociatedObject(self, &passKey);
  if (pass != nullptr) {
    pass.currentPipeline = state.label != nil ? state.label : @"unnamed";
    [pass.pipelines addObject:pass.currentPipeline];
  }
  reinterpret_cast<void (*)(id, SEL, id<MTLComputePipelineState>)>(computePipelineOriginal)(
      self, sel, state);
}

std::optional<uint64_t> Product(MTLSize size) {
  uint64_t count = 1;
  for (const auto dimension : {size.width, size.height, size.depth}) {
    if (dimension == 0 || dimension > std::numeric_limits<uint64_t>::max() / count) {
      return std::nullopt;
    }
    count *= dimension;
  }
  return count;
}

struct DispatchDimensions {
  MTLSize Groups, Threads;
};

void RecordDispatch(OutshineProfilePass *pass, DispatchDimensions shape) {
  if (pass == nullptr) { return; }
  ++pass.calls;
  const auto groupCount = Product(shape.Groups);
  const auto localCount = Product(shape.Threads);
  constexpr auto maximum = std::numeric_limits<uint64_t>::max();
  if (!groupCount || !localCount || *localCount > maximum / *groupCount ||
      pass.groups > maximum - *groupCount) {
    ++pass.unavailableDispatches;
    return;
  }
  const uint64_t declared = *groupCount * *localCount;
  if (pass.declaredThreads > maximum - declared) {
    ++pass.unavailableDispatches;
    return;
  }
  pass.groups += *groupCount;
  pass.declaredThreads += declared;
  pass.work[pass.currentPipeline] =
      @(pass.work[pass.currentPipeline].unsignedLongLongValue + declared);
}

id<MTLComputeCommandEncoder> Compute(id self, SEL sel) {
  OutshineProfileCommand *const trace = Trace(self);
  if (trace.samples == nil || trace.passes.count >= 64) {
    ++trace.omittedPasses;
    return reinterpret_cast<id<MTLComputeCommandEncoder> (*)(id, SEL)>(computeOriginal)(self, sel);
  }
  OutshineProfilePass *const pass = [OutshineProfilePass new];
  pass.compute = YES;
  pass.sampleOffset = trace.sampleCount;
  trace.sampleCount += 2;
  [trace.passes addObject:pass];
  const auto descriptor = [MTLComputePassDescriptor new];
  const auto samples = descriptor.sampleBufferAttachments[0];
  samples.sampleBuffer = trace.samples;
  samples.startOfEncoderSampleIndex = pass.sampleOffset;
  samples.endOfEncoderSampleIndex = pass.sampleOffset + 1;
  const auto encoder = [(id<MTLCommandBuffer>)self computeCommandEncoderWithDescriptor:descriptor];
  if (encoder != nil && !computeEncoderPatched) {
    const Class cls = object_getClass(encoder);
    computePipelineOriginal = Patch(cls, @selector(setComputePipelineState:), (IMP)ComputePipeline);
    dispatchOriginal = Patch(cls, @selector(dispatchThreadgroups:threadsPerThreadgroup:));
    computeEncoderPatched = true;
  }
  if (encoder != nil) {
    objc_setAssociatedObject(encoder, &passKey, pass, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return encoder;
}

void DecodeIndirect(OutshineProfileCommand *trace) {
  for (OutshineProfileIndirect *call in trace.indirect) {
    if (call.buffer.readback == nil) {
      ++call.pass.unavailableIndirect;
      continue;
    }
    IndirectArguments arguments;
    std::memcpy(&arguments,
                static_cast<const char *>(call.buffer.readback.contents) + call.offset,
                sizeof(arguments));
    const auto indices = uint64_t(arguments.count) * arguments.instances;
    call.pass.indirectIndices += indices;
    call.pass.work[call.pipeline] =
        @(call.pass.work[call.pipeline].unsignedLongLongValue + indices);
  }
}

void PrintPass(OutshineProfileCommand *trace, NSData *data, NSUInteger at) {
  OutshineProfilePass *const pass = trace.passes[at];
  std::array<MTLCounterResultTimestamp, 4> timestamps{};
  if (data.length >= (pass.sampleOffset + timestamps.size()) * sizeof(MTLCounterResultTimestamp)) {
    std::memcpy(timestamps.data(),
                static_cast<const char *>(data.bytes) +
                    pass.sampleOffset * sizeof(MTLCounterResultTimestamp),
                sizeof(timestamps));
  }
  bool valid = true;
  for (const auto &sample : timestamps) {
    valid = valid && sample.timestamp > 0 && sample.timestamp != MTLCounterErrorValue;
  }
  valid = valid && timestamps[1].timestamp >= timestamps[0].timestamp &&
          timestamps[3].timestamp >= timestamps[2].timestamp;
  NSString *const labels = [[[pass.pipelines allObjects]
      sortedArrayUsingSelector:@selector(compare:)] componentsJoinedByString:@","];
  std::println(
      stderr,
      "METAL_PASS serial={} pass={} valid={:d} vertex_ns={} fragment_ns={} calls={} "
      "indirect_calls={} direct_indices={} indirect_indices={} unavailable_indirect={} "
      "vertex_start_ns={} vertex_end_ns={} fragment_start_ns={} fragment_end_ns={} pipeline={}",
      trace.serial,
      at,
      static_cast<int>(valid),
      valid ? timestamps[1].timestamp - timestamps[0].timestamp : 0,
      valid ? timestamps[3].timestamp - timestamps[2].timestamp : 0,
      pass.calls,
      pass.indirectCalls,
      pass.directIndices,
      pass.indirectIndices,
      pass.unavailableIndirect,
      valid ? timestamps[0].timestamp : 0,
      valid ? timestamps[1].timestamp : 0,
      valid ? timestamps[2].timestamp : 0,
      valid ? timestamps[3].timestamp : 0,
      labels.UTF8String);
  for (NSString *key in pass.work) {
    std::println(stderr,
                 "METAL_WORK serial={} pass={} indices={} pipeline={}",
                 trace.serial,
                 at,
                 pass.work[key].unsignedLongLongValue,
                 key.UTF8String);
  }
}

void PrintCompute(OutshineProfileCommand *trace, NSData *data, NSUInteger at) {
  OutshineProfilePass *const pass = trace.passes[at];
  std::array<MTLCounterResultTimestamp, 2> times{};
  if (data.length >= (pass.sampleOffset + times.size()) * sizeof(MTLCounterResultTimestamp)) {
    std::memcpy(times.data(),
                static_cast<const char *>(data.bytes) +
                    pass.sampleOffset * sizeof(MTLCounterResultTimestamp),
                sizeof(times));
  }
  const bool valid = times[0].timestamp > 0 && times[0].timestamp != MTLCounterErrorValue &&
                     times[1].timestamp != MTLCounterErrorValue &&
                     times[1].timestamp >= times[0].timestamp;
  NSString *const labels = [[[pass.pipelines allObjects]
      sortedArrayUsingSelector:@selector(compare:)] componentsJoinedByString:@","];
  std::println(stderr,
               "METAL_COMPUTE serial={} pass={} valid={:d} duration_ns={} start_ns={} end_ns={} "
               "dispatches={} groups={} declared_threads={} unavailable_dispatches={} pipeline={}",
               trace.serial,
               at,
               static_cast<int>(valid),
               valid ? times[1].timestamp - times[0].timestamp : 0,
               valid ? times[0].timestamp : 0,
               valid ? times[1].timestamp : 0,
               pass.calls,
               pass.groups,
               pass.declaredThreads,
               pass.unavailableDispatches,
               labels.UTF8String);
  for (NSString *key in pass.work) {
    std::println(stderr,
                 "METAL_COMPUTE_WORK serial={} pass={} declared_threads={} pipeline={}",
                 trace.serial,
                 at,
                 pass.work[key].unsignedLongLongValue,
                 key.UTF8String);
  }
}

void Complete(OutshineProfileCommand *trace) {
  if (trace.omittedPasses > 0) {
    std::println(
        stderr, "METAL_TRUNCATED serial={} omitted_passes={}", trace.serial, trace.omittedPasses);
  }
  DecodeIndirect(trace);
  const auto data = [trace.samples resolveCounterRange:NSMakeRange(0, trace.sampleCount)];
  for (NSUInteger at = 0; at < trace.passes.count; ++at) {
    if (trace.passes[at].compute) {
      PrintCompute(trace, data, at);
    } else {
      PrintPass(trace, data, at);
    }
  }
}

void CopyArguments(id<MTLCommandBuffer> commands, OutshineProfileCommand *trace) {
  if (trace.buffers.count == 0) { return; }
  const auto copy = [commands blitCommandEncoder];
  for (NSValue *key in trace.buffers) {
    OutshineProfileBuffer *const buffer = trace.buffers[key];
    buffer.readback = [commands.device newBufferWithLength:buffer.bytes
                                                   options:MTLResourceStorageModeShared];
    if (buffer.readback != nil) {
      [copy copyFromBuffer:buffer.source
               sourceOffset:0
                   toBuffer:buffer.readback
          destinationOffset:0
                       size:buffer.bytes];
    }
  }
  [copy endEncoding];
}

void Commit(id self, SEL sel) {
  OutshineProfileCommand *const trace = objc_getAssociatedObject(self, &traceKey);
  const id<MTLCommandBuffer> commands = self;
  if (trace != nil) {
    CopyArguments(commands, trace);
    [commands addCompletedHandler:^(id<MTLCommandBuffer> done) {
      @autoreleasepool {
        constexpr double nanosecondsPerSecond = 1'000'000'000.0;
        const double start = done.GPUStartTime;
        const double end = done.GPUEndTime;
        const double elapsed = (end - start) * nanosecondsPerSecond;
        const bool valid = std::isfinite(start) && start > 0 && std::isfinite(end) &&
                           std::isfinite(elapsed) && elapsed >= 0;
        std::println(stderr,
                     "METAL_COMMAND serial={} valid={:d} gpu_ns={:.0f}",
                     trace.serial,
                     static_cast<int>(valid),
                     valid ? elapsed : 0);
        Complete(trace);
      }
    }];
  }
  reinterpret_cast<void (*)(id, SEL)>(commitOriginal)(self, sel);
}

__attribute__((constructor)) void Install() {
  @autoreleasepool {
    const auto device = MTLCreateSystemDefaultDevice();
    for (id<MTLCounterSet> set in device.counterSets) {
      if ([set.name isEqualToString:MTLCommonCounterSetTimestamp]) { counterSet = set; }
    }
    if ((counterSet == nullptr) ||
        ![device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary]) {
      return;
    }
    const auto queue = [device newCommandQueue];
    const auto commands = [queue commandBuffer];
    const Class cls = object_getClass(commands);
    renderOriginal = Patch(cls, @selector(renderCommandEncoderWithDescriptor:), (IMP)Render);
    commitOriginal = Patch(cls, @selector(commit), (IMP)Commit);
    computeOriginal = Patch(cls, @selector(computeCommandEncoder), (IMP)Compute);
    std::println(stderr, "METAL_PASS installed");
  }
}
}

@implementation OutshineProfileHooks

- (void)dispatchThreadgroups:(MTLSize)groups threadsPerThreadgroup:(MTLSize)threads {
  RecordDispatch(objc_getAssociatedObject(self, &passKey), {.Groups = groups, .Threads = threads});
  reinterpret_cast<void (*)(id, SEL, MTLSize, MTLSize)>(dispatchOriginal)(
      self, _cmd, groups, threads);
}

- (void)drawIndexedPrimitives:(MTLPrimitiveType)type
                   indexCount:(NSUInteger)count
                    indexType:(MTLIndexType)indexType
                  indexBuffer:(id<MTLBuffer>)buffer
            indexBufferOffset:(NSUInteger)offset
                instanceCount:(NSUInteger)instances
                   baseVertex:(NSInteger)base
                 baseInstance:(NSUInteger)first {
  OutshineProfilePass *const pass = objc_getAssociatedObject(self, &passKey);
  if (pass != nullptr) {
    ++pass.calls;
    pass.directIndices += uint64_t(count) * instances;
    pass.work[pass.currentPipeline] =
        @(pass.work[pass.currentPipeline].unsignedLongLongValue + uint64_t(count) * instances);
  }
  reinterpret_cast<void (*)(id,
                            SEL,
                            MTLPrimitiveType,
                            NSUInteger,
                            MTLIndexType,
                            id<MTLBuffer>,
                            NSUInteger,
                            NSUInteger,
                            NSInteger,
                            NSUInteger)>(indexedOriginal)(
      self, _cmd, type, count, indexType, buffer, offset, instances, base, first);
}

- (void)drawPrimitives:(MTLPrimitiveType)type
           vertexStart:(NSUInteger)first
           vertexCount:(NSUInteger)count
         instanceCount:(NSUInteger)instances
          baseInstance:(NSUInteger)firstInstance {
  OutshineProfilePass *const pass = objc_getAssociatedObject(self, &passKey);
  if (pass != nullptr) {
    ++pass.calls;
    pass.directIndices += uint64_t(count) * instances;
    pass.work[pass.currentPipeline] =
        @(pass.work[pass.currentPipeline].unsignedLongLongValue + uint64_t(count) * instances);
  }
  reinterpret_cast<void (*)(
      id, SEL, MTLPrimitiveType, NSUInteger, NSUInteger, NSUInteger, NSUInteger)>(drawOriginal)(
      self, _cmd, type, first, count, instances, firstInstance);
}

@end
