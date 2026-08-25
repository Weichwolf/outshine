#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
constexpr Assertion kAssertions[] = {
    {"data-expected-width", "width"},
    {"data-expected-height", "height"},
    {"data-offset-x", "x"},
    {"data-offset-y", "y"},
};

std::string ReadFile(const std::string &path, bool &found) {
  std::FILE *f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) {
    found = false;
    return {};
  }
  std::string text;
  char buffer[1 << 15];
  size_t got = 0;
  while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0) { text.append(buffer, got); }
  std::fclose(f);
  found = true;
  return text;
}

bool NumberIn(const std::string &text, double &value) {
  char *end = nullptr;
  value = std::strtod(text.c_str(), &end);
  return end != nullptr && end != text.c_str() && *end == '\0';
}

}

int main(int argc, char **argv) {
  if (argc != 2) {
    outshine::Test::Checked(false, "argc == 2", "one prepared case directory is the argument",
                            __FILE__, __LINE__);
    return outshine::Test::Report();
  }

  const std::string prepared = argv[1];

  bool found = false;
  const std::string manifestText = ReadFile(prepared + "/manifest.json", found);
  if (!found) {
    outshine::Test::Unprepared((prepared + " is not prepared -- run test/harness/shared/corpus/prepare.py").c_str());
    return outshine::Test::Report();
  }
  Json manifest;
  if (!manifest.Parse(manifestText.c_str(), manifestText.size())) {
    char why[512];
    std::snprintf(why, sizeof why, "%s: the manifest is not JSON at byte %zu", prepared.c_str(),
                  manifest.StoppedAt());
    outshine::Test::Checked(false, "the manifest parses", why, __FILE__, __LINE__);
    return outshine::Test::Report();
  }
  const Json::Ref root = manifest.Root();
  const std::string id = root["id"].Str();
  const Json::Ref subject = root["subjects"][size_t(0)];
  const std::string entry = subject["entry"].Str();
  const double viewportWidth = root["viewport"]["widthPx"].Num();
  const double viewportHeight = root["viewport"]["heightPx"].Num();

  const std::string document = ReadFile(prepared + "/" + entry, found);
  if (!found) {
    outshine::Test::Unprepared((prepared + " -- " + entry).c_str());
    return outshine::Test::Report();
  }

  Ui::Markup markup;
  std::string error;
  if (!markup.Read(document, error)) {

    const char *why = Ui::WhyOutside(error);
    std::printf("UI-SUBSET %s\n", why != nullptr ? "reduced" : "outside");
    std::printf("%s %s -- the document is outside this reader: %s%s\n",
                why != nullptr ? "REDUCED" : "OUTSIDE", id.c_str(), error.c_str(),
                why != nullptr ? (" (" + std::string(why) + ")").c_str() : "");
    outshine::Test::Checked(why != nullptr, "the reader refuses at a boundary this engine declared",
                            (id + ": " + error).c_str(), __FILE__, __LINE__);
    return outshine::Test::Report();
  }

  Ui::Stylesheet sheet;
  sheet.Read(Ui::UserAgentSheet());
  for (size_t i = 0; i < subject["files"].Size(); ++i) {
    const Json::Ref file = subject["files"][i];
    if (!file["role"].StrEquals("stylesheet")) { continue; }
    bool linked = false;
    const std::string linkedSheet = ReadFile(prepared + "/" + file["as"].Str(), linked);
    if (!linked) {
      outshine::Test::Unprepared((prepared + " -- " + file["as"].Str()).c_str());
      return outshine::Test::Report();
    }
    sheet.Read(linkedSheet);
  }
  sheet.Read(markup.StyleText());

  Ui::Layout layout;
  const Ui::AhemFont font;
  if (!layout.Build(markup, sheet, viewportWidth, viewportHeight, font, error)) {
    outshine::Test::Checked(false, "the layout builds", (id + ": " + error).c_str(), __FILE__,
                            __LINE__);
    std::printf("UI-LAYOUT red\n");
    return outshine::Test::Report();
  }

  const std::string scripted =
      markup.CarriesAScript() ? "a script in the document decides this layout" : "";
  const std::vector<std::string> outsideElements = Ui::ElementsOutsideTheSubset(markup);
  std::vector<std::string> names;
  for (const std::string &name : sheet.NamesOutsideTheSubset()) { names.push_back(name); }
  for (const std::string &name : outsideElements) { names.push_back("<" + name + ">"); }
  if (!scripted.empty()) { names.push_back(scripted); }

  if (!names.empty()) {

    std::string boundary, gap;
    for (const std::string &name : names) {
      const char *why = Ui::WhyOutside(name);
      std::string &into = why != nullptr ? boundary : gap;
      if (!into.empty()) { into += " "; }
      into += name;
      if (why != nullptr) { into += " (" + std::string(why) + ")"; }
    }
    if (gap.empty()) {
      std::printf("UI-SUBSET reduced\n");
      std::printf("REDUCED %s -- every name that puts it outside is a declared boundary: %s\n",
                  id.c_str(), boundary.c_str());
      outshine::Test::Checked(true, "the case is outside a boundary this engine declared",
                              (id + ": " + boundary).c_str(), __FILE__, __LINE__);
    } else {
      std::printf("UI-SUBSET outside\n");
      std::printf("OUTSIDE %s -- undeclared: %s\n", id.c_str(), gap.c_str());
      outshine::Test::Checked(false, "every name that puts a case outside is declared",
                              (id + ": " + gap + " is outside the subset and nothing says why")
                                  .c_str(),
                              __FILE__, __LINE__);
    }
    return outshine::Test::Report();
  }

  std::printf("UI-SUBSET inside\n");

  int stated = 0;
  double worst = 0.0;

  const auto originOf = [&layout](int at) {
    double x = 0, y = 0;
    for (int up = layout.Boxes()[(size_t)at].Parent; up >= 0;
         up = layout.Boxes()[(size_t)up].Parent) {
      const Ui::Box &over = layout.Boxes()[(size_t)up];
      if (!over.Positioned) { continue; }
      x = over.X + over.Border.Left;
      y = over.Y + over.Top();
      break;
    }
    return std::pair<double, double>{x, y};
  };
  for (size_t boxAt = 0; boxAt < layout.Boxes().size(); ++boxAt) {
    const Ui::Box &box = layout.Boxes()[boxAt];
    if (box.Node < 0) { continue; }
    const std::pair<double, double> origin = originOf((int)boxAt);
    const std::string &element = markup.Nodes()[size_t(box.Node)].Name;
    for (const Assertion &assertion : kAssertions) {
      const std::string *declared = markup.AttributeOf(box.Node, assertion.Attribute);
      if (declared == nullptr) { continue; }
      ++stated;
      double want = 0;
      if (!NumberIn(*declared, want)) {
        const std::string claim = id + ": <" + element + "> " + assertion.Attribute + "=\"" +
                                  *declared + "\" is not a number";
        outshine::Test::Checked(false, "the declaration is a number", claim.c_str(), __FILE__,
                                __LINE__);
        continue;
      }
      const double got = std::strcmp(assertion.What, "width") == 0    ? box.Width
                         : std::strcmp(assertion.What, "height") == 0 ? box.Height
                         : std::strcmp(assertion.What, "x") == 0      ? box.X - origin.first
                                                                     : box.Y - origin.second;

      const bool integral = want == std::floor(want);
      const double residual = std::fabs(got - want);
      worst = std::fmax(worst, residual);
      const bool holds = integral ? std::floor(got + 0.5) == want : got == want;
      char measured[224];
      std::snprintf(measured, sizeof measured, "%s: <%s> %s is %.6f, the document states %.6f",
                    id.c_str(), element.c_str(), assertion.What, got, want);
      outshine::Test::Checked(holds, "the box lands where the document says", measured, __FILE__,
                              __LINE__);
    }
  }
  outshine::Test::Checked(stated > 0, "the document states its own layout",
                          (id + " carries at least one assertion").c_str(), __FILE__, __LINE__);
  std::printf("STATED %s %d assertions, worst sub-pixel residual %.6f px\n", id.c_str(), stated,
              worst);
  std::printf("UI-LAYOUT %s\n", outshine::Test::Failures.Value() == 0 ? "held" : "red");
  return outshine::Test::Report();
}
