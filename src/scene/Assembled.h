#ifndef OUTSHINE_SCENE_ASSEMBLED_H
#define OUTSHINE_SCENE_ASSEMBLED_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Scene.h>

namespace outshine {

struct Assembled {
  std::vector<Entity> Bodies;
  Entity PlayerBody = kNoEntity;
  Entity PlayerMind = kNoEntity;
  Entity Nav = kNoEntity;
  Entity Assignment = kNoEntity;

  std::vector<std::pair<std::string, Entity>> Prefabs;
  std::vector<std::pair<std::string, Entity>> Instances;
  std::vector<std::string> TraitNames;
  std::vector<std::string> TagNames;

  [[nodiscard]] Entity PrefabNamed(std::string_view name) const {
    for (const auto &row : Prefabs) {
      if (row.first == name) { return row.second; }
    }
    return kNoEntity;
  }

  [[nodiscard]] Entity InstanceNamed(std::string_view id) const {
    for (const auto &row : Instances) {
      if (row.first == id) { return row.second; }
    }
    return kNoEntity;
  }

  [[nodiscard]] uint32_t TraitKey(std::string_view name) const {
    for (size_t at = 0; at < TraitNames.size(); ++at) {
      if (TraitNames[at] == name) { return (uint32_t)(at + 1); }
    }
    return 0;
  }
};

}

#endif
