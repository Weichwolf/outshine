#include "SignalGraph.h"

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>
#include <map>
#include <string_view>

namespace outshine::Audio {
namespace {
namespace Says {
constexpr auto Budget = "audio signal graph exceeds node or edge budget";
constexpr auto Id = "audio signal graph requires nonempty unique node IDs";
constexpr auto Missing = "audio signal graph input refers to a missing node";
constexpr auto Cycle = "audio signal graph contains a cycle";
}
}

std::expected<SignalGraph, std::string>
SignalGraph::Compile(std::span<const Scenario::Voice> voices, SignalGraphBudget &remaining) {
  if (voices.size() > remaining.Nodes) { return std::unexpected(Says::Budget); }
  size_t edges = 0;
  for (const auto &voice : voices) {
    if (voice.From.size() > remaining.Edges - edges) { return std::unexpected(Says::Budget); }
    edges += voice.From.size();
  }
  std::map<std::string_view, size_t> names;
  for (size_t index = 0; index < voices.size(); ++index) {
    if (voices[index].Id.empty() || !names.emplace(voices[index].Id, index).second) {
      return std::unexpected(Says::Id);
    }
  }
  SignalGraph result;
  result.Inputs.resize(voices.size());
  result.Order.reserve(voices.size());
  std::vector<std::vector<size_t>> consumers(voices.size());
  std::vector<size_t> pending(voices.size());
  for (size_t index = 0; index < voices.size(); ++index) {
    for (const auto &id : voices[index].From) {
      const auto found = names.find(id);
      if (found == names.end()) { return std::unexpected(Says::Missing); }
      result.Inputs[index].push_back(found->second);
      consumers[found->second].push_back(index);
    }
    pending[index] = result.Inputs[index].size();
    if (pending[index] == 0) { result.Order.push_back(index); }
  }
  for (size_t next = 0; next < result.Order.size(); ++next) {
    for (const size_t consumer : consumers[result.Order[next]]) {
      if (--pending[consumer] == 0) { result.Order.push_back(consumer); }
    }
  }
  if (result.Order.size() != voices.size()) { return std::unexpected(Says::Cycle); }
  remaining.Nodes -= voices.size();
  remaining.Edges -= edges;
  return result;
}

}
