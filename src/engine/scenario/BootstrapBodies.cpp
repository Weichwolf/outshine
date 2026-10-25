#include "BootstrapInputs.h"
#include "Scenario.h"
#include <span>
#include <string>

namespace outshine {
bool BuildBootstrapBodies(std::span<const Scenario::Body> bodies,
                          const Scenario::Player &player,
                          BootstrapInputs &inputs,
                          std::string &error) {
  inputs.Output.Bodies.reserve(bodies.size());
  for (const auto &body : bodies) {
    const auto entity = inputs.Entities.addEntity();
    if (!entity) {
      error = entity.error().Message;
      return false;
    }
    if (!inputs.Bodies.Put(*entity, body)) {
      error = "could not install a scenario body definition";
      return false;
    }
    inputs.Output.Bodies.push_back(*entity);
    if (!player.Is.empty() && player.Is == body.Name) { inputs.Output.PlayerBody = *entity; }
  }
  if (!player.Is.empty() && inputs.Output.PlayerBody == kNoEntity) {
    error = "the player requires a declared scenario body";
    return false;
  }
  return true;
}
}
