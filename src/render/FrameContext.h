/* The shared per-frame state every stage's Encode() reads: Renderer fills exactly one per frame,
 * before recording any pass. A stage never reaches back into Renderer for camera state. */
#ifndef FRAMECONTEXT_H
#define FRAMECONTEXT_H

namespace outshine::Render {

struct FrameContext {
  /* The eye in ECEF metres, in double because a float metre at the Earth's radius is a half-metre
   * quantum: everything a stage uploads is this subtracted from a place, and the subtraction has to
   * happen in the width the place is known in. */
  double Eye[3];
  float Mvp16[16];   /* camera-relative view-projection, column-major, eye at the render origin */
};

} // namespace outshine::Render
#endif
