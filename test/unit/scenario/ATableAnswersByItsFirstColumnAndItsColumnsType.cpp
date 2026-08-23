#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <string>
#include <utility>

#include "Check.h"

#include "ScenarioRead.h"
#include "Tables.h"

namespace {
size_t gAllocations = 0;
}
void *operator new(size_t bytes) {
  ++gAllocations;
  void *held = std::malloc(bytes == 0 ? 1 : bytes);
  if (held == nullptr) { std::abort(); }
  return held;
}
void *operator new[](size_t bytes) { return operator new(bytes); }
void operator delete(void *held) noexcept { std::free(held); }
void operator delete[](void *held) noexcept { std::free(held); }
void operator delete(void *held, size_t) noexcept { std::free(held); }
void operator delete[](void *held, size_t) noexcept { std::free(held); }

using outshine::ReadScenario;
using outshine::Scenario;
using outshine::TableBook;

namespace {

[[nodiscard]] std::expected<TableBook, std::string> Stood(const char *text) {
  Scenario declared;
  std::string refusal;
  if (!ReadScenario(text, std::strlen(text), declared, refusal)) {
    return std::unexpected(refusal);
  }
  return TableBook::Stand(declared.Tables);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  auto stood = Stood(
      "<scenario name=\"balance\"><tables><table id=\"weapons\">"
      "<column name=\"name\"/><column name=\"damage\" type=\"number\"/>"
      "<column name=\"grip\"/>"
      "<row><cell value=\"sword\"/><cell value=\"13\"/><cell value=\"13\"/></row>"
      "<row><cell value=\"bow\"/><cell value=\"7.5\"/><cell value=\"two-handed\"/></row>"
      "</table></tables></scenario>");
  if (!stood) { std::printf("REFUSED %s\n", stood.error().c_str()); }
  CHECK(stood.has_value(), "a declared table stands up once");
  if (!stood) { return Report(); }
  const TableBook book = *std::move(stood);

  const double *damage = book.Number("weapons", "sword", "damage");
  CHECK(damage != nullptr && *damage == 13.0,
        "**A ROW IS LOOKED UP BY ITS FIRST COLUMN** and the cell reads as the NUMBER its "
        "column declares");
  const std::string *grip = book.Text("weapons", "sword", "grip");
  CHECK(grip != nullptr && *grip == "13",
        "**A CELL IS TYPED BY ITS COLUMN AND NOT BY ITS SPELLING**: the same '13' is text "
        "where the column says text");
  CHECK(book.Number("weapons", "axe", "damage") == nullptr,
        "**A LOOKUP THAT FINDS NOTHING ANSWERS SO** -- a missing damage row and a damage of "
        "zero are different facts, and the answer is null, never 0");
  CHECK(book.Number("weapons", "sword", "grip") == nullptr,
        "and asking a text column for a number answers nothing rather than a guess");

  {
    const auto bad = Stood("<scenario name=\"t\"><tables><table id=\"w\">"
                 "<column name=\"k\"/><column name=\"d\" type=\"number\"/>"
                 "<row><cell value=\"a\"/><cell value=\"tall\"/></row>"
                 "</table></tables></scenario>");
    CHECK(!bad && bad.error().find("tall") != std::string::npos,
          "a cell that does not read as its column's number refuses at stand-up, naming the "
          "cell and the column");
  }
  {
    const auto bad = Stood("<scenario name=\"t\"><tables><table id=\"w\"><column name=\"k\"/>"
                 "<row><cell value=\"a\"/></row><row><cell value=\"a\"/></row>"
                 "</table></tables></scenario>");
    CHECK(!bad && bad.error().find("two answers") != std::string::npos,
          "two rows under one key refuse -- a lookup with two answers has none");
  }
  {
    const auto bad = Stood("<scenario name=\"t\"><tables><table id=\"w\">"
                 "<column name=\"k\" type=\"maybe\"/></table></tables></scenario>");
    CHECK(!bad && bad.error().find("maybe") != std::string::npos,
          "and a third cell type does not exist");
  }

  // board:1489: every lookup built a std::string from its string_view argument just to
  // reach an unordered_map keyed by std::string -- two allocations per ask, on a door whose
  // whole point is that a scenario reads numbers out of declared data.
  {
    const size_t before = gAllocations;
    double sum = 0.0;
    for (int at = 0; at < 1000; ++at) {
      const double *asked = book.Number("weapons", "sword", "damage");
      const std::string *held = book.Text("weapons", "sword", "grip");
      sum += asked != nullptr ? *asked : 0.0;
      sum += held != nullptr ? (double)held->size() : 0.0;
    }
    const size_t spent = gAllocations - before;
    Note("lookups made", 2000.0, "lookups");
    Note("allocations they cost", (double)spent, "allocations");
    CHECK(sum > 0.0, "the lookups answered");
    CHECK(spent == 0,
          "**A LOOKUP BY string_view ALLOCATES NOTHING**: the table is keyed for the type the "
          "door takes, so asking a declared table for a number costs no heap at all "
          "(board:1489)");
  }

  Covers("III.8 a table is declared data: rows keyed by the first column at stand-up, cells "
         "typed by their declared column, nothing answered as zero, the bound declared -- "
         "the script host reads this book when board:1448's surface lands (board:1489)");
  return Report();
}
