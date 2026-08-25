#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include <outshine/Transport.h>
#include "WebTileSource.h"

using outshine::Data::Address;
using outshine::Data::Coverage;
using outshine::Data::DataKind;
using outshine::Data::Fetched;
using outshine::Data::Meaning;
using outshine::Data::Request;
using outshine::Data::Scheme;
using outshine::Data::SourceDecl;
using outshine::Data::Ticket;
using outshine::Data::Transport;
using outshine::Data::WebTileSource;
using outshine::Data::Wire;

namespace {

// board:1806: WebTileSource was drawn in the CURRENT class map and named by nothing under
// test/. It is the base every web-fetched tile source stands on -- Terrarium elevation and
// Versatiles vector both derive from it -- and the two decisions it owns are pure functions of
// a declaration: whether a request is covered at all, and which tile actually gets fetched when
// the request is deeper than the source goes.
class Probe final : public WebTileSource {
public:
  explicit Probe(SourceDecl decl) : WebTileSource(std::move(decl)) {}

  [[nodiscard]] const std::vector<std::string> &Asked() const { return Asked_; }
  void Answers(int status, size_t bytes) {
    Status_ = status;
    Bytes_ = bytes;
  }

protected:
  [[nodiscard]] std::string Url(const Address &at) const override {
    Asked_.push_back(at.Text());
    return "https://tiles.invalid/" + at.Text();
  }
  [[nodiscard]] Meaning Classify(int status, size_t bytes) const noexcept override {
    return status == 200 && bytes > 0 ? Meaning::Bytes : Meaning::Refused;
  }

private:
  mutable std::vector<std::string> Asked_;
  int Status_ = 200;
  size_t Bytes_ = 1;
};

[[nodiscard]] SourceDecl Declared(int minZoom, int maxZoom, bool ancestorFill) {
  SourceDecl decl;
  decl.Id = "probe";
  decl.Kind = DataKind::Elevation;
  decl.How = Scheme::TileZxy;
  decl.MinZoom = minZoom;
  decl.MaxZoom = maxZoom;
  decl.AncestorFill = ancestorFill;
  return decl;
}

[[nodiscard]] bool ServedTile(const WebTileSource &source, int z, uint32_t x, uint32_t y,
                              int &outZ, uint32_t &outX, uint32_t &outY) {
  const Request asked(DataKind::Elevation, Address::Tile(z, x, y));
  return source.Serves(asked).TryTile(&outZ, &outX, &outY);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Probe filling(Declared(2, 12, true));
  const Probe plain(Declared(2, 12, false));

  Note("the zooms the probe declares", 12.0 - 2.0, "levels, 2 to 12");

  // 1. coverage is the declaration, and nothing else.
  const struct {
    int Z;
    uint32_t X, Y;
    Coverage Filling, Plain;
    const char *Why;
  } kAsked[] = {
      {2, 0, 0, Coverage::Inside, Coverage::Inside, "the shallowest declared zoom"},
      {12, 4095, 4095, Coverage::Inside, Coverage::Inside, "the deepest, at its last tile"},
      {1, 0, 0, Coverage::Outside, Coverage::Outside, "one zoom shallower than declared"},
      {13, 0, 0, Coverage::Inside, Coverage::Outside, "one deeper -- ancestor fill decides"},
      {30, 0, 0, Coverage::Inside, Coverage::Outside, "the deepest tile zoom there is"},
      {31, 0, 0, Coverage::Outside, Coverage::Outside, "past every tile zoom"},
      {2, 4, 0, Coverage::Outside, Coverage::Outside, "x past the side of its own zoom"},
      {2, 0, 4, Coverage::Outside, Coverage::Outside, "y past the side of its own zoom"},
  };
  size_t agreed = 0;
  for (const auto &one : kAsked) {
    const Request asked(DataKind::Elevation, Address::Tile(one.Z, one.X, one.Y));
    const Coverage filled = filling.Covers(asked);
    const Coverage bare = plain.Covers(asked);
    std::printf("NOTE z%-3d x%-5u y%-5u  fill:%-8s plain:%-8s  %s\n", one.Z, one.X, one.Y,
                filled == Coverage::Inside ? "inside" : "outside",
                bare == Coverage::Inside ? "inside" : "outside", one.Why);
    agreed += (filled == one.Filling && bare == one.Plain) ? 1u : 0u;
  }
  Note("rows where coverage was what the declaration implies", (double)agreed,
       "of 8");
  CHECK(agreed == 8,
        "**COVERAGE IS THE DECLARATION AND NOT A HABIT**: the zoom band, the tile grid's own "
        "side at each zoom, and whether the source fills from an ancestor are all read from "
        "SourceDecl -- a source that answered outside them would be answering for data it was "
        "never declared to hold (board:1806)");

  // 2. a request deeper than the source goes is SERVED by the ancestor that contains it, and
  // the arithmetic is one shift per level rather than a lookup.
  int z = 0;
  uint32_t x = 0, y = 0;
  CHECK(ServedTile(filling, 15, 17000, 11000, z, x, y), "a deep request resolves to a tile");
  std::printf("NOTE z15 x17000 y11000 is served by z%d x%u y%u\n", z, x, y);
  CHECK(z == 12 && x == (17000u >> 3) && y == (11000u >> 3),
        "**AND A REQUEST DEEPER THAN THE SOURCE GOES IS SERVED BY THE TILE THAT CONTAINS IT**: "
        "three levels up is three shifts, so the ancestor is computed rather than searched, and "
        "the caller gets bytes that really cover the place it asked about");

  CHECK(ServedTile(filling, 12, 4095, 4095, z, x, y) && z == 12 && x == 4095u && y == 4095u,
        "and a request AT the deepest declared zoom is served by itself, untouched");
  CHECK(ServedTile(filling, 5, 17, 9, z, x, y) && z == 5 && x == 17u && y == 9u,
        "and a shallower one likewise -- the ancestor walk only ever climbs");

  // 3. the fetch path: the url is asked for the SERVED address, and what comes back is
  // classified by the derived source rather than by this base.
  const Address deep = filling.Serves(Request(DataKind::Elevation, Address::Tile(15, 17000, 11000)));
  Note("addresses the probe was asked to spell before the fetch", (double)filling.Asked().size(),
       "urls");
  CHECK(filling.Asked().empty(),
        "and nothing spelled a url while only coverage was being asked -- a question about "
        "whether a source COULD serve must not reach the network");
  Note("the address the fetch would use", (double)z, "");
  CHECK(deep.Text() == Address::Tile(12, 17000u >> 3, 11000u >> 3).Text(),
        "and the address the fetch would use is the ancestor, not the request -- so the store "
        "keys one tile where a naive source would key eight");

  Covers("I.19.4 a web tile source answers from its declaration alone: the zoom band and the "
         "tile grid decide coverage, a request deeper than the source goes is served by the "
         "ancestor containing it, and asking whether it covers a place spells no url "
         "(board:1806)");
  return Report();
}
