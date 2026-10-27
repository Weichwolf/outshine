#include <print>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include "Check.h"
#include "Json.h"
#include "Layout.h"
#include "Markup.h"
#include "Style.h"

namespace {

using outshine::Json;
namespace Ui = outshine::Ui;

struct Assertion {
  const char *Attribute;
  const char *What;
};

constexpr std::array kAssertions{
    Assertion{.Attribute = "data-expected-width", .What = "width"},
    Assertion{.Attribute = "data-expected-height", .What = "height"},
    Assertion{.Attribute = "data-offset-x", .What = "x"},
    Assertion{.Attribute = "data-offset-y", .What = "y"},
};

std::string ReadFile(const std::string &path, bool &found) {
  std::FILE *f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) {
    found = false;
    return {};
  }
  std::string text;
  std::array<char, 1 << 15> buffer;
  while (std::feof(f) == 0 && std::ferror(f) == 0) {
    const size_t got = std::fread(buffer.data(), 1, buffer.size(), f);
    text.append(buffer.data(), got);
  }
  found = std::ferror(f) == 0;
  std::fclose(f);
  if (!found) { return {}; }
  return text;
}

bool NumberIn(const std::string &text, double &value) {
  char *end = nullptr;
  value = std::strtod(text.c_str(), &end);
  return end != nullptr && end != text.c_str() && *end == '\0';
}

bool ReadManifest(const std::string &prepared, Json &manifest) {
  bool found = false;
  const std::string manifestText = ReadFile(prepared + "/manifest.json", found);
  if (!found) {
    outshine::Test::Unprepared(
        (prepared + " is not prepared -- run test/harness/shared/corpus/prepare.py").c_str());
    return false;
  }
  if (!manifest.Parse(manifestText.c_str(), manifestText.size())) {
    std::array<char, 512> why;
    std::snprintf(why.data(),
                  why.size(),
                  "%s: the manifest is not JSON at byte %zu",
                  prepared.c_str(),
                  manifest.StoppedAt());
    outshine::Test::Checked(false, "the manifest parses", why.data(), __FILE__, __LINE__);
    return false;
  }
  const Json::Ref root = manifest.Root();
  const bool inputsDeclared = root["schemaVersion"].Num() == 2 && root["inputs"].Size() > 0;
  CHECK(inputsDeclared, "manifest declares versioned corpus inputs; reprepare older cases");
  return inputsDeclared;
}

bool ReadMarkup(const std::string &prepared,
                const Json::Ref &input,
                const std::string &id,
                Ui::Markup &markup) {
  bool found = false;
  const std::string entry = input["entry"].Str();
  const std::string document = ReadFile(prepared + "/" + entry, found);
  if (!found) {
    outshine::Test::Unprepared((prepared + " -- " + entry).c_str());
    return false;
  }

  std::string error;
  if (!markup.Read(document, error)) {
    const char *why = Ui::WhyOutside(error);
    std::println("UI-SUBSET {}", why != nullptr ? "reduced" : "outside");
    std::println("{} {} -- the document is outside this reader: {}{}",
                 why != nullptr ? "REDUCED" : "OUTSIDE",
                 id,
                 error,
                 why != nullptr ? (" (" + std::string(why) + ")") : "");
    outshine::Test::Checked(why != nullptr,
                            "the reader refuses at a boundary this engine declared",
                            (id + ": " + error).c_str(),
                            __FILE__,
                            __LINE__);
    return false;
  }

  return true;
}

bool ReadStylesheet(const std::string &prepared,
                    const Json::Ref &input,
                    const Ui::Markup &markup,
                    Ui::Stylesheet &sheet) {
  sheet.Read(Ui::UserAgentSheet());
  for (size_t i = 0; i < input["files"].Size(); ++i) {
    const Json::Ref file = input["files"][i];
    if (!file["role"].StrEquals("stylesheet")) { continue; }
    bool linked = false;
    const std::string linkedSheet = ReadFile(prepared + "/" + file["as"].Str(), linked);
    if (!linked) {
      outshine::Test::Unprepared((prepared + " -- " + file["as"].Str()).c_str());
      return false;
    }
    sheet.Read(linkedSheet);
  }
  sheet.Read(markup.StyleText());

  return true;
}

bool InsideSubset(const Ui::Markup &markup, const Ui::Stylesheet &sheet, const std::string &id) {
  const std::string scripted =
      markup.CarriesAScript() ? "a script in the document decides this layout" : "";
  const std::vector<std::string> outsideElements = Ui::ElementsOutsideTheSubset(markup);
  std::vector<std::string> names;
  for (const std::string &name : sheet.NamesOutsideTheSubset()) { names.push_back(name); }
  for (const std::string &name : outsideElements) { names.push_back("<" + name + ">"); }
  if (!scripted.empty()) { names.push_back(scripted); }

  if (!names.empty()) {
    std::string boundary;
    std::string gap;
    for (const std::string &name : names) {
      const char *why = Ui::WhyOutside(name);
      std::string &into = why != nullptr ? boundary : gap;
      if (!into.empty()) { into += " "; }
      into += name;
      if (why != nullptr) {
        into += " (";
        into += why;
        into += ")";
      }
    }
    if (gap.empty()) {
      std::println("UI-SUBSET reduced");
      std::println(
          "REDUCED {} -- every name that puts it outside is a declared boundary: {}", id, boundary);
      outshine::Test::Checked(true,
                              "the case is outside a boundary this engine declared",
                              (id + ": " + boundary).c_str(),
                              __FILE__,
                              __LINE__);
    } else {
      std::println("UI-SUBSET outside");
      std::println("OUTSIDE {} -- undeclared: {}", id, gap);
      outshine::Test::Checked(
          false,
          "every name that puts a case outside is declared",
          (id + ": " + gap + " is outside the subset and nothing says why").c_str(),
          __FILE__,
          __LINE__);
    }
    return false;
  }

  return true;
}

