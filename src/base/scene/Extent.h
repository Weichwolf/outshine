#ifndef OUTSHINE_EXTENT_H
#define OUTSHINE_EXTENT_H

namespace outshine {

struct Extent {

  int WidthPx = 0;

  int HeightPx = 0;

  [[nodiscard]] constexpr bool operator==(const Extent &) const = default;
};

}

#endif
