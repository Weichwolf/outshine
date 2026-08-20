#ifndef RENDER_MANIFEST_SCHEMA_H
#define RENDER_MANIFEST_SCHEMA_H

#include <cmath>
#include <string>
#include <vector>

#include "Json.h"

namespace outshine::Render::Parity {

class ManifestSchema {
public:
  [[nodiscard]] bool Load(const std::string &text, std::string &error) {
    if (!Declaration_.Parse(text.c_str(), text.size())) {
      error = "the manifest schema stopped parsing at byte " +
              std::to_string(Declaration_.StoppedAt());
      return false;
    }
    if (!Declaration_.Root()["objects"]["manifest"].Valid()) {
      error = "the manifest schema declares no object named 'manifest'";
      return false;
    }
    return true;
  }

  [[nodiscard]] bool Check(const Json::Ref &document, std::string &error) const {
    return Object("manifest", "manifest", document, {}, error);
  }

private:
  Json Declaration_;

  [[nodiscard]] Json::Ref Declared(const std::string &name) const {
    return Declaration_.Root()["objects"][name.c_str()];
  }

  [[nodiscard]] static bool Missing(const std::string &where, const std::string &key,
                                    std::string &error) {
    error = where + " states no " + key;
    return false;
  }

  [[nodiscard]] bool Object(const std::string &name, const std::string &where,
                            const Json::Ref &value, std::vector<std::string> inherited,
                            std::string &error) const {
    Json::Ref node = Declared(name);
    if (!node.Valid()) {
      error = "the manifest schema declares no object named '" + name + "'";
      return false;
    }
    if (value.GetKind() != Json::Kind::Object) {
      error = where + " is not an object";
      return false;
    }
    while (node["variants"].Valid()) {
      const std::string key = node["discriminator"].Str("");
      const Json::Ref chosen = value[key.c_str()];
      const std::string spelling =
          chosen.Valid() ? chosen.Str("") : node["default"].Str("");
      const Json::Ref variant = node["variants"][spelling.c_str()];
      if (spelling.empty() || !variant.Valid()) {
        error = where + "." + key + " is '" + spelling + "', and the schema declares " +
                Spellings(node["variants"]);
        return false;
      }
      inherited.push_back(key);
      node = variant;
    }
    const Json::Ref required = node["required"];
    const Json::Ref optional = node["optional"];
    for (size_t entry = 0; entry < value.Size(); ++entry) {
      const std::string key = value.Key(entry);
      if (required[key.c_str()].Valid() || optional[key.c_str()].Valid()) { continue; }
      bool carried = false;
      for (const std::string &discriminator : inherited) { carried |= discriminator == key; }
      if (carried) { continue; }
      error = where + "." + key + " is a key the schema does not declare, and a key nobody reads " +
              "is a setting that silently did not apply";
      return false;
    }
    for (size_t entry = 0; entry < required.Size(); ++entry) {
      const std::string key = required.Key(entry);
      if (!value[key.c_str()].Valid()) { return Missing(where, key, error); }
    }
    for (const Json::Ref &group : {required, optional}) {
      for (size_t entry = 0; entry < group.Size(); ++entry) {
        const std::string key = group.Key(entry);
        const Json::Ref held = value[key.c_str()];
        if (!held.Valid()) { continue; }
        if (!Value(group[key.c_str()], where + "." + key, held, error)) { return false; }
      }
    }
    return true;
  }

  [[nodiscard]] static std::string Spellings(const Json::Ref &variants) {
    std::string list;
    for (size_t entry = 0; entry < variants.Size(); ++entry) {
      list += (entry ? ", " : "") + variants.Key(entry);
    }
    return list;
  }

  [[nodiscard]] bool Value(const Json::Ref &declared, const std::string &where,
                           const Json::Ref &value, std::string &error) const {
    if (declared.GetKind() == Json::Kind::String) {
      return Scalar(declared.Str(""), where, value, error);
    }
    if (declared["enum"].Valid()) {
      const Json::Ref allowed = declared["enum"];
      for (size_t entry = 0; entry < allowed.Size(); ++entry) {
        const Json::Ref option = allowed[entry];
        if (option.GetKind() != value.GetKind()) { continue; }
        if (option.GetKind() == Json::Kind::Number && option.Num() == value.Num()) { return true; }
        if (option.GetKind() == Json::Kind::String && value.StrEquals(option.Str("").c_str())) {
          return true;
        }
      }
      error = where + " is '" + value.Str("") + "', and the schema declares one of " +
              Options(allowed);
      return false;
    }
    if (declared["object"].Valid()) {
      return Object(declared["object"].Str(""), where, value, {}, error);
    }
    if (declared["array"].Valid()) {
      if (value.GetKind() != Json::Kind::Array || value.Size() == 0) {
        error = where + " is not a non-empty list";
        return false;
      }
      for (size_t entry = 0; entry < value.Size(); ++entry) {
        if (!Value(declared["array"], where + "[" + std::to_string(entry) + "]", value[entry],
                   error)) {
          return false;
        }
      }
      return true;
    }

    if (declared["oneOrMore"].Valid()) {
      if (value.GetKind() != Json::Kind::Array) {
        return Value(declared["oneOrMore"], where, value, error);
      }
      if (value.Size() == 0) {
        error = where + " is an empty list where one entry or several were declared";
        return false;
      }
      for (size_t entry = 0; entry < value.Size(); ++entry) {
        if (!Value(declared["oneOrMore"], where + "[" + std::to_string(entry) + "]", value[entry],
                   error)) {
          return false;
        }
      }
      return true;
    }
    if (declared["map"].Valid()) {
      if (value.GetKind() != Json::Kind::Object || value.Size() == 0) {
        error = where + " is not a non-empty object";
        return false;
      }
      for (size_t entry = 0; entry < value.Size(); ++entry) {
        const std::string key = value.Key(entry);
        if (!Value(declared["map"], where + "." + key, value[key.c_str()], error)) { return false; }
      }
      return true;
    }
    if (declared["quantity"].Valid()) { return Quantity(declared["quantity"], where, value, error); }
    error = "the manifest schema declares a type at " + where + " that this reader does not know";
    return false;
  }

