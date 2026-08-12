/* WHAT ORDER DRAWS GO IN, as one integer per draw. Ericson's layout, most significant field first:
 * viewport, viewport layer, translucency type, depth, material (*Order your graphics draw calls
 * around!*, realtimecollisiondetection.net, 2008). Sorting the list by this integer puts every
 * correctness constraint above every performance one -- a blended surface cannot sort ahead of the
 * opaque surface behind it, whatever material either wears -- and leaves the material field free to
 * group draws that may then be batched.
 *
 * IT IS A PER-DRAW QUANTITY AND NOT A PER-STAGE ONE. A stage row declares a pass: what it reads,
 * what it writes, what it contributes to. One pass carries many draws with different materials,
 * different transforms and different vertex layouts, so a key on the stage row could only ever say
 * one thing for all of them.
 *
 * THE TRANSLUCENCY FIELD IS `SurfaceKind` ITSELF and not a second taxonomy beside it: the
 * enumeration is already ordered so that opaque comes before masked comes before the blended kinds,
 * which is the order they must be drawn in. THE DEPTH FIELD IS COMPLEMENTED FOR A BLENDING SURFACE,
 * so one ascending sort delivers front-to-back for opaque and back-to-front for blended -- the two
 * orders each kind needs, out of one comparison. */
#ifndef DRAWKEY_H
#define DRAWKEY_H

#include <cstdint>

#include "SurfaceState.h"

namespace outshine::Render {

/* WHICH LAYER OF A VIEW a draw belongs to, above every depth question: a sky drawn behind everything
 * and an overlay drawn in front of everything are not decided by their depth, and giving them a
 * depth that happens to sort right is the thing this field exists to stop. */
enum class ViewLayer : uint8_t { Background, World, Overlay };

/* HOW MANY VIEWS, LAYERS AND MATERIALS A KEY CAN ADDRESS. The widths sum to 64 and are stated here
 * so that a field's capacity is a number a caller can check against rather than a bit count to
 * count. `kViewportBits` covers a shadow cascade, a reflection and a second camera in the same
 * field, which is the reason the view index is here and not in the catalogue's stage row. */
inline constexpr int kViewportBits = 8;
inline constexpr int kViewLayerBits = 4;
inline constexpr int kSurfaceKindBits = 4;
inline constexpr int kDepthBits = 24;
inline constexpr int kMaterialBits = 24;
static_assert(kViewportBits + kViewLayerBits + kSurfaceKindBits + kDepthBits + kMaterialBits == 64,
              "the five fields are the whole key");

inline constexpr uint32_t kViewportSlots = 1u << kViewportBits;
inline constexpr uint32_t kMaterialSlots = 1u << kMaterialBits;
inline constexpr uint32_t kDepthSteps = (1u << kDepthBits) - 1u;

/* WHAT DECIDES ONE DRAW'S PLACE IN THE LIST, as a parameter object rather than five arguments
 * (`I.23`) -- and every one of the five is meaningless without its neighbours, so they travel
 * together. `DepthFraction` is 0 at the near plane and 1 at the far plane along the view axis; the
 * caller derives it because only the caller knows the camera. */
struct DrawOrder {
  uint32_t Viewport = 0;
  ViewLayer Layer = ViewLayer::World;
  SurfaceState Surface = StateOf(Material{});
  double DepthFraction = 0.0;
  uint32_t MaterialSlot = 0;
};

/* Two draws with the same key may be drawn in either order without changing the picture, which is
 * exactly the licence a batcher needs. */
class DrawKey {
public:
  [[nodiscard]] static constexpr DrawKey Of(const DrawOrder &order) {
    const double clamped =
        order.DepthFraction < 0.0 ? 0.0 : (order.DepthFraction > 1.0 ? 1.0 : order.DepthFraction);
    /* +0.5 and a truncation rather than a library rounding call, because the whole key must be a
     * constant expression and `std::llround` is not one. */
    const uint32_t step =
        static_cast<uint32_t>(clamped * static_cast<double>(kDepthSteps) + 0.5);
    const uint32_t depth = order.Surface.Blends() ? kDepthSteps - step : step;
    uint64_t bits = order.Viewport & (kViewportSlots - 1u);
    bits = (bits << kViewLayerBits) | static_cast<uint64_t>(order.Layer);
    bits = (bits << kSurfaceKindBits) | static_cast<uint64_t>(order.Surface.Kind());
    bits = (bits << kDepthBits) | depth;
    bits = (bits << kMaterialBits) | (order.MaterialSlot & (kMaterialSlots - 1u));
    return DrawKey(bits);
  }

  [[nodiscard]] constexpr uint64_t Bits() const { return Bits_; }
  [[nodiscard]] constexpr bool operator<(const DrawKey &other) const { return Bits_ < other.Bits_; }
  [[nodiscard]] constexpr bool operator==(const DrawKey &other) const {
    return Bits_ == other.Bits_;
  }

private:
  explicit constexpr DrawKey(uint64_t bits) : Bits_(bits) {}

  uint64_t Bits_ = 0;
};

} // namespace outshine::Render
#endif
