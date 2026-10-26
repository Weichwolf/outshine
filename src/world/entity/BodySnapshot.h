#ifndef OUTSHINE_WORLD_ENTITY_BODYSNAPSHOT_H
#define OUTSHINE_WORLD_ENTITY_BODYSNAPSHOT_H
#include "EntityBodyState.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <limits>

namespace outshine::World {
inline constexpr size_t kBodiesPerSnapshotPage = std::numeric_limits<uint64_t>::digits;
struct BodySnapshotData;
class EntityState;

class BodySnapshot {
public:
  [[nodiscard]] size_t Count() const noexcept;
  [[nodiscard]] const EntityBodyState &At(size_t row) const noexcept;
  [[nodiscard]] const RigidBodyState *Body(Entity entity) const noexcept;

  [[nodiscard]] uint64_t Revision() const noexcept { return Revision_; }

private:
  std::shared_ptr<const BodySnapshotData> Data_;
  uint64_t Revision_ = 0;
  friend class EntityState;
};

struct BodyPublicationCost {
  size_t CopiedBytes = 0;
  size_t NewPages = 0;
  size_t DirectoryEntries = 0;
};
}
#endif
