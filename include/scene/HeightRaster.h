#ifndef OUTSHINE_SCENE_HEIGHTRASTER_H
#define OUTSHINE_SCENE_HEIGHTRASTER_H

#include <cstdint>
#include <vector>

namespace outshine {

/// Owned native height samples in metres, row-major, with no encoded image data.
/// The consuming geometry supplies the spatial transform and vertical reference.
/// A surface requires at least two rows/columns, exactly Rows*Cols finite samples.
struct HeightRaster {
  uint32_t Rows = 0;         ///< Number of sample rows.
  uint32_t Cols = 0;         ///< Number of sample columns.
  std::vector<float> Meters; ///< Owned row-major samples; transfer without copying.
};

}
#endif
