#ifndef OUTSHINE_GENERATORS_BUILDING_HIPROOFCODEC_H
#define OUTSHINE_GENERATORS_BUILDING_HIPROOFCODEC_H

#include "HipRoofPlan.h"
#include "StructureBinary.h"
#include <expected>
#include <memory>

namespace outshine::Generators {

class HipRoofCodec {
public:
  using Product = std::shared_ptr<const std::expected<HipRoofPlan, HipRoofError>>;
  [[nodiscard]] static bool Fields(StructureBinary::Writer &archive, const Product &value);
  [[nodiscard]] static bool Fields(StructureBinary::Reader &archive, Product &value);
  [[nodiscard]] static bool Valid(const Product &value);
  [[nodiscard]] static bool BoundTo(const Product &value,
                                    std::span<const EastNorth> exterior,
                                    std::span<const std::vector<EastNorth>> holes);
};

}
#endif
