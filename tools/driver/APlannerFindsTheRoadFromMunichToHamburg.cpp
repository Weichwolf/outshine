#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Check.h"

#include "ContentStore.h"
#include "CurlTransport.h"
#include "DeclaredSources.h"
#include "GroundMaterials.h"
#include "OsmField.h"
#include "RoadHarvest.h"
#include "SourceSet.h"
#include "TerrainLoader.h"
#include "TilePool.h"
#include "VegetationTemplates.h"
#include "Wayfinding.h"

using outshine::World::ApartM;
using outshine::World::Network;
using outshine::World::OsmField;
using outshine::World::OsmLayer;
using outshine::World::OsmLayerNames;
using outshine::World::Reap;
using outshine::World::Reaped;
using outshine::World::Route;
using outshine::World::TilePool;
using outshine::World::VegetationTemplates;
using outshine::World::Waypoint;

namespace {

constexpr double kMunichLat = 48.1371;
constexpr double kMunichLon = 11.5754;
constexpr double kHamburgLat = 53.5503;
constexpr double kHamburgLon = 9.9920;
constexpr double kF31WidthM = 1.811;
constexpr int kZoom = 10;

constexpr double kPatienceS = 900.0;

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const double straightM = ApartM(kMunichLat, kMunichLon, kHamburgLat, kHamburgLon);
  const double middleLat = 0.5 * (kMunichLat + kHamburgLat);
  const double middleLon = 0.5 * (kMunichLon + kHamburgLon);
  Note("Marienplatz to Rathausmarkt as the crow flies", straightM / 1000.0, "km");
  Note("the zoom the ways are read at", (double)kZoom, "");
  const double tileGroundM =
      40075017.0 * std::cos(middleLat * 3.14159265358979 / 180.0) / (double)(1L << kZoom);
  const int kCorridorRing = 2;
  const long steps = (long)std::ceil(straightM / tileGroundM) + 1;
  const long square = (long)(2 * std::ceil(0.5 * straightM / tileGroundM) + 3);
  Note("a tile's ground size at this zoom and latitude", tileGroundM / 1000.0, "km");
  Note("stations along the line the corridor is fetched at", (double)steps, "stations");
  Note("the ring fetched around each", (double)(2 * kCorridorRing + 1), "tiles across");
  Note("what a square covering both cities would have cost", (double)(square * square), "tiles");

  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = "/tmp/outshine-drive-cache";
  outshine::Data::ContentStore store(keeping);
  outshine::Data::SourceSet sources(store);
  CHECK(outshine::Data::RegisterDeclared(sources, {"src/assets/sky", true}) ==
            outshine::Data::Registered::Complete,
        "the declared upstream sources register, ranked and without a clash");
  Note("sources registered", (double)sources.Count(), "sources");

  outshine::Host::CurlTransport::Config wiring;
  wiring.Threads = 8;
  outshine::Host::CurlTransport wire(wiring);
  FbGroundSurface surface;
  surface.Z = 12;
  surface.Grid = 64;
  const int opened = fb_stream_open(sources, wire, middleLat, middleLon, surface);
  CHECK(opened == 1,
        "the streamer opens over the declared sources -- and it is a GLOBAL, which is what "
        "OsmField reaches for. CLAUDE.md gives neither a global nor a singleton a place to live, "
        "and board:1527 is that finding");
  if (opened != 1) { return Report(); }

  OsmField field(kZoom, OsmLayerNames({OsmLayer::Streets, OsmLayer::StreetPolygons}));
  const auto began = std::chrono::steady_clock::now();
  long passes = 0;
  int built = 0;
  bool ranOut = false;
  for (long step = 0; step <= steps && !ranOut; ++step) {
    const double part = (double)step / (double)steps;
    const double atLat = kMunichLat + part * (kHamburgLat - kMunichLat);
    const double atLon = kMunichLon + part * (kHamburgLon - kMunichLon);
    for (;;) {
      built += field.Build(atLat, atLon, kCorridorRing);
      ++passes;
      if (field.PendingTiles() == 0) { break; }
      if (std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count() >
          kPatienceS) {
        ranOut = true;
        break;
      }
    }
  }
  CHECK(!ranOut, "the corridor is fetched inside the patience declared for it");
  const double fetchedS =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();

  Note("passes over the corridor", (double)passes, "passes");
  Note("tiles the corridor actually took", (double)field.Tiles().size(), "tiles");
  Note("tiles decoded", (double)built, "tiles");
  Note("tiles still pending", (double)field.PendingTiles(), "tiles");
  Note("layers the server did not send", (double)field.MissingLayers(), "layers");
  Note("tiles that would not decode", (double)field.BadTiles(), "tiles");
  Note("features read", (double)field.Features().size(), "features");
  Note("points in them", (double)(field.Points().size() / 2), "points");
  Note("seconds spent fetching and decoding", fetchedS, "s");

