/* FlightBox — FBF16Max7456: the F-16 module's hook for MAX7456-CHIP-specific rendering quirks (this
 * particular OSD chip's interlace jitter, brightness/contrast curve, edge-enhance, sync artifacts, ...).
 * The generic bitmap-font atlas/coverage-AA machinery is airframe-agnostic and lives in
 * render/FBHudFont.h + FBHudStage's text path -- nothing chip-specific belongs there. This class is
 * that override point instead, owned by FBF16Module (same pattern as FBAutopilot/FBFlightControl's one
 * virtual override point): today it is a real, instantiated NoOp (StyleGlyph is the identity), the
 * place a future chip-artifact model hangs without touching render/ or any other module. */
#ifndef FBF16MAX7456_H
#define FBF16MAX7456_H

namespace FlightBox {

class FBF16Max7456 {
public:
  virtual ~FBF16Max7456() = default;

  /* Per-glyph style hook: position (px) and colour, in place, before a glyph quad is appended. NoOp
   * today -- a chip-artifact model would perturb these (e.g. per-glyph jitter, brightness rolloff). */
  virtual void StyleGlyph(float &x, float &y, float &r, float &g, float &b) const;
};

} // namespace FlightBox
#endif
