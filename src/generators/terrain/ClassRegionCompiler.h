#ifndef OUTSHINE_GENERATORS_TERRAIN_CLASSREGIONCOMPILER_H
#define OUTSHINE_GENERATORS_TERRAIN_CLASSREGIONCOMPILER_H

#include "ClassificationRasterizer.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <vector>

namespace outshine::Generators {

struct CompiledClassRegions {
  ClassificationRasterizer::Input Raster;
  std::vector<std::vector<uint32_t>> Contributors;
  size_t CoalescedGroups = 0;
  size_t RetainedGroups = 0;
};

[[nodiscard]] std::optional<CompiledClassRegions>
CompileClassRegions(const ClassificationRasterizer::Input &source,
                    const std::stop_token &stop = {});

}
#endif
