#include "ScenarioRoundTrip.h"
#include "PlaceCamera.h"
#include "io/WriteFileAtomically.h"
#include <Outshine.h>
#include <cstddef>
#include <expected>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Client {
namespace Says {
constexpr auto ChangedDeclaration = "serialized declarations differ after reimport";
constexpr auto CoverageLimit = "Roundtrip equality does not prove preservation of omitted "
                               "sections; independent fixtures provide that evidence.";
}

std::expected<size_t, std::string> RoundTripScenario(const Scenario::Document &declaration,
                                                     std::string_view scratchPath) {
  Engine engine;
  if (auto declared = engine.declare(declaration); !declared) {
    return std::unexpected(std::move(declared.error()));
  }
  auto first = engine.writeScenario();
  if (!first) { return std::unexpected(std::move(first.error())); }
  if (auto written = WriteFileAtomically(scratchPath, std::as_bytes(std::span(*first))); !written) {
    return std::unexpected(std::move(written.error()));
  }
  Engine restored;
  if (auto read = restored.readScenario(scratchPath); !read) {
    return std::unexpected(std::move(read.error()));
  }
  auto second = restored.writeScenario();
  if (!second) { return std::unexpected(std::move(second.error())); }
  if (*first != *second) { return std::unexpected(Says::ChangedDeclaration); }
  return first->size();
}

int RoundTripPlaces(std::span<const Shots::Place> places, std::string_view scratchPath) {
  size_t failures = 0;
  for (const auto &place : places) {
    const auto result = RoundTripScenario(place.Declaration, scratchPath);
    if (result) {
      std::println("HELD    {:<14} {} byte(s)", place.Name, *result);
    } else {
      std::println("APART   {:<14} {}", place.Name, result.error());
      ++failures;
    }
  }
  std::println("\n{} place(s) apart", failures);
  std::println("{}", Says::CoverageLimit);
  return failures == 0 ? 0 : 1;
}
}
