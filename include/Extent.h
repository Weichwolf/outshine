#ifndef OUTSHINE_EXTENT_H
#define OUTSHINE_EXTENT_H

namespace outshine {

/// A picture's size in PIXELS.
///
/// It stands at the door rather than in the declaration schema because both sides use it: a
/// scenario declares the frame it wants, and the client hands the same value to
/// @ref Engine::drawsInto and gets it back from @ref SwapChain::extent. A type only one side reads
/// would have to be converted at the boundary for no reason.
struct Extent {
  /// The width in pixels.
  int WidthPx = 0;
  /// The height in pixels.
  int HeightPx = 0;

  /// Two sizes are the same size when both their measures are.
  [[nodiscard]] constexpr bool operator==(const Extent &) const = default;
};

} // namespace outshine

#endif
