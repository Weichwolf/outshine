/* THE HARNESS FOR web-platform-tests' own CSS tests, fetched at the pin every manifest cites
 * (board:1444).
 *
 * WHAT DECIDES A CASE HERE IS THE DOCUMENT ITSELF, and that is what makes this suite harder than the
 * picture suite rather than softer. Upstream writes `data-expected-width`, `data-expected-height`,
 * `data-offset-x` and `data-offset-y` on the elements it means, in CSS pixels against the viewport's
 * own origin, and the verdict is every one of them EXACTLY. There is no threshold here, so there is
 * none to widen -- the ladder's rungs are fix the engine, reduce the case, patch the asset,
 * disqualify, and the first is the only one that moves a number.
 *
 * TWO COUNTS AND NEITHER STANDS FOR THE OTHER (board:1444). `HELD` counts cases whose every assertion
 * landed; `OUTSIDE` counts cases whose declaration reaches past the subset this engine holds. A suite
 * that reported only the first would improve by shrinking, which is the defect the pair exists to
 * prevent: the second is what says how much of the corpus the engine has actually arrived at.
 *
 * THE SELECTION IS DERIVED AND NEVER CURATED. Every test in the pinned directory that states its own
 * layout is a case; whether this engine can hold it is decided HERE, by the subset counters the
 * stylesheet reader publishes, and never by a `grep` for a property name -- a grep reads a shorthand
 * it cannot expand and a selector it cannot parse, and it would answer a different question. */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Check.h"
#include "Json.h"
#include "Layout.h"
#include "Markup.h"
#include "Style.h"

namespace {

using outshine::Json;
namespace Ui = outshine::Ui;

/* THE ATTRIBUTES THE CORPUS STATES ITS OWN LAYOUT WITH, and the whole list. `data-expected-client-*`
 * and the scroll family are upstream's too and are NOT read: they are about a box this engine does not
 * model yet, and reading them as if they were the border box would report a number about the wrong
 * quantity -- the failure that wears four faces in `CLAUDE.md`, here the one called domain too narrow. */
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

/* A number as the corpus writes it: an integer, or a decimal upstream states to the pixel. */
bool NumberIn(const std::string &text, double &value) {
  char *end = nullptr;
  value = std::strtod(text.c_str(), &end);
  return end != nullptr && end != text.c_str() && *end == '\0';
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    outshine::Test::Checked(false, "argc == 2", "one prepared case directory is the argument",
                            __FILE__, __LINE__);
    return outshine::Test::Report();
  }
  /* THE ARGUMENT IS THE PREPARED DIRECTORY AND THE DECLARATION IS IN IT. `test/run.sh` hands the
   * runner the case's prepared root, and the preparer copies the manifest beside what it fetched --
   * so a case is one directory to this program, the same way it is to the picture suite. */
  const std::string prepared = argv[1];

  bool found = false;
  const std::string manifestText = ReadFile(prepared + "/manifest.json", found);
  if (!found) {
    outshine::Test::Unprepared(prepared.c_str());
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
    /* A DOCUMENT THIS READER REFUSES IS OUTSIDE THE SUBSET AND NOT A FAILED CASE. The refusal is the
     * library's contract with a consumer who writes a declaration; upstream's corpus is not that
     * consumer, and counting its stray end tag as a layout defect would put a markup question into a
     * layout number (board:1445). What it may never be is silent, so the refusal is printed with the case. */
    std::printf("UI-SUBSET outside\n");
    std::printf("OUTSIDE %s -- the document is outside this reader: %s\n", id.c_str(),
                error.c_str());
    outshine::Test::Checked(!error.empty(), "the reader says why it refused",
                            (id + ": " + error).c_str(), __FILE__, __LINE__);
    return outshine::Test::Report();
  }

  /* THE SHEET IS THE UA SHEET, THEN EVERY FILE THE DOCUMENT LINKS, THEN ITS OWN `<style>` -- which is
   * the cascade's own order, so a later rule of equal specificity wins as CSS says it does. */
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

