#include "TestEngine.h"
#include "OsmSourceAcquisition.h"
#include "OsmProvider.h"
#include "OfflineSourceTransport.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <thread>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-probe-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated source directory created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const auto path = directory + "/source.osm";
  {
    std::ofstream file(path);
    file << R"(<osm version="0.6"><node id="1" lat="0.5" lon="0.5"/></osm>)";
    file.close();
    CHECK(!file.fail(), "local original source written completely");
  }
  {
    const Generators::Osm::Provider provider;
    Engine engine;
    CHECK(engine.registerProvider(provider) && engine.declare({}) && engine.assemble(),
          "source-free engine declares and assembles without IO");
    auto &state = TestEngine::StateOf(engine);
    auto &world = state.World;
    auto &services = state.Services;
    services.Compute = std::make_unique<Tasks>(1);
    services.Io = std::make_unique<Tasks>(1);
    services.Transport = std::make_unique<Data::OfflineSourceTransport>();
    world.OsmSource = std::make_unique<Generators::Osm::SourceAcquisition>(
        Generators::Osm::SourceAcquisition::Workers{.Compute = *services.Compute,
                                                    .Io = *services.Io},
        services.Transport.get(),
        directory);
    CHECK(engine.preload(0.0), "zero-time readiness probe succeeds for the assembled empty world");
    Data::SourceProvider declaration;
    declaration.Kind = "osm";
    declaration.Missing = Data::MissingDataPolicy::Fail;
    declaration.Dataset = "preload-probe";
    declaration.Revision = "first";
    declaration.Location = path;
    declaration.Coverage =
        GeographicBounds{.WestDeg = 0, .SouthDeg = 0, .EastDeg = 1, .NorthDeg = 1};
    const std::array declarations{declaration};
    const auto requested = world.OsmSource->Request(declarations, {}, &services.Providers);
    CHECK(requested,
          requested ? "a subsequent local source request starts normally"
                    : requested.error().c_str());
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (world.OsmSource->CurrentPhase() == Generators::Osm::SourceAcquisition::Phase::Loading &&
           std::chrono::steady_clock::now() < until) {
      world.OsmSource->Poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(world.OsmSource->CurrentPhase() == Generators::Osm::SourceAcquisition::Phase::Ready &&
              world.OsmSource->Current() != nullptr,
          "readiness probing does not expire the following acquisition");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated source inputs removed");
  return Report();
}
