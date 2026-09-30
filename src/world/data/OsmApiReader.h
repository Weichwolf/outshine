#ifndef OUTSHINE_WORLD_DATA_OSMAPIREADER_H
#define OUTSHINE_WORLD_DATA_OSMAPIREADER_H

#include "ContentStore.h"
#include "OsmChunkSetLoader.h"
#include <world/data/Transport.h>
#include <world/Provider.h>

namespace outshine::Data {

[[nodiscard]] std::expected<OsmSourceChunk, std::string>
ReadOsmApiRegion(const SourceProvider &provider,
                 ContentStore &store,
                 Transport &wire,
                 double deadlineMs,
                 const std::stop_token &stop,
                 const ProviderRegistry *registry = nullptr,
                 std::string_view shippedRoot = {});

}
#endif
