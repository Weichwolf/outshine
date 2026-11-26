#ifndef OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_MAPTERHORNAVAILABILITY_H
#define OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_MAPTERHORNAVAILABILITY_H

#include "PmTilesDirectory.h"
#include <Source.h>
#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

namespace outshine::Generators::Terrain {

class MapterhornAvailability {
public:
  [[nodiscard]] Data::Source::AvailabilityCheck
  Check(Data::TileId tile, Data::RawSourceCache &store, Data::SourceTransport &transport);

private:
  struct RangeRead {
    Data::Ticket Ticket = Data::Ticket::None;
    const Data::SourceTransport *Transport = nullptr;
    bool Failed = false;

    void Cancel(Data::SourceTransport &transport) const;
  };

  struct Leaf {
    Data::ByteRange Range;
    RangeRead Read{};
    std::optional<PmTilesDirectory> Directory = std::nullopt;
  };

  struct Index {
    PmTilesDirectory::Layout Layout;
    PmTilesDirectory Root;
    Data::RangeResponse Origin;
  };

  struct Archive {
    Data::TileId Region;
    Data::SourceDecl Declaration;
    RangeRead Read{};
    std::optional<Index> Directory = std::nullopt;
    std::list<Leaf> Leaves;
  };

  struct RangeRecord {
    std::vector<uint8_t> Bytes;
    Data::RangeResponse Origin;
    bool FromStore = false;
  };

  struct RangeResult {
    Data::Source::AvailabilityCheck Check;
    std::optional<RangeRecord> Record = std::nullopt;
  };

  [[nodiscard]] Archive &Access(Data::TileId region, Data::SourceTransport &transport);
  [[nodiscard]] static Leaf &
  AccessLeaf(Archive &archive, Data::ByteRange range, Data::SourceTransport &transport);

  [[nodiscard]] static RangeResult PollRange(const Archive &archive,
                                             RangeRead &read,
                                             Data::ByteRange range,
                                             const Data::RangeResponse *binding,
                                             Data::RawSourceCache &store,
                                             Data::SourceTransport &transport);
  static void KeepRange(const Archive &archive,
                        const RangeRecord &record,
                        std::string_view tag,
                        Data::RawSourceCache &store);

  [[nodiscard]] static Data::Source::AvailabilityCheck
  LoadRoot(Archive &archive, Data::RawSourceCache &store, Data::SourceTransport &transport);

  [[nodiscard]] static Data::Source::AvailabilityCheck
  CheckLeaf(Archive &archive,
            const Index &index,
            const PmTilesDirectory::Entry &entry,
            uint64_t tileId,
            Data::RawSourceCache &store,
            Data::SourceTransport &transport);

  std::mutex Mutex_;
  std::list<Archive> Archives_;
};

}
#endif
