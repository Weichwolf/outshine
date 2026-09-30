#ifndef OUTSHINE_WORLD_DATA_OSMAPIREADER_H
#define OUTSHINE_WORLD_DATA_OSMAPIREADER_H

#include "ContentStore.h"
#include "OsmChunkSetLoader.h"
#include "Transport.h"

namespace outshine::Data {

[[nodiscard]] std::expected<OsmSourceChunk, std::string>
ReadOsmApiRegion(const SourceProvider &provider,
                 ContentStore &store,
                 Transport &wire,
                 double deadlineMs,
                 const std::stop_token &stop);

}
#endif
