/* WHICH BOUND A PIXEL IS GATED BY, IN ONE PLACE (board:1144). The picture bound routes every pixel
 * to exactly one of two instruments, and until this existed the rule lived inside the comparison
 * loop as an expression over two masks -- so the geometric bound's own population was written a
 * second time, in another file, over the same two masks, and the two could not be kept in step by
 * anything but attention.
 *
 * AGREEMENT ABOUT COVERAGE IS NOT AGREEMENT ABOUT WHAT IS THERE, and that is the whole of why the
 * router grew a third mask. Two sides can both cover a pixel and draw a DIFFERENT SURFACE at it;
 * scored perceptually, that difference is read as a colour disagreement between two materials
 * nobody claimed were the same one -- [MEASURED] at `coverage/negative-scale` (717, 274), where the
 * oracle carries `LabelMat` at its declared 0.5 grey and we carry `BackgroundMaterial` at its
 * declared blue, and the three channels of that ONE pixel were the whole of the case's perceptual
 * tail at 103.269, 70.097 and 21.834 codes. Which surface covers a pixel centre is a rasterisation
 * question, so it is scored where rasterisation questions are scored.
 *
 * THE THIRD MASK IS THE ATTRIBUTABLE POPULATION AND NEVER THE DISAGREEING ONE. `board:1155`: the
 * oracle's index passes name the first camera-ray hit, which on the same case is a DIFFERENT surface
 * from the one that coloured the pixel at 189 of its 190 disagreements. Routing those 189 would be a
 * pixel moved between two metrics for a reason that is about Cycles' pass semantics, and it would
 * read as a repair. What arrives here has already survived that predicate; this file only spends it.
 *
 * A ROUTED PIXEL IS NEVER AN EXCLUDED PIXEL. It leaves the tail and it enters `worst_disagreement_px`,
 * which is stricter -- the disagreement must sit inside the oracle's own filter half-width. A router
 * whose second bin were unbounded would be a mask wearing the word `routed`. */
#ifndef RENDER_ROUTING_H
#define RENDER_ROUTING_H

#include "Mask.h"

namespace outshine::Render::Parity {

/* THE THREE MASKS THAT DECIDE, as one parameter object rather than three arguments spread over two
 * call sites (`I.23`). It holds references and is therefore not assignable (`C.12`), which is what
 * it should be: it is the question asked of one frame and it outlives nothing. */
struct Routing {
  const Mask &Ours;
  const Mask &Theirs;
  /* Pixels both sides cover and name a different surface at, with the disagreement attributed to
   * this engine. Empty where the case has no identity reading at all, and an empty mask answers
   * every pixel `false` -- which is the same routing the coverage masks alone used to produce. */
  const Mask &SurfaceDisagreeing;

  /* The two sides agree about covering this pixel AND about what covers it, so its difference is a
   * difference of appearance. */
  [[nodiscard]] bool ToAppearance(int x, int y) const { return !ToGeometry(x, y); }

  /* One of the two agreements failed, so the difference is a difference of geometry and the codes it
   * arrives as are the instrument's amplification rather than the picture's. */
  [[nodiscard]] bool ToGeometry(int x, int y) const {
    return Ours.At(x, y) != Theirs.At(x, y) || SurfaceDisagreeing.At(x, y);
  }
};

} // namespace outshine::Render::Parity
#endif
