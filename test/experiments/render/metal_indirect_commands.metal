#include <metal_stdlib>
using namespace metal;

struct DrawArguments {
  uint count, instances, first;
  int base;
  uint model;
};

struct Commands {
  command_buffer commands [[id(0)]];
};

struct Frame {
  float4x4 clip;
  float4 shift;
};

vertex float4 positionOnly(uint vertexId [[vertex_id]],
                           uint instanceId [[instance_id]],
                           const device packed_float3 *positions [[buffer(0)]],
                           const device float4x4 *models [[buffer(1)]],
                           constant Frame &frame [[buffer(2)]]) {
  float3 world =
      (models[instanceId] * float4(float3(positions[vertexId]), 1)).xyz + frame.shift.xyz;
  return frame.clip * float4(world, 1);
}

kernel void prepareCommands(const device DrawArguments *args [[buffer(0)]],
                            const device uint *indices [[buffer(1)]],
                            const device Commands &icb [[buffer(2)]],
                            constant uint &count [[buffer(3)]],
                            uint at [[thread_position_in_grid]]) {
  if (at >= count) { return; }
  render_command command(icb.commands, at);
  command.reset();
  DrawArguments draw = args[at];
  if (draw.count > 0 && draw.instances > 0) {
    command.draw_indexed_primitives(primitive_type::triangle,
                                    draw.count,
                                    indices + draw.first,
                                    draw.instances,
                                    draw.base,
                                    draw.model);
  }
}

kernel void clearCounts(device DrawArguments *args [[buffer(0)]],
                        constant uint &count [[buffer(1)]],
                        constant uint &mask [[buffer(2)]],
                        uint at [[thread_position_in_grid]]) {
  if (at < count && (mask == 0 || (at & mask) == 0)) { args[at].count = 0; }
}

kernel void commandPrefix(const device DrawArguments *args [[buffer(0)]],
                          device uint *slots [[buffer(1)]],
                          device uint2 &range [[buffer(2)]],
                          constant uint &count [[buffer(3)]],
                          uint lane [[thread_index_in_threadgroup]],
                          uint simdLane [[thread_index_in_simdgroup]],
                          uint wave [[simdgroup_index_in_threadgroup]],
                          uint waveWidth [[threads_per_simdgroup]]) {
  threadgroup uint totals[32];
  threadgroup uint carry, chunk;
  if (lane == 0) { carry = 0; }
  threadgroup_barrier(mem_flags::mem_threadgroup);
  for (uint from = 0; from < count; from += 256) {
    uint at = from + lane;
    uint mine = at < count && args[at].count > 0 && args[at].instances > 0 ? 1u : 0u;
    uint prefix = simd_prefix_exclusive_sum(mine);
    uint total = simd_sum(mine);
    if (simdLane == 0) { totals[wave] = total; }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (lane == 0) {
      chunk = carry;
      for (uint group = 0; group < 256 / waveWidth; ++group) {
        uint taken = totals[group];
        totals[group] = carry - chunk;
        carry += taken;
      }
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (at < count) { slots[at] = chunk + totals[wave] + prefix; }
    threadgroup_barrier(mem_flags::mem_threadgroup);
  }
  if (lane == 0) { range = uint2(0, carry); }
}

kernel void prepareDenseCommands(const device DrawArguments *args [[buffer(0)]],
                                 const device uint *indices [[buffer(1)]],
                                 const device Commands &icb [[buffer(2)]],
                                 constant uint &count [[buffer(3)]],
                                 const device uint *slots [[buffer(4)]],
                                 uint at [[thread_position_in_grid]]) {
  if (at >= count) { return; }
  DrawArguments draw = args[at];
  if (draw.count == 0 || draw.instances == 0) { return; }
  render_command command(icb.commands, slots[at]);
  command.reset();
  command.draw_indexed_primitives(primitive_type::triangle,
                                  draw.count,
                                  indices + draw.first,
                                  draw.instances,
                                  draw.base,
                                  draw.model);
}
