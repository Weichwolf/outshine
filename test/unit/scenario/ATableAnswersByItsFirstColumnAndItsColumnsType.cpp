#include <cstdio>
#include <cstring>
#include <string>

#include "Check.h"

#include "ScenarioRead.h"
#include "Tables.h"

using outshine::ReadScenario;
using outshine::Scenario;
using outshine::TableBook;

namespace {

[[nodiscard]] bool Stood(const char *text, TableBook &book, std::string &error) {
  Scenario declared;
  if (!ReadScenario(text, std::strlen(text), declared, error)) { return false; }
  return book.Build(declared.Tables, error);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;
  TableBook book;
  const bool up = Stood(
      "<scenario name=\"balance\"><tables><table id=\"weapons\">"
      "<column name=\"name\"/><column name=\"damage\" type=\"number\"/>"
      "<column name=\"grip\"/>"
      "<row><cell value=\"sword\"/><cell value=\"13\"/><cell value=\"13\"/></row>"
      "<row><cell value=\"bow\"/><cell value=\"7.5\"/><cell value=\"two-handed\"/></row>"
      "</table></tables></scenario>",
      book, error);
  if (!up) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(up, "a declared table stands up once");
  if (!up) { return Report(); }

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
    TableBook bad;
    CHECK(!Stood("<scenario name=\"t\"><tables><table id=\"w\">"
                 "<column name=\"k\"/><column name=\"d\" type=\"number\"/>"
                 "<row><cell value=\"a\"/><cell value=\"tall\"/></row>"
                 "</table></tables></scenario>",
                 bad, error) &&
              error.find("tall") != std::string::npos,
          "a cell that does not read as its column's number refuses at stand-up, naming the "
          "cell and the column");
  }
  {
    TableBook bad;
    CHECK(!Stood("<scenario name=\"t\"><tables><table id=\"w\"><column name=\"k\"/>"
                 "<row><cell value=\"a\"/></row><row><cell value=\"a\"/></row>"
                 "</table></tables></scenario>",
                 bad, error) &&
              error.find("two answers") != std::string::npos,
          "two rows under one key refuse -- a lookup with two answers has none");
  }
  {
    TableBook bad;
    CHECK(!Stood("<scenario name=\"t\"><tables><table id=\"w\">"
                 "<column name=\"k\" type=\"maybe\"/></table></tables></scenario>",
                 bad, error) &&
              error.find("maybe") != std::string::npos,
          "and a third cell type does not exist");
  }

  Covers("III.8 a table is declared data: rows keyed by the first column at stand-up, cells "
         "typed by their declared column, nothing answered as zero, the bound declared -- "
         "the script host reads this book when board:1448's surface lands (board:1489)");
  return Report();
}
