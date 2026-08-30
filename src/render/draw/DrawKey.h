#ifndef OUTSHINE_RENDER_DRAW_DRAWKEY_H
#define OUTSHINE_RENDER_DRAW_DRAWKEY_H

#include <cstdint>

#include "SurfaceState.h"

namespace outshine::Render {

enum class ViewLayer : uint8_t { Background, World, Overlay };

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

struct DrawOrder {
  uint32_t Viewport = 0;
  ViewLayer Layer = ViewLayer::World;
  SurfaceState Surface = StateOf(Material{});
  double DepthFraction = 0.0;
  uint32_t MaterialSlot = 0;
};

class DrawKey {
public:
  [[nodiscard]] static constexpr DrawKey Of(const DrawOrder &order) {
    const double clamped =
        order.DepthFraction < 0.0 ? 0.0 : (order.DepthFraction > 1.0 ? 1.0 : order.DepthFraction);

    const uint32_t step = static_cast<uint32_t>(clamped * static_cast<double>(kDepthSteps) + 0.5);
    const uint32_t depth = order.Surface.Blends() ? kDepthSteps - step : step;
    uint64_t bits = order.Viewport & (kViewportSlots - 1u);
    bits = (bits << kViewLayerBits) | static_cast<uint64_t>(order.Layer);
    bits = (bits << kSurfaceKindBits) | static_cast<uint64_t>(order.Surface.Kind());
    const uint64_t material = order.MaterialSlot & (kMaterialSlots - 1u);
    if (order.Surface.Blends()) {
      bits = (bits << kDepthBits) | depth;
      bits = (bits << kMaterialBits) | material;
    } else {
      bits = (bits << kMaterialBits) | material;
      bits = (bits << kDepthBits) | depth;
    }
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
