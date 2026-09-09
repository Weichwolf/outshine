#ifndef OUTSHINE_ENGINE_INPUTPUMP_H
#define OUTSHINE_ENGINE_INPUTPUMP_H

#include <span>
#include <cstddef>
#include <cstdint>

#include <SDL3/SDL_events.h>

#include "InputMap.h"

namespace outshine::Core {

class InputPump {
public:
  [[nodiscard]] static bool CatalogueReady();

  struct Fired {
    uint16_t Action = InputMap::kUnbound;
    InputMap::Kind What = InputMap::Kind::Button;
    float Value = 0.0f;
  };

  [[nodiscard]] static size_t
  Translate(const SDL_Event &event, const InputMap &bindings, std::span<Fired, 2> out);
};

}
#endif
