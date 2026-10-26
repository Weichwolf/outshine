#ifndef OUTSHINE_WORLD_ENTITY_BODYSNAPSHOTDATA_H
#define OUTSHINE_WORLD_ENTITY_BODYSNAPSHOTDATA_H
#include "EntityBodyState.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace outshine::World {

struct BodySnapshotData {
  std::shared_ptr<const std::vector<uint32_t>> RowOfEntity;
  std::vector<std::shared_ptr<const std::vector<EntityBodyState>>> Pages;
  size_t Count = 0;
};
}
#endif
