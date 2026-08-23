#ifndef OUTSHINE_CLIENTS_INPUTPUMP_H
#define OUTSHINE_CLIENTS_INPUTPUMP_H

#include <cstddef>
#include <cstdint>

#include <SDL3/SDL_events.h>

#include "InputMap.h"

namespace outshine::Clients {

// the one seam between the device and the declaration: an SDL event resolves to its
// catalogue index ONCE at open, and a pumped event leaves as the declared action's id --
// the client never sees a keycode, and an event the scenario left unbound leaves as nothing
class InputPump {
public:
  [[nodiscard]] bool Open(const InputMap &declared);

  struct Fired {
    uint16_t Action = InputMap::kUnbound;
    InputMap::Kind What = InputMap::Kind::Button;
    float Value = 0.0f; // a button's press is 1, its release 0; an axis is its own value
  };

  // 0..2 firings per device event -- mouse motion carries two axes in one event
  [[nodiscard]] size_t Translate(const SDL_Event &event, Fired out[2]) const;

private:
  const InputMap *Map_ = nullptr;
};

}
#endif