  [[nodiscard]] static std::string Options(const Json::Ref &allowed) {
    std::string list;
    for (size_t entry = 0; entry < allowed.Size(); ++entry) {
      const Json::Ref option = allowed[entry];
      list += (entry ? ", " : "");
      list += option.GetKind() == Json::Kind::String ? option.Str("")
                                                     : std::to_string(option.Num());
    }
    return list;
  }

  [[nodiscard]] bool Quantity(const Json::Ref &inner, const std::string &where,
                             const Json::Ref &value, std::string &error) const {
    if (value.GetKind() != Json::Kind::Object) {
      error = where + " is not an object carrying value, unit and origin";
      return false;
    }
    static const char *const kKnown[] = {"value", "unit", "origin", "derivation", "note"};
    for (size_t entry = 0; entry < value.Size(); ++entry) {
      const std::string key = value.Key(entry);
      bool known = false;
      for (const char *const candidate : kKnown) { known |= key == candidate; }
      if (!known) {
        error = where + "." + key + " is a key a declared number does not have";
        return false;
      }
    }
    if (!value["value"].Valid()) { return Missing(where, "value", error); }
    if (!Value(inner, where + ".value", value["value"], error)) { return false; }
    if (value["unit"].Str("").empty()) { return Missing(where, "unit", error); }
    const std::string origin = value["origin"].Str("");
    if (origin != "SET" && origin != "derived" && origin != "measured") {
      error = where + " has origin '" + origin + "', and a number's origin is SET, derived or "
                                                 "measured";
      return false;
    }
    if (origin == "derived" && value["derivation"].Str("").empty()) {
      error = where + " is derived and states no derivation, which is a bare number wearing a label";
      return false;
    }
    return true;
  }

  [[nodiscard]] static bool Whole(const Json::Ref &value) {
    return value.GetKind() == Json::Kind::Number &&
           value.Num() == std::floor(value.Num());
  }

  [[nodiscard]] static bool Hex(const std::string &text, size_t digits) {
    if (text.size() != digits) { return false; }
    for (const char character : text) {
      const bool digit = character >= '0' && character <= '9';
      const bool lower = character >= 'a' && character <= 'f';
      if (!digit && !lower) { return false; }
    }
    return true;
  }

  [[nodiscard]] static bool Release(const std::string &text) {
    size_t digits = 0, dots = 0;
    for (const char character : text) {
      if (character == '.') {
        if (digits == 0) { return false; }
        ++dots;
        digits = 0;
        continue;
      }
      if (character < '0' || character > '9') { return false; }
      ++digits;
    }
    return digits > 0 && (dots == 1 || dots == 2);
  }

  [[nodiscard]] static bool Scalar(const std::string &kind, const std::string &where,
                                   const Json::Ref &value, std::string &error) {
    const bool text = value.GetKind() == Json::Kind::String;
    bool held = false;
    if (kind == "string") {
      held = text;
    } else if (kind == "text") {
      held = text && !value.Str("").empty();
    } else if (kind == "boolean") {
      held = value.GetKind() == Json::Kind::Bool;
    } else if (kind == "number") {
      held = value.GetKind() == Json::Kind::Number;
    } else if (kind == "integer") {
      held = Whole(value);
    } else if (kind == "index") {
      held = Whole(value) && value.Num() >= 0.0;
    } else if (kind == "vector3") {
      held = value.GetKind() == Json::Kind::Array && value.Size() == 3;
      for (size_t entry = 0; held && entry < 3; ++entry) {
        held = value[entry].GetKind() == Json::Kind::Number;
      }
    } else if (kind == "opaque") {
      held = value.GetKind() == Json::Kind::Object && value.Size() > 0;
    } else if (kind == "filename") {

      const std::string name = value.Str("");
      held = text && !name.empty() && name[0] != '.' && name[0] != '/' && name.back() != '/';
      for (size_t at = 0, next = 0; held && at < name.size(); at = next + 1) {
        next = name.find('/', at);
        if (next == std::string::npos) { next = name.size(); }
        const std::string segment = name.substr(at, next - at);
        held = !segment.empty() && segment != "." && segment != "..";
      }
    } else if (kind == "sha256") {
      held = text && Hex(value.Str(""), 64);
    } else if (kind == "sha1") {
      held = text && Hex(value.Str(""), 40);
    } else if (kind == "release") {
      held = text && Release(value.Str(""));
    } else {
      error = "the manifest schema declares the scalar '" + kind + "' at " + where +
              ", and this reader does not know it";
      return false;
    }
    if (!held) { error = where + " is not " + kind; }
    return held;
  }
};

[[nodiscard]] inline std::string SchemaPathBesideCase(const std::string &caseDirectory) {

  (void)caseDirectory;
  return "test/harness/shared/corpus/manifest-schema.json";
}

}

#endif