  /* WHETHER THIS CASE IS THE ENGINE'S TO HOLD, decided by the reader that would have to hold it. A
   * case outside the subset is neither a pass nor a failure: it is the second count, and it is
   * announced by name so the run says which capability would take it in. */
  /* THE QUESTION IS ASKED AFTER THE LAYOUT AND NOT BEFORE IT, because an element's own `style`
   * attribute is read during the build and a sheet cannot report what it has not been given yet.
   * [MEASURED] asking first put `writing-mode` cases INSIDE the subset -- the property is written
   * inline in those documents, and an error in the coverage number's favour is the one direction it
   * must never be wrong in. */
  /* A LAYOUT A SCRIPT DECIDES IS OUTSIDE THIS ENGINE, AND THAT IS A DECISION RATHER THAN A GAP. This
   * is a mechanism and not a browser: a consumer declares a surface and the library measures it, and
   * nothing here will ever run a program the document carries. [MEASURED] `percentage-heights-011`
   * states `data-expected-height="100"` on a box whose height is assigned by `outer.style.height =
   * "100px"` in the document's own `window.onload`, so the number it states is about a tree this
   * engine is never given.
   *
   * THE RULE IS DERIVED AND NOT A JUDGEMENT ABOUT WHAT A SCRIPT DOES. Upstream's own harness arrives
   * as `<script src>` and the static form of a case calls it from a `body` attribute; a case that
   * carries an inline script BODY runs code before the layout is asked about. Reading the code to
   * decide whether it matters is the heuristic this refuses to be. */
  const std::string scripted =
      markup.CarriesAScript() ? "a script in the document decides this layout" : "";
  const size_t outsideProperties = sheet.PropertiesOutsideTheSubset();
  const size_t outsideSelectors = sheet.SelectorsOutsideTheSubset();
  const std::vector<std::string> outsideElements = Ui::ElementsOutsideTheSubset(markup);
  if (outsideProperties + outsideSelectors + outsideElements.size() > 0 || !scripted.empty()) {
    std::string names;
    for (const std::string &name : sheet.NamesOutsideTheSubset()) {
      if (!names.empty()) { names += " "; }
      names += name;
    }
    for (const std::string &name : outsideElements) {
      if (!names.empty()) { names += " "; }
      names += "<" + name + ">";
    }
    if (!scripted.empty()) {
      if (!names.empty()) { names += " "; }
      names += scripted;
    }
    std::printf("UI-SUBSET outside\n");
    std::printf("OUTSIDE %s -- %zu properties, %zu selectors and %zu elements this engine does not "
                "hold: %s\n",
                id.c_str(), outsideProperties, outsideSelectors, outsideElements.size(),
                names.c_str());
    /* THE CLAIM A CASE OUTSIDE THE SUBSET STILL MAKES, and it is checkable rather than waved through:
     * the reader NAMED what it could not hold. A counter that went up with an empty list would be a
     * case quietly dropped, which is the one thing the second count exists to make visible. */
    outshine::Test::Checked(!names.empty(), "the reader names what it drops",
                            (id + " reaches past the subset and says which names").c_str(), __FILE__,
                            __LINE__);
    return outshine::Test::Report();
  }

  std::printf("UI-SUBSET inside\n");

  /* EVERY ASSERTION THE DOCUMENT STATES, AND A DOCUMENT THAT STATES NONE IS A FAILURE. A case that
   * measured nothing and reported a pass is the empty-renderer fixed point wearing the suite's own
   * colours, so the count is checked before the values are. */
  int stated = 0;
  double worst = 0.0;
  for (const Ui::Box &box : layout.Boxes()) {
    if (box.Node < 0) { continue; }
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
                         : std::strcmp(assertion.What, "x") == 0      ? box.X
                                                                     : box.Y;
      /* THE DOCUMENT QUOTES THE PROPERTY IT READS, AND THAT PROPERTY IS AN INTEGER. `check-layout`
       * compares `offsetLeft`, `offsetTop`, `offsetWidth` and `offsetHeight`, every one of which CSSOM
       * defines as a rounded value -- so a document stating `61` is stating what the browser rounded
       * `61.333` to, and comparing an unrounded double against it measures a different quantity. This
       * is the failure `CLAUDE.md` calls *the number was right and about something else*, and it is
       * NOT a widened bound: a bound this suite could widen does not exist.
       *
       * **THE SUB-PIXEL RESIDUAL IS PUBLISHED SO THE ROUNDING CANNOT HIDE A DRIFT.** An engine that
       * was wrong by 0.49 px everywhere would pass every one of these, and the number below is what
       * would show it. A fractional expectation is compared EXACTLY, because a document that wrote one
       * is quoting something else. */
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
