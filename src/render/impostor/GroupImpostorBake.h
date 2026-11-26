#ifndef OUTSHINE_RENDER_IMPOSTOR_GROUPIMPOSTORBAKE_H
#define OUTSHINE_RENDER_IMPOSTOR_GROUPIMPOSTORBAKE_H

#include "GroupImpostorView.h"
#include "ImpostorAtlas.h"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace outshine::Render {

[[nodiscard]] std::optional<GroupImpostorView>
BakeGroupImpostorView(const ImpostorAtlas &atlas, size_t view, uint32_t side);

}
#endif