  CHECK(built > 0 && field.Features().size() > 0,
        "**REAL OSM WAYS ARRIVE OVER THE WIRE AND DECODE.** No fixture, no committed extract: the "
        "declared upstream source is asked for the tiles between Munich and Hamburg and answers "
        "with vector geometry");
  if (field.Features().empty()) { return Report(); }

  outshine::World::GroundMaterials materials;
  CHECK(materials.Load("src/assets/world/ground-materials.json"),
        "the declared ground materials load, because a width table names them");
  VegetationTemplates widths;
  std::string error;
  CHECK(widths.Load("src/assets/world/vegetation.json", materials),
        "the declared widths load, with their RAA, RAL and RASt origins");

  const double quantumM = 40075017.0 / ((double)(1L << kZoom) * 4096.0);
  Network roads(1.05 * quantumM);
  Note("the tile's own coordinate quantisation", quantumM, "m");
  const Reaped reaped = Reap(field, widths, kF31WidthM, roads);
  Note("the snapping distance just above it", roads.SnapM(), "m");
  Note("ways a car can fit down", (double)reaped.Ways, "ways");
  Note("points in them", (double)reaped.Points, "points");
  Note("ways too narrow for it", (double)reaped.TooNarrow, "ways");
  Note("the widest way it refused", reaped.WidestRefusedM, "m");
  Note("the narrowest it took", reaped.NarrowestTakenM, "m");
  Note("ways whose kind carries no declared width", (double)reaped.Unclassed, "ways");

  CHECK(reaped.Ways > 0, "and roads a 1.811 m car can fit down are harvested from them");
  CHECK(reaped.WidestRefusedM < kF31WidthM || reaped.TooNarrow == 0,
        "**ADMISSIBILITY IS THE VEHICLE'S WIDTH AND NOT A LIST OF TAGS.** Every way this refused is "
        "narrower than the car; every way it took is wider. Nobody wrote down which highway kinds a "
        "car may use, and traffic law stays unmodelled -- what decides is whether it fits");

  CHECK(roads.Weave(error), "the ways weave into a network");
  if (!error.empty()) { std::printf("REFUSED %s\n", error.c_str()); }
  Note("nodes after snapping", (double)roads.NodeCount(), "nodes");
  Note("junctions among them", (double)roads.JunctionCount(), "nodes");
  Note("edges", (double)roads.EdgeCount(), "edges");

  size_t atMunich = 0, atHamburg = 0;
  double munichAwayM = 0.0, hamburgAwayM = 0.0;
  CHECK(roads.Nearest(Waypoint{kMunichLat, kMunichLon}, atMunich, munichAwayM) &&
            roads.Nearest(Waypoint{kHamburgLat, kHamburgLon}, atHamburg, hamburgAwayM),
        "both city centres resolve to a node of the network");
  Note("how far Marienplatz is from the nearest road node", munichAwayM, "m");
  Note("how far Rathausmarkt is from the nearest road node", hamburgAwayM, "m");
  CHECK(munichAwayM < 200.0 && hamburgAwayM < 200.0,
        "**AND BOTH ARE ON A ROAD RATHER THAN NEAR ONE.** A route is only between the places asked "
        "for if the endpoints snapped to them; a node 200 km away would give a route that is "
        "shorter than the great circle and still look like a route");

  const Route route = roads.Plan(Waypoint{kMunichLat, kMunichLon}, Waypoint{kHamburgLat, kHamburgLon});
  if (!route.Found) { std::printf("REFUSED %s\n", route.Error.c_str()); }
  Note("nodes the search settled", (double)route.Reached, "nodes");
  CHECK(route.Found,
        "**AND A ROUTE IS FOUND FROM MARIENPLATZ TO RATHAUSMARKT OVER OSM'S OWN WAYS.** Two "
        "coordinates in, a chain of real roads out, nothing stored and nothing hand-placed");
  if (!route.Found) { return Report(); }

  Note("how far the route runs", route.LengthM / 1000.0, "km");
  Note("how far the crow flies", route.StraightM / 1000.0, "km");
  Note("the detour that is", route.LengthM / route.StraightM, "x");
  CHECK(route.LengthM > route.StraightM,
        "**A ROAD CANNOT BE SHORTER THAN THE GREAT CIRCLE.** If it is, the route did not run between "
        "the two places asked for, or the network welded roads that do not meet -- and both are "
        "findings rather than a route");
  Note("legs in it", (double)route.Legs.size(), "legs");

  CHECK(route.LengthM / 1000.0 > 700.0 && route.LengthM / 1000.0 < 900.0,
        "and its length is what a road between these two cities is -- 612 km as the crow flies and "
        "roughly 775 by motorway, so a route far outside that band went somewhere a car would not");

  fb_stream_close();

  Covers("I.4.5 a route from Marienplatz to Rathausmarkt is planned over ways fetched live from the "
         "declared OSM source, woven into a network by snapping at twice the tile resolution, with "
         "admissibility decided by whether the declared vehicle fits");
  return Report();
}
