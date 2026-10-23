#ifndef OUTSHINE_TEST_URBANPRISMCODEC_PRISMFIXTURE_H
#define OUTSHINE_TEST_URBANPRISMCODEC_PRISMFIXTURE_H

#include "UrbanPrismCodec.h"
#include <memory>
#include <vector>

namespace outshine::Test::UrbanPrismFixture {
inline Generators::Osm::UrbanPrisms Product() {
  Generators::Osm::UrbanPrisms result;
  result.Geometry.Samples = std::make_shared<const std::vector<PrismSpan>>(
      std::vector<PrismSpan>{{.Lower = -10, .Upper = -2},
                             {.Lower = 4, .Upper = 8},
                             {.Lower = 2, .Upper = 2},
                             {.Lower = 5, .Upper = 17}});
  result.Geometry.Patches = {
      {.Columns = 2, .Rows = 2, .PitchX = 4, .PitchZ = 7, .FillFraction = .5f},
      {.FirstSample = 2, .Columns = 2, .Rows = 1, .PitchX = 9, .PitchZ = 3}};
  result.Placements.resize(2);
  result.Placements[0].EcefFromLocal.At(0, 3) = 6378137;
  result.Placements[0].MaxCornerErrorM = .125;
  auto &second = result.Placements[1].EcefFromLocal;
  second.At(0, 0) = 0;
  second.At(2, 0) = 1;
  second.At(0, 2) = -1;
  second.At(2, 2) = 0;
  second.At(0, 3) = -4500000;
  second.At(1, 3) = 1200000;
  second.At(2, 3) = 4500000;
  result.Placements[1].MaxCornerErrorM = .25;
  return result;
}
}
#endif
