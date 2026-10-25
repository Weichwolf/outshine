#include "Assembly.h"
#include "Assembled.h"
#include "BodyValidation.h"
#include "BootstrapInputs.h"
#include "Column.h"
#include "EntityRegistry.h"
#include "Scenario.h"
#include "Traits.h"
#include <array>
#include <cstddef>
#include <expected>
#include <string>

namespace outshine {
namespace {
constexpr size_t kMaximumScenarioItems = 65536;

}

std::expected<size_t, std::string> RequiredEntityCapacity(const Scenario::Document &declared) {
  const std::array counts{
      declared.Room, declared.Bodies.size(), declared.Kinds.size(), declared.Instances.size()};
  size_t items = 0;
  for (const size_t count : counts) {
    if (count > kMaximumScenarioItems - items) {
      return std::unexpected("scenario item capacity exceeds 65536");
    }
    items += count;
  }
  return declared.Room + declared.Bodies.size() + declared.Instances.size();
}

bool Assemble(const Scenario::Document &declared,
              World::EntityRegistry &entities,
              Column<Scenario::Body> &bodies,
              Column<Traits> &traits,
              Assembled &out,
              std::string &error) {
  if (const auto valid = ValidateBodyDynamics(declared.Bodies); !valid) {
    error = valid.error();
    return false;
  }
  out = {};
  BootstrapInputs inputs{.Entities = entities,
                         .Bodies = bodies,
                         .Attributes = traits,
                         .Output = out,
                         .Kinds = {},
                         .Instances = {},
                         .TraitKeys = {}};
  return BuildBootstrapKinds(declared.Kinds, inputs, error) &&
         BuildBootstrapInstances(declared.Instances, inputs, error) &&
         LinkBootstrapInstances(declared.Instances, inputs, error) &&
         BuildBootstrapBodies(declared.Bodies, declared.Played, inputs, error);
}
}
