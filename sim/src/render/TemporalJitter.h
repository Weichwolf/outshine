/* THE SUB-PIXEL SAMPLE POSITION of a frame, as a property of the CAMERA and of nothing else.
 *
 * A blade eleven millimetres wide at eight metres covers 0.9 px. A rasteriser asks ONE question per
 * pixel — is the centre inside? — so that blade is either fully there or fully gone, and along its
 * length the answer alternates: the gestrichelte Linie the art director measured. No post filter can
 * repair it, because the coverage was never in the image. Moving the sample point by a fraction of a
 * pixel each frame and averaging over frames IS the missing coverage, recovered statistically.
 *
 * Halton(2,3) because it is the low-discrepancy sequence whose 2-D projection stays stratified at
 * every prefix length: any 8 consecutive phases already cover the pixel evenly, so the accumulation
 * is unbiased before the cycle closes. */
#ifndef TEMPORALJITTER_H
#define TEMPORALJITTER_H

namespace outshine::Render {

class TemporalJitter {
public:
  /* [SET] 8 phases. The resolve's own feedback (TaaStage kFeedback = 0.1) has an e-fold of 9.5
   * frames, so a longer cycle would sample positions the accumulator has already forgotten. */
  static constexpr int kPhases = 8;

  TemporalJitter(void) { Reset(); }

  /* THE SEQUENCE IS PART OF THE HISTORY. Emptying the accumulator without restarting the phase makes
   * the settled picture a function of how many frames ran before it: the same eight positions are
   * visited in a rotated order and the resolve's feedback weights them unequally. */
  void Reset(void) {
    if (Pinned) return;
    Phase = 0;
    CurX = Halton(1, 2) - 0.5f;
    CurY = Halton(1, 3) - 0.5f;
    PrevX = CurX;
    PrevY = CurY;
  }

  void Advance(void) {
    if (Pinned) return;
    PrevX = CurX;
    PrevY = CurY;
    Phase = (Phase + 1) % kPhases;
    CurX = Halton(Phase + 1, 2) - 0.5f;
    CurY = Halton(Phase + 1, 3) - 0.5f;
  }

  void Disarm(void) { CurX = CurY = PrevX = PrevY = 0.0f; }

  /* A FROZEN phase, for the measurement that asks whether the offset moved anything world-fixed. It
   * LATCHES: a pin the caller has to re-apply every frame is a pin the next caller forgets. */
  void Pin(float x, float y) {
    Pinned = true;
    PrevX = CurX = x;
    PrevY = CurY = y;
  }

  float PixelX(void) const { return CurX; }
  float PixelY(void) const { return CurY; }
  float PrevPixelX(void) const { return PrevX; }
  float PrevPixelY(void) const { return PrevY; }

private:
  static float Halton(int i, int base) {
    float f = 1.0f, r = 0.0f;
    while (i > 0) {
      f /= (float)base;
      r += f * (float)(i % base);
      i /= base;
    }
    return r;
  }

  int Phase = 0;
  bool Pinned = false;
  float CurX = 0.0f, CurY = 0.0f, PrevX = 0.0f, PrevY = 0.0f;
};

} // namespace outshine::Render
#endif
