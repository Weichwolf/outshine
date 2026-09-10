#ifndef OUTSHINE_EXTENT_H
#define OUTSHINE_EXTENT_H

namespace outshine {

/// Owned integer image dimensions in pixels; no implicit validation or allocation.
/// Zero-initialized dimensions describe no image. Consumers define their positive-size limits.
struct Extent {
  /// The width in pixels.
  int WidthPx = 0;
  /// The height in pixels.
  int HeightPx = 0;

  /// Two sizes are the same size when both their measures are.
  /// @return Exact width/height equality; this does not validate either extent.
  [[nodiscard]] constexpr bool operator==(const Extent &) const = default;
};

}

#endif
