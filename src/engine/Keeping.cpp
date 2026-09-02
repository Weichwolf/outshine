#include "EngineHeld.h"
#include <algorithm>
#include <array>
#include <string_view>
#include <expected>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <charconv>
#include <algorithm>
#include <string>
#include <cstdio>
#include <system_error>
#include <cmath>
#include <utility>

namespace outshine {

Result Engine::save(std::string_view path) const {
  if (S_->Session.Declared.State.empty()) {
    S_->Error = "the scenario declares nothing to persist, so a save would be an empty "
                "promise -- declare <state><persist what=.../></state> first";
    return std::unexpected(S_->Error);
  }
  std::vector<std::string> lines;
  for (const Scenario::Persisted &row : S_->Session.Declared.State) {
    const size_t dot = row.What.find('.');
    if (dot == std::string::npos) {
      S_->Error = "the persist row '" + row.What +
                  "' names no instance.trait pair, and a save writes only what a load can "
                  "put back";
      return std::unexpected(S_->Error);
    }
    const Entity holder = S_->Cast.Stood.InstanceNamed(std::string_view(row.What).substr(0, dot));
    const uint32_t key = S_->Cast.Stood.TraitKey(std::string_view(row.What).substr(dot + 1));
    const Traits *held = holder == kNoEntity ? nullptr : S_->Cast.Kinds.Get(holder);
    const double *value = held == nullptr || key == 0 ? nullptr : held->Named(key);
    if (value == nullptr) {
      S_->Error = "the persist row '" + row.What +
                  "' names nothing the assembled scene holds -- a save of a missing value "
                  "would load as a lie";
      return std::unexpected(S_->Error);
    }
    std::array<char, 64> digits{};
    const auto written = std::to_chars(digits.data(), digits.data() + digits.size(), *value);
    lines.push_back(row.What + " " + std::string(digits.data(), written.ptr) + "\n");
  }
  std::ranges::sort(lines);
  std::string text = "outshine-save 1 " + S_->Session.Declared.Named.Name + " " +
                     S_->Session.Declared.Named.Version + "\n";
  for (const std::string &line : lines) { text += line; }
  if (text.size() > kMostSaveBytes) {
    S_->Error = "the save of " + std::to_string(text.size()) + " bytes overflows the bound of " +
                std::to_string(kMostSaveBytes);
    return std::unexpected(S_->Error);
  }
  const std::string held(path);
  std::FILE *const file = std::fopen(held.c_str(), "wb");
  if (file == nullptr) {
    S_->Error = held + ": the save file would not open";
    return std::unexpected(S_->Error);
  }
  const size_t wrote = std::fwrite(text.data(), 1, text.size(), file);
  const bool closed = std::fclose(file) == 0;
  if (wrote != text.size() || !closed) {
    S_->Error = held + ": the save did not reach the disk whole -- a full disk is a refusal, "
                       "never a successful save";
    return std::unexpected(S_->Error);
  }
  S_->Error.clear();
  return {};
}

Result Engine::restore(std::string_view path) {
  if ((S_->Cast.Stood.Instances.size() == 0u) && S_->Session.Declared.Instances.empty()) {
    S_->Error = "nothing is assembled, and loading a save is standing the scenario up FIRST "
                "and then applying the state -- one arrival route";
    return std::unexpected(S_->Error);
  }
  std::string text;
  if (!SlurpFile(std::string(path), text, S_->Error)) { return std::unexpected(S_->Error); }
  size_t at = text.find('\n');
  const std::string head = text.substr(0, at == std::string::npos ? text.size() : at);
  const std::string wanted = "outshine-save 1 " + S_->Session.Declared.Named.Name + " " +
                             S_->Session.Declared.Named.Version;
  if (head != wanted) {
    S_->Error = "the save says '" + head + "' and this engine stands '" + wanted +
                "' -- a save from another scenario or version refuses quoting both";
    return std::unexpected(S_->Error);
  }

  struct Landing {
    Entity Holder = kNoEntity;
    uint32_t Key = 0;
    double Value = 0.0;
  };

  std::vector<Landing> staged;
  while (at != std::string::npos && at + 1 < text.size()) {
    const size_t end = text.find('\n', at + 1);
    const std::string line =
        text.substr(at + 1, (end == std::string::npos ? text.size() : end) - at - 1);
    at = end;
    if (line.empty()) { continue; }
    const size_t gap = line.rfind(' ');
    const size_t dot = line.find('.');
    if (gap == std::string::npos || dot == std::string::npos || dot > gap) {
      S_->Error = "the save line '" + line + "' does not read as instance.trait value";
      return std::unexpected(S_->Error);
    }
    Landing landing;
    landing.Holder = S_->Cast.Stood.InstanceNamed(std::string_view(line).substr(0, dot));
    landing.Key = S_->Cast.Stood.TraitKey(std::string_view(line).substr(dot + 1, gap - dot - 1));
    const auto scanned =
        std::from_chars(line.data() + gap + 1, line.data() + line.size(), landing.Value);
    if (scanned.ec != std::errc() || scanned.ptr != line.data() + line.size() ||
        !std::isfinite(landing.Value)) {
      S_->Error = "the save line '" + line + "' does not read as instance.trait value";
      return std::unexpected(S_->Error);
    }
    if (landing.Holder == kNoEntity || landing.Key == 0) {
      S_->Error = "the save names '" + line.substr(0, gap) +
                  "', which the assembled scene does not hold -- the declaration moved on and "
                  "the save did not";
      return std::unexpected(S_->Error);
    }
    staged.push_back(landing);
  }
  std::vector<std::pair<Entity, Traits>> rows;
  for (const Landing &landing : staged) {
    Traits *row = nullptr;
    for (auto &held : rows) {
      if (held.first == landing.Holder) { row = &held.second; }
    }
    if (row == nullptr) {
      const Traits *standing = S_->Cast.Kinds.Get(landing.Holder);
      rows.emplace_back(landing.Holder, standing == nullptr ? Traits{} : *standing);
      row = &rows.back().second;
    }
    if (row->Named(landing.Key) == nullptr) {
      S_->Error = "the save carries a value for a trait this holder never declared -- the "
                  "declaration moved on and the save did not, and NOTHING was applied";
      return std::unexpected(S_->Error);
    }
    if (!row->Put(landing.Key, landing.Value)) {
      S_->Error = "the saved value found no seat -- the holder already carries its full " +
                  std::to_string(Traits::kMost) + " traits, and NOTHING was applied";
      return std::unexpected(S_->Error);
    }
  }
  for (const auto &held : rows) {
    if (!S_->Cast.Kinds.Put(held.first, held.second)) {
      S_->Error = "a validated holder died between the dry run and the commit";
      return std::unexpected(S_->Error);
    }
  }
  S_->Error.clear();
  return {};
}

Result Engine::park() {
  if (!S_->Picture.Standing) {
    S_->Error = "no scenario is standing, so there is nothing to park";
    return std::unexpected(S_->Error);
  }
  if (S_->Session.Declared.Named.Name.empty()) {
    S_->Error = "a scenario is parked under its name and this one declares none";
    return std::unexpected(S_->Error);
  }
  for (const Scenario::Document &asleep : S_->Session.Asleep) {
    if (asleep.Named.Name == S_->Session.Declared.Named.Name) {
      S_->Error = S_->Session.Declared.Named.Name +
                  " is parked already, so parking it twice would leave two";
      return std::unexpected(S_->Error);
    }
  }
  if (S_->Session.Asleep.size() >= kParkedBound) {
    S_->Error = "the parked set is full at its declared bound of " + std::to_string(kParkedBound) +
                " -- resume or discard '" + S_->Session.Asleep.front().Named.Name +
                "' (the least recently live) before parking " + S_->Session.Declared.Named.Name;
    return std::unexpected(S_->Error);
  }
  S_->Session.Asleep.push_back(S_->Session.Declared);
  S_->Picture.Standing.reset();
  S_->Error.clear();
  return {};
}

Result Engine::resume(std::string_view name) {
  if (S_->Picture.Standing) {
    S_->Error = "a scenario is standing, and Resume stands nothing down -- park it or Discard "
                "it explicitly first, because state that vanishes on somebody else's call is "
                "state nobody can reason about";
    return std::unexpected(S_->Error);
  }
  for (size_t at = 0; at < S_->Session.Asleep.size(); ++at) {
    if (S_->Session.Asleep[at].Named.Name != name) { continue; }
    if (!declare(S_->Session.Asleep[at])) { return std::unexpected(S_->Error); }
    S_->Session.Asleep.erase(S_->Session.Asleep.begin() + static_cast<long>(at));
    return {};
  }
  S_->Error =
      std::string(name) + " is not parked, and resuming what was never parked is not a load";
  return std::unexpected(S_->Error);
}

Result Engine::discard(std::string_view name) {
  for (size_t at = 0; at < S_->Session.Asleep.size(); ++at) {
    if (S_->Session.Asleep[at].Named.Name != name) { continue; }
    S_->Session.Asleep.erase(S_->Session.Asleep.begin() + static_cast<long>(at));
    S_->Error.clear();
    return {};
  }
  S_->Error = std::string(name) + " is not parked, so there is nothing to discard";
  return std::unexpected(S_->Error);
}

std::vector<std::string> Engine::parked() const {
  std::vector<std::string> names;
  for (const Scenario::Document &asleep : S_->Session.Asleep) {
    names.push_back(asleep.Named.Name);
  }
  return names;
}

} // namespace outshine