std::pair<double, double> OriginOf(const Ui::Layout &layout, int at) {
  double x = 0;
  double y = 0;
  for (int up = layout.Boxes()[static_cast<size_t>(at)].Parent; up >= 0;
       up = layout.Boxes()[static_cast<size_t>(up)].Parent) {
    const Ui::Box &over = layout.Boxes()[static_cast<size_t>(up)];
    if (!over.Positioned) { continue; }
    x = over.X + over.Border.Left;
    y = over.Y + over.Top();
    break;
  }
  return std::pair<double, double>{x, y};
}

double
Measured(const Ui::Box &box, const Assertion &assertion, const std::pair<double, double> &origin) {
  if (std::strcmp(assertion.What, "width") == 0) { return box.Width; }
  if (std::strcmp(assertion.What, "height") == 0) { return box.Height; }
  if (std::strcmp(assertion.What, "x") == 0) { return box.X - origin.first; }
  return box.Y - origin.second;
}

struct Score {
  int Stated = 0;
  double Worst = 0;
};

void ScoreBox(const Ui::Layout &layout,
              const Ui::Markup &markup,
              size_t boxAt,
              const std::string &id,
              Score &score) {
  const Ui::Box &box = layout.Boxes()[boxAt];
  if (box.Node < 0) { return; }
  const std::pair<double, double> origin = OriginOf(layout, static_cast<int>(boxAt));
  const std::string &element = markup.Nodes()[static_cast<size_t>(box.Node)].Name;
  for (const Assertion &assertion : kAssertions) {
    const std::string *declared = markup.AttributeOf(box.Node, assertion.Attribute);
    if (declared == nullptr) { continue; }
    ++score.Stated;
    double want = 0;
    if (!NumberIn(*declared, want)) {
      const std::string claim = std::format(
          "{}: <{}> {}=\"{}\" is not a number", id, element, assertion.Attribute, *declared);
      outshine::Test::Checked(
          false, "the declaration is a number", claim.c_str(), __FILE__, __LINE__);
      continue;
    }
    const double got = Measured(box, assertion, origin);

    const bool integral = want == std::floor(want);
    const double residual = std::fabs(got - want);
    score.Worst = std::fmax(score.Worst, residual);
    const bool holds = integral ? std::floor(got + 0.5) == want : got == want;
    std::array<char, 224> measured;
    std::snprintf(measured.data(),
                  measured.size(),
                  "%s: <%s> %s is %.6f, the document states %.6f",
                  id.c_str(),
                  element.c_str(),
                  assertion.What,
                  got,
                  want);
    outshine::Test::Checked(
        holds, "the box lands where the document says", measured.data(), __FILE__, __LINE__);
  }
}

void ScoreLayout(const Ui::Layout &layout, const Ui::Markup &markup, const std::string &id) {
  std::println("UI-SUBSET inside");
  Score score;
  for (size_t boxAt = 0; boxAt < layout.Boxes().size(); ++boxAt) {
    ScoreBox(layout, markup, boxAt, id, score);
  }
  outshine::Test::Checked(score.Stated > 0,
                          "the document states its own layout",
                          (id + " carries at least one assertion").c_str(),
                          __FILE__,
                          __LINE__);
  std::println(
      "STATED {} {} assertions, worst sub-pixel residual {:.6f} px", id, score.Stated, score.Worst);
  std::println("UI-LAYOUT {}", outshine::Test::Failures.Value() == 0 ? "held" : "red");
}

}

int main(int argc, char **argv) {
  if (argc != 2) {
    outshine::Test::Checked(
        false, "argc == 2", "one prepared case directory is the argument", __FILE__, __LINE__);
    return outshine::Test::Report();
  }
  const std::string prepared = argv[1];
  Json manifest;
  if (!ReadManifest(prepared, manifest)) { return outshine::Test::Report(); }
  const Json::Ref root = manifest.Root();
  const std::string id = root["id"].Str();
  const Json::Ref input = root["inputs"][size_t{0}];
  Ui::Markup markup;
  if (!ReadMarkup(prepared, input, id, markup)) { return outshine::Test::Report(); }
  Ui::Stylesheet sheet;
  if (!ReadStylesheet(prepared, input, markup, sheet)) { return outshine::Test::Report(); }
  Ui::Layout layout;
  const Ui::AhemFont font;
  std::string error;
  if (!layout.Build(markup,
                    sheet,
                    root["viewport"]["widthPx"].Num(),
                    root["viewport"]["heightPx"].Num(),
                    font,
                    error)) {
    outshine::Test::Checked(
        false, "the layout builds", (id + ": " + error).c_str(), __FILE__, __LINE__);
    std::println("UI-LAYOUT red");
    return outshine::Test::Report();
  }
  if (InsideSubset(markup, sheet, id)) { ScoreLayout(layout, markup, id); }
  return outshine::Test::Report();
}
