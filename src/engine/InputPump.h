#ifndef OUTSHINE_ENGINE_INPUTPUMP_H
#define OUTSHINE_ENGINE_INPUTPUMP_H

#include <cstddef>
#include <cstdint>

#include <SDL3/SDL_events.h>

#include "InputMap.h"

namespace outshine::Core {

class InputPump {
public:
  [[nodiscard]] bool Open(const InputMap &declared);

  struct Fired {
    uint16_t Action = InputMap::kUnbound;
    InputMap::Kind What = InputMap::Kind::Button;
    float Value = 0.0f;
  };

  [[nodiscard]] size_t Translate(const SDL_Event &event, Fired out[2]) const;

private:
  const InputMap *Map_ = nullptr;
};

} // namespace outshine::Core
#endif
