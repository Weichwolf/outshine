/* FlightBox — FBF16Max7456: the hook for MAX7456-CHIP-specific rendering quirks (interlace jitter,
 * brightness curve, sync artifacts). The generic font machinery in render/ is airframe-agnostic, so
 * nothing chip-specific belongs there; this is that override point instead, today a real NoOp. */
#ifndef FBF16MAX7456_H
#define FBF16MAX7456_H

namespace FlightBox {

class FBF16Max7456 {
public:
  virtual ~FBF16Max7456() = default;

  /* Position (px) and colour, in place, before a glyph quad is appended. */
  virtual void StyleGlyph(float &x, float &y, float &r, float &g, float &b) const;
};

} // namespace FlightBox
#endif
