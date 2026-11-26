#ifndef OUTSHINE_RENDER_IMPOSTOR_GROUPIMPOSTORCOMPOSER_H
#define OUTSHINE_RENDER_IMPOSTOR_GROUPIMPOSTORCOMPOSER_H

#include "ImpostorAtlas.h"
#include "Vec3.h"
#include <optional>
#include <span>
#include <string>

namespace outshine::Render {

struct GroupImpostorInstance {
  Vec3 RootM;
  double Scale = 1;
};

struct GroupImpostorLayout {
  Vec3 CentreM;
  double HalfExtentM;
  int Pixels;
  std::span<const GroupImpostorInstance> Instances;
};

[[nodiscard]] std::optional<ImpostorAtlas> ComposeGroupImpostor(const ImpostorAtlas &prototype,
                                                                const GroupImpostorLayout &layout,
                                                                std::string &error);

}
#endif
