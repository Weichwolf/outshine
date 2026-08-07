/* THE SCENE PASS HAS TWO ATTACHMENTS and that is a contract, like the pass topology above it: linear
 * radiance, and the screen-space MOTION of whatever wrote the depth. Every pipeline recorded into
 * that pass must declare both targets whatever it actually writes, so the second one is described
 * here once instead of in twelve stages.
 *
 * The clear value IS a statement: "nothing dynamic wrote this pixel". TaaStage then reconstructs the
 * motion from depth through the previous view-projection, which is EXACT for anything world-fixed and
 * costs no attachment write at all. An opaque stage therefore writes the sentinel rather than nothing
 * — otherwise a building drawn over a blade would inherit the blade's velocity. */
#ifndef SCENETARGETS_H
#define SCENETARGETS_H

#include <webgpu/webgpu_cpp.h>

namespace outshine::Render {

inline constexpr wgpu::TextureFormat kVelocityFormat = wgpu::TextureFormat::RG16Float;
/* NDC motion is bounded by 2 in each axis; -1e4 is unreachable and exactly representable in f16. */
inline constexpr float kVelocityStatic = -1.0e4f;

inline wgpu::ColorTargetState VelocityTarget(bool writes) {
  wgpu::ColorTargetState ct{};
  ct.format = kVelocityFormat;
  ct.writeMask = writes ? wgpu::ColorWriteMask::All : wgpu::ColorWriteMask::None;
  return ct;
}

static const char *kVelocityWGSL = R"(
const kVelStatic = -1.0e4;
/* Both clip positions carry their own frame's jitter, so their difference does too; the resolve
 * subtracts the jitter difference ONCE, for this path and the depth path alike. */
fn velFromClip(cur : vec4f, prv : vec4f) -> vec2f {
  return cur.xy / max(cur.w, 1.0e-6) - prv.xy / max(prv.w, 1.0e-6);
}
)";

} // namespace outshine::Render
#endif
