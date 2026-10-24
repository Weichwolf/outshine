#ifndef OUTSHINE_BASE_SCENE_HEIGHTRASTER_H
#define OUTSHINE_BASE_SCENE_HEIGHTRASTER_H

#include <cstdint>
#include <vector>

namespace outshine {

struct HeightRaster {
  uint32_t Rows = 0;
  uint32_t Cols = 0;
  std::vector<float> Meters;
};

}
#endif
