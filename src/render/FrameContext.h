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
  /* THE SAME TWO AT THE PREVIOUS SUBMITTED FRAME (board:1169), which is what a screen-space motion
   * vector is measured against. On the first frame of a run they are this frame's, so the velocity
   * a stage writes is zero rather than the motion from an undefined pose.
   *
   * BOTH HALVES, OR THE MOTION IS ONLY HALF ANSWERED: a vertex moved and the camera moved are two
   * different displacements of the same pixel, and a stage handed only the previous positions would
   * report a still subject under a turning camera as static. */
  double PrevEye[3];
  float PrevMvp16[16];
};

} // namespace outshine::Render
#endif
