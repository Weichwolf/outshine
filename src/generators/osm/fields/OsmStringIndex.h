#ifndef OUTSHINE_GENERATORS_OSM_FIELDS_OSMSTRINGINDEX_H
#define OUTSHINE_GENERATORS_OSM_FIELDS_OSMSTRINGINDEX_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators::Osm {
// Pool IDs survive vector/SSO relocation; the index owns no string copies or borrowed views.
class OsmStringIndex {
public:
  [[nodiscard]] uint32_t Intern(std::vector<std::string> &pool, std::string_view text);
  [[nodiscard]] size_t CapacityBytes() const noexcept;
  // Maximum old/new slot bytes when a fresh or cleared index grows to this pool size.
  [[nodiscard]] static size_t PeakBytes(size_t entries) noexcept;
  void Clear() noexcept;

private:
  [[nodiscard]] size_t Position(const std::vector<std::string> &pool, std::string_view text) const;
  void Rebuild(const std::vector<std::string> &pool, size_t capacity);
  std::vector<uint32_t> Slots_;
  size_t Entries_ = 0;
};
}
#endif
