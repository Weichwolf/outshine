#include "EngineHeld.h"
#include "Assembled.h"
#include "Column.h"
#include "Traits.h"
#include "ReadTextFile.h"
#include "WriteFileAtomically.h"
#include <algorithm>
#include <array>
#include <string_view>
#include <span>
#include <expected>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <charconv>
#include <string>
#include <system_error>
#include <cmath>

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
    const Entity holder =
        S_->Simulation->Stood.InstanceNamed(std::string_view(row.What).substr(0, dot));
    const uint32_t key = S_->Simulation->Stood.TraitKey(std::string_view(row.What).substr(dot + 1));
    const Traits *held = holder == kNoEntity ? nullptr : S_->Simulation->Kinds.Get(holder);
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
  const auto written = WriteFileAtomically(path, std::as_bytes(std::span(text)));
  if (!written) {
    S_->Error = written.error();
    return std::unexpected(S_->Error);
  }
  S_->Error.clear();
  return {};
}

namespace Says {
constexpr std::string_view InvalidSavedTrait = "invalid saved instance.trait value: ";
constexpr std::string_view UnknownSavedTrait =
    "saved trait is not held by the assembled instance: ";
constexpr std::string_view MissingSavedHolder = "saved holder has no current trait component";
constexpr std::string_view SavedTraitCapacity = "saved trait cannot replace its declared value";
constexpr std::string_view RestoreTargetsChanged =
    "restore targets are no longer valid; nothing applied";
}

namespace {
struct SavedTrait {
  Entity Holder = kNoEntity;
  uint32_t Key = 0;
  double Value = 0.0;
};

std::expected<SavedTrait, std::string> ParseSavedTrait(std::string_view line,
                                                       const Assembled &scene) {
  const auto invalid = [&line] {
    return std::unexpected(std::string(Says::InvalidSavedTrait) + std::string(line));
  };
  const size_t gap = line.rfind(' ');
  const size_t dot = line.find('.');
  if (gap == std::string_view::npos || dot == std::string_view::npos || dot > gap) {
    return invalid();
  }
  SavedTrait trait;
  trait.Holder = scene.InstanceNamed(line.substr(0, dot));
  trait.Key = scene.TraitKey(line.substr(dot + 1, gap - dot - 1));
  const auto scanned =
      std::from_chars(line.data() + gap + 1, line.data() + line.size(), trait.Value);
  if (scanned.ec != std::errc() || scanned.ptr != line.data() + line.size() ||
      !std::isfinite(trait.Value)) {
    return invalid();
  }
  if (trait.Holder == kNoEntity || trait.Key == 0) {
    return std::unexpected(std::string(Says::UnknownSavedTrait) + std::string(line.substr(0, gap)));
  }
  return trait;
}

std::expected<std::vector<Column<Traits>::Replacement>, std::string>
StageSavedTraits(std::span<SavedTrait> traits, const Column<Traits> &column) {
  std::ranges::stable_sort(traits, {}, [](const SavedTrait &trait) { return trait.Holder.Index; });
  std::vector<Column<Traits>::Replacement> rows;
  for (const auto &trait : traits) {
    if (rows.empty() || rows.back().Owner != trait.Holder) {
      const Traits *standing = column.Get(trait.Holder);
      if (standing == nullptr) { return std::unexpected(std::string(Says::MissingSavedHolder)); }
      rows.push_back({.Owner = trait.Holder, .Data = *standing});
    }
    auto &row = rows.back().Data;
    if (row.Named(trait.Key) == nullptr || !row.Put({.Key = trait.Key, .Value = trait.Value})) {
      return std::unexpected(std::string(Says::SavedTraitCapacity));
    }
  }
  return rows;
}
}

Result Engine::restore(std::string_view path) {
  if (S_->Simulation->Stood.Instances.empty() && S_->Session.Declared.Instances.empty()) {
    S_->Error = "nothing is assembled, and loading a save is standing the scenario up FIRST "
                "and then applying the state -- one arrival route";
    return std::unexpected(S_->Error);
  }
  const std::expected<std::string, std::string> slurped = ReadTextFile(path, kMostSaveBytes);
  if (!slurped) {
    S_->Error = slurped.error();
    return std::unexpected(S_->Error);
  }
  const std::string &text = *slurped;
  size_t at = text.find('\n');
  const std::string head = text.substr(0, at == std::string::npos ? text.size() : at);
  const std::string wanted = "outshine-save 1 " + S_->Session.Declared.Named.Name + " " +
                             S_->Session.Declared.Named.Version;
  if (head != wanted) {
    S_->Error = "the save says '" + head + "' and this engine stands '" + wanted +
                "' -- a save from another scenario or version refuses quoting both";
    return std::unexpected(S_->Error);
  }

  std::vector<SavedTrait> staged;
  while (at != std::string::npos && at + 1 < text.size()) {
    const size_t end = text.find('\n', at + 1);
    const std::string_view line = std::string_view(text).substr(
        at + 1, (end == std::string::npos ? text.size() : end) - at - 1);
    at = end;
    if (line.empty()) { continue; }
    const auto trait = ParseSavedTrait(line, S_->Simulation->Stood);
    if (!trait) {
      S_->Error = trait.error();
      return std::unexpected(S_->Error);
    }
    staged.push_back(*trait);
  }
  const auto rows = StageSavedTraits(staged, S_->Simulation->Kinds);
  if (!rows) {
    S_->Error = rows.error();
    return std::unexpected(S_->Error);
  }
  if (!S_->Simulation->Kinds.Replace(*rows)) {
    S_->Error = Says::RestoreTargetsChanged;
    return std::unexpected(S_->Error);
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
  S_->World.Bakes.Clear();
  S_->World.Pieces.Clear();
  S_->World.Sheets.Clear();
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

}
