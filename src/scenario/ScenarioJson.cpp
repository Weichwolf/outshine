#include "ScenarioJson.h"
#include "Json.h"
#include "Xml.h"
#include "XmlAttribute.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace outshine {
namespace {

bool NameCharacter(char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':';
}

bool ValidName(std::string_view name) noexcept {
  return !name.empty() && NameCharacter(name.front()) && std::ranges::all_of(name, [](char c) {
    return NameCharacter(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
  });
}

bool Scalar(Json::Ref value) noexcept {
  return value.GetKind() == Json::Kind::String || value.GetKind() == Json::Kind::Number ||
         value.GetKind() == Json::Kind::Bool;
}

bool Attribute(Json::Ref value) {
  if (Scalar(value)) { return true; }
  if (value.GetKind() != Json::Kind::Array || value.Size() == 0) { return false; }
  for (size_t at = 0; at < value.Size(); ++at) {
    if (!Scalar(value[at])) { return false; }
  }
  return true;
}

std::string ScalarText(Json::Ref value) {
  if (value.GetKind() == Json::Kind::String) { return value.Str(); }
  if (value.GetKind() == Json::Kind::Bool) { return value.Bool() ? "yes" : "no"; }
  return std::string(value.Source());
}

std::string AttributeText(Json::Ref value) {
  if (Scalar(value)) { return ScalarText(value); }
  std::string text;
  for (size_t at = 0; at < value.Size(); ++at) {
    if (at != 0) { text += ' '; }
    text += ScalarText(value[at]);
  }
  return text;
}

class ScenarioJsonWriter {
public:
  [[nodiscard]] std::expected<std::string, std::string> Write(Json::Ref patch, Xml::Ref base) {
    if (!Node("scenario", patch, base, 0)) { return std::unexpected(std::move(Error_)); }
    return std::move(Text_);
  }

private:
  bool Refuse(std::string error) {
    Error_ = std::move(error);
    return false;
  }

  bool Members(Json::Ref patch) {
    std::unordered_set<std::string> names;
    for (size_t at = 0; at < patch.Size(); ++at) {
      const auto name = patch.Key(at);
      if (!ValidName(name)) { return Refuse("invalid scenario JSON key: " + name); }
      if (!names.insert(name).second) { return Refuse("duplicate scenario JSON key: " + name); }
    }
    return true;
  }

  void WriteAttribute(std::string_view name, const std::string &value) {
    Text_ += ' ';
    Text_ += name;
    Text_ += "=\"";
    AppendXmlAttribute(value, Text_);
    Text_ += '"';
  }

  bool Children(std::string_view name, Json::Ref patch, Xml::Ref base, size_t depth) {
    if (patch.GetKind() == Json::Kind::Null || Attribute(patch)) { return true; }
    if (patch.GetKind() == Json::Kind::Object) { return Node(name, patch, base, depth); }
    if (patch.GetKind() != Json::Kind::Array) {
      return Refuse("scenario JSON child must be an object: " + std::string(name));
    }
    for (size_t at = 0; at < patch.Size(); ++at) {
      if (patch[at].GetKind() != Json::Kind::Object) {
        return Refuse("scenario JSON collection must contain objects: " + std::string(name));
      }
      if (!Node(name, patch[at], {}, depth)) { return false; }
    }
    return true;
  }

  bool WriteChildren(Json::Ref patch, Xml::Ref base, size_t depth) {
    std::unordered_set<std::string> replaced;
    for (const auto child : base.Children()) {
      const auto key = child.Name();
      const auto value = patch[key.c_str()];
      if (!value.Valid()) {
        if (!Node(key, {}, child, depth + 1)) { return false; }
      } else if (replaced.insert(key).second) {
        if (value.GetKind() == Json::Kind::Object && base.Count(key.c_str()) != 1) {
          return Refuse("scenario JSON must replace repeated children with an array: " + key);
        }
        if (!Children(key, value, child, depth + 1)) { return false; }
      }
    }
    for (size_t at = 0; at < patch.Size(); ++at) {
      const auto key = patch.Key(at);
      if (!base.Child(key.c_str()).Valid() && !Children(key, patch[key.c_str()], {}, depth + 1)) {
        return false;
      }
    }
    return true;
  }

  bool Node(std::string_view name, Json::Ref patch, Xml::Ref base, size_t depth) {
    if (depth > kXmlMaxDepth || ++Nodes_ > kXmlMaxNodes) {
      return Refuse("scenario JSON exceeds the scenario node/depth budget");
    }
    if (patch.Valid() && patch.GetKind() != Json::Kind::Object) {
      return Refuse("scenario JSON must contain an object");
    }
    if (!Members(patch)) { return false; }
    Text_ += '<';
    Text_ += name;
    for (size_t at = 0; at < base.AttributeCount(); ++at) {
      const auto key = base.AttributeAt(at);
      if (!patch[key.c_str()].Valid()) { WriteAttribute(key, base.Attr(key.c_str())); }
    }
    for (size_t at = 0; at < patch.Size(); ++at) {
      const auto key = patch.Key(at);
      const auto value = patch[key.c_str()];
      if (Attribute(value)) { WriteAttribute(key, AttributeText(value)); }
    }
    Text_ += '>';
    Text_ += base.Text();
    if (!WriteChildren(patch, base, depth)) { return false; }
    Text_ += "</";
    Text_ += name;
    Text_ += '>';
    return Text_.size() <= 6 * kMostScenarioBytes || Refuse("scenario JSON exceeds output budget");
  }

  std::string Text_, Error_;
  size_t Nodes_ = 0;
};

}

std::expected<std::string, std::string> ScenarioXmlFromJson(std::string_view input,
                                                            std::string_view baseline) {
  if (input.size() > kMostScenarioBytes || baseline.size() > 6 * kMostScenarioBytes) {
    return std::unexpected("scenario JSON exceeds input budget");
  }
  Json json;
  if (!json.Parse(input.data(), input.size())) {
    return std::unexpected("invalid scenario JSON at byte " + std::to_string(json.StoppedAt()));
  }
  Xml xml;
  if (!baseline.empty() && !xml.Parse(baseline.data(), baseline.size())) {
    return std::unexpected(xml.Error());
  }
  if (!baseline.empty() && xml.Root().Name() != "scenario") {
    return std::unexpected("scenario override baseline must have a scenario root");
  }
  return ScenarioJsonWriter{}.Write(json.Root(), xml.Root());
}

}
