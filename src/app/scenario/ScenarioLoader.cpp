#include "ScenarioLoader.h"
#include "ReadTextFile.h"
#include "ScenarioJson.h"
#include "ScenarioLayer.h"
#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include <utility>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace outshine::App {
namespace {

struct ScenarioInput {
  std::string Text;
  size_t RemainingBytes = 0;
};

[[nodiscard]] std::expected<ScenarioInput, std::string>
ReadScenarioSource(std::string_view source, std::string_view overrides) {
  if (source.contains('\0') || overrides.contains('\0') || overrides.size() > kMostScenarioBytes) {
    return std::unexpected("scenario input contains NUL or exceeds the input budget");
  }
  const bool inlineJson = IsJsonScenario(source);
  const size_t sourceBudget = kMostScenarioBytes - overrides.size();
  if (inlineJson && source.size() > sourceBudget) {
    return std::unexpected("scenario input exceeds the input budget");
  }
  auto slurped = inlineJson ? std::expected<std::string, std::string>(std::string(source))
                            : ReadTextFile(source, sourceBudget);
  if (!slurped) { return std::unexpected(slurped.error()); }
  if (slurped->size() > sourceBudget) {
    return std::unexpected("scenario input exceeds the input budget");
  }
  const size_t remaining = sourceBudget - slurped->size();
  if (!overrides.empty()) {
    auto xml = IsJsonScenario(*slurped) ? ScenarioXmlFromJson(*slurped) : std::move(slurped);
    if (!xml) { return std::unexpected(xml.error()); }
    auto patched = ScenarioXmlFromJson(overrides, *xml);
    if (!patched) { return std::unexpected(patched.error()); }
    slurped = std::move(patched);
  }
  return ScenarioInput{.Text = std::move(*slurped), .RemainingBytes = remaining};
}

[[nodiscard]] std::expected<void, std::string>
ResolveLayers(LoadedScenario &loaded, std::string_view directory, size_t remainingBytes) {
  auto &document = loaded.Document;
  for (const Scenario::Layer &layer : document.Layers) {
    const std::string named = layer.Id.empty() ? layer.Path : layer.Id;
    if (!LayerActive(layer, document.Named.Active)) {
      loaded.LayerTrace.push_back("layer '" + named + "' is inactive -- its set '" + layer.Set +
                                  "' is not selected by active=\"" + document.Named.Active + "\"");
      continue;
    }
    const std::string path = (!layer.Path.empty() && layer.Path.front() == '/')
                                 ? layer.Path
                                 : std::string(directory) + layer.Path;
    const auto fragment = ReadTextFile(path, remainingBytes);
    if (!fragment) { return std::unexpected(fragment.error()); }
    remainingBytes -= fragment->size();
    std::string error;
    if (!ApplyLayer(
            document, fragment->data(), fragment->size(), named, loaded.LayerTrace, error)) {
      error.insert(0, ": ");
      error.insert(0, path);
      return std::unexpected(std::move(error));
    }
  }
  document.Layers.clear();
  return {};
}

}

std::expected<LoadedScenario, std::string> LoadScenario(const ScenarioLoadRequest &request) {
  const auto input = ReadScenarioSource(request.Source, request.Overrides);
  if (!input) { return std::unexpected(input.error()); }
  LoadedScenario loaded;
  std::string error;
  const bool inlineJson = IsJsonScenario(request.Source);
  if (!ReadScenario(input->Text.data(), input->Text.size(), loaded.Document, error)) {
    return std::unexpected((inlineJson ? "inline scenario" : std::string(request.Source)) + ": " +
                           error);
  }
  std::string directory;
  if (inlineJson) {
    directory = request.ResourceDirectory;
    if (!directory.empty() && directory.back() != '/') { directory += '/'; }
  } else if (const size_t cut = request.Source.find_last_of('/'); cut != std::string_view::npos) {
    directory = request.Source.substr(0, cut + 1);
  }
  if (const auto layers = ResolveLayers(loaded, directory, input->RemainingBytes); !layers) {
    return std::unexpected(layers.error());
  }
  if (request.Overrides.empty()) { return loaded; }
  const auto flat = WriteScenario(loaded.Document);
  if (!flat) { return std::unexpected(flat.error()); }
  const auto patched = ScenarioXmlFromJson(request.Overrides, *flat);
  if (!patched) { return std::unexpected(patched.error()); }
  if (!ReadScenario(patched->data(), patched->size(), loaded.Document, error)) {
    return std::unexpected(error);
  }
  loaded.Document.Layers.clear();
  return loaded;
}

}
