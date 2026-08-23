#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include "Check.h"

#include "ScenarioRead.h"
#include "Views.h"

using outshine::ReadScenario;
using outshine::Scenario;
using outshine::ViewBook;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *text =
      "<scenario name=\"eyes\">"
      "<views>"
      "<view id=\"eyes\" follows=\"player\" person=\"first\" offsetY=\"1.7\" fovDeg=\"80\"/>"
      "<view id=\"chase\" follows=\"player\" person=\"third\" distanceM=\"6\"/>"
      "<view id=\"aimed\" follows=\"player\" person=\"first\" fovDeg=\"45\" "
      "timeScale=\"0.2\"/>"
      "</views>"
      "<player is=\"settler\" view=\"eyes\"/>"
      "</scenario>";
  Scenario declared;
  std::string error;
  CHECK(ReadScenario(text, std::strlen(text), declared, error), "the declaration reads");

  auto stood = ViewBook::Stand(declared.Views, declared.Played.View);
  if (!stood) { std::printf("REFUSED %s\n", stood.error().c_str()); }
  CHECK(stood.has_value(), "three declared views stand up");
  if (!stood) { return Report(); }
  ViewBook book = *std::move(stood);
  CHECK(book.ActiveId() == "eyes" && book.Count() == 3,
        "**EXACTLY ONE VIEW IS ACTIVE AND WHICH IS ANSWERABLE** -- the player's declared "
        "starting view");
  CHECK(book.Active().Follows == "player",
        "**A VIEW FOLLOWS AN INSTANCE** -- the camera is not a thing the client drives "
        "frame by frame");
  CHECK(book.ClockScale() == 1.0, "the unslowed view runs the clock at one");

  CHECK(book.Take("aimed") && book.ClockScale() == 0.2,
        "**THE AIMED VIEW SLOWS THE CLOCK, NOT THE FRAME**: timeScale reaches the world's "
        "advance while the frames keep landing");
  CHECK(book.ListensFrom() == "player",
        "**THE ACTIVE VIEW IS THE AUDIO LISTENER** -- one seat, no second declaration");
  CHECK(book.Take("chase") && book.Active().DistanceM == 6.0,
        "**SWITCHING COSTS NO STAND-UP** -- one relink of the active index, the way every "
        "aimed shot demands");
  CHECK(!book.Take("drone"), "a view nobody declared cannot be taken");

  {
    Scenario twice;
    const char *dup =
        "<scenario name=\"t\"><views>"
        "<view id=\"a\" follows=\"player\" person=\"first\"/>"
        "<view id=\"a\" follows=\"player\" person=\"third\"/>"
        "</views></scenario>";
    CHECK(ReadScenario(dup, std::strlen(dup), twice, error), "the duplicate reads");
    const auto duplicate = ViewBook::Stand(twice.Views, "");
    CHECK(!duplicate && duplicate.error().find("twice") != std::string::npos,
          "a view declared twice refuses -- taking it would be a coin toss");

    Scenario still;
    const char *frozen =
        "<scenario name=\"t\"><views>"
        "<view id=\"a\" follows=\"player\" person=\"first\" timeScale=\"0\"/>"
        "</views></scenario>";
    CHECK(ReadScenario(frozen, std::strlen(frozen), still, error), "the frozen one reads");
    const auto frozenBook = ViewBook::Stand(still.Views, "");
    CHECK(!frozenBook && frozenBook.error().find("timeScale") != std::string::npos,
          "a timeScale of zero refuses -- a clock runs forward or the scenario is a still");

    Scenario lost;
    const char *nowhere =
        "<scenario name=\"t\"><views>"
        "<view id=\"a\" follows=\"player\" person=\"first\"/></views>"
        "<player is=\"s\" view=\"b\"/></scenario>";
    CHECK(ReadScenario(nowhere, std::strlen(nowhere), lost, error), "the lost one reads");
    const auto lostBook = ViewBook::Stand(lost.Views, lost.Played.View);
    CHECK(!lostBook && lostBook.error().find("'b'") != std::string::npos,
          "a starting view nothing declares refuses naming it");
  }

  Covers("III.12 a view is one of several and time runs at its rate: one active view, "
         "answerable; a view follows an instance; timeScale is the clock's and not the "
         "frame's; the active view is the listener; switching is one index (board:1490)");
  return Report();
}
