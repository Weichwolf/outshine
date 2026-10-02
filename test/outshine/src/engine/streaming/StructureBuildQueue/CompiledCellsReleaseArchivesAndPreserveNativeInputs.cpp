#include "Check.h"
#include "OsmSourceAcquisition.h"
#include "OsmStructureCell.h"
#include "OsmSourceCapture.h"
#include "SourceProviderValidation.h"
#include "StructureBuildQueue.h"
#include "ShippedProviders.h"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <atomic>
#include <chrono>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using Loader = Generators::Osm::SourceAcquisition;

class ApiWire final : public Data::Transport {
public:
  std::string Xml;
  std::atomic<int> Starts{0};

  Data::FetchStart Begin(const std::string &) override {
    return static_cast<Data::Ticket>(++Starts);
  }

  Data::Wire Collect(Data::Ticket) override {
    return Data::Wire::Answered(200, std::vector<uint8_t>(Xml.begin(), Xml.end()));
  }

  void Cancel(Data::Ticket) override {}
};

class TrackedCompiler final : public Generators::Osm::CellCompiler {
public:
  const std::shared_ptr<const Generators::Osm::CellCompiler> Native =
      Generators::Osm::MakeStructureCellCompiler({.Heights = {.StoreyHeightM = 3, .BodyHeightM = 9},
                                                  .PointWidthM = 2,
                                                  .PointsMost = 1024});
  mutable std::vector<std::weak_ptr<const Data::OsmSourceSnapshot>> Archives;
  mutable size_t RawBytes = 0;

  std::expected<std::shared_ptr<const Generators::Osm::CellProduct>, std::string>
  Compile(std::shared_ptr<const Data::OsmSourceSnapshot> source,
          const std::stop_token &stop) const override {
    Archives.push_back(source);
    RawBytes = source->StorageChargeBytes();
    return Native->Compile(std::move(source), stop);
  }
};

bool Wait(Loader &source) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (source.CurrentPhase() == Loader::Phase::Loading &&
         std::chrono::steady_clock::now() < until) {
    source.Poll();
    (void)source.AwaitSlice(0.01);
  }
  return source.CurrentPhase() == Loader::Phase::Ready;
}

std::string OriginalXml() {
  std::string xml = "<osm version='0.6'><node id='1' lat='0.01' lon='0.01'/>"
                    "<node id='2' lat='0.01' lon='0.011'/><node id='3' lat='0.011' lon='0.011'/>"
                    "<node id='4' lat='0.011' lon='0.01'/><way id='10'>"
                    "<nd ref='1'/><nd ref='2'/><nd ref='3'/><nd ref='4'/><nd ref='1'/>"
                    "<tag k='building' v='yes'/><tag k='height' v='12'/>"
                    "<tag k='custom:original' v='preserved'/></way>";
  for (int id = 100; id < 180; ++id) {
    xml += "<node id='" + std::to_string(id) +
           "' lat='0.02' lon='0.02'>"
           "<tag k='unconsumed:original' v='" +
           std::string(4096, 'x') + "'/></node>";
  }
  return xml + "</osm>";
}
}

int main() {
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-native-inputs-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "native input test owns an isolated source cache");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  Data::ProviderRegistry registry;
  Generators::RegisterShippedProviders(registry);
  Tasks compute(1), io(1);
  ApiWire wire;
  wire.Xml = OriginalXml();
  auto compiler = std::make_shared<TrackedCompiler>();
  Loader source({.Compute = compute, .Io = io}, &wire, directory, Loader::Target::Inputs, compiler);
  const Data::SourceProvider catalogue{.Kind = "osm",
                                       .Revision = "one",
                                       .Missing = Data::MissingDataPolicy::Fail,
                                       .Dataset = "original",
                                       .Endpoint = std::string(Data::kOfficialOsmApi)};
  const std::array<Data::GeoCellId, 4> cells{
      {{9, 256, 256}, {9, 257, 256}, {9, 258, 256}, {9, 259, 256}}};
  CHECK(
      source.RequestCells(catalogue, std::span(cells).first(1), {4, 1024 * 1024}, ".", &registry) &&
          Wait(source),
      "one source compiles through the actual bounded IO and compute pipeline");
  const size_t rawBytes = compiler->RawBytes;
  CHECK(rawBytes > 0 && source.RequestCells(catalogue, cells, {4, rawBytes}, ".", &registry) &&
            Wait(source),
        "four native cells fit within the measured retention cost of one full original archive");
  CHECK(source.CurrentCells().size() == 4 && source.CellSnapshotChargeBytes() <= rawBytes,
        "complete native coverage publishes within the unchanged retained byte bound");
  std::vector<std::shared_ptr<const Generators::Osm::StructureCell>> native;
  for (const auto &cell : source.CurrentCells()) {
    CHECK(!cell.Snapshot && cell.Product, "published products retain no complete original archive");
    if (cell.Product) {
      native.push_back(
          std::static_pointer_cast<const Generators::Osm::StructureCell>(cell.Product));
    }
  }
  for (const auto &archive : compiler->Archives) {
    CHECK(archive.expired(), "decoded archives retire after their consumers finish");
  }
  if (native.size() != cells.size()) { return Report(); }
  const auto &capture =
      static_cast<const Generators::Osm::SourceCapture &>(*native.front()->Description.Source);
  const auto *building = capture.Snapshot().Elements.FindWay(10);
  CHECK(building && building->Tags.size() == 3 &&
            std::ranges::any_of(building->Tags,
                                [](const auto &tag) {
                                  return tag.Key == "custom:original" && tag.Value == "preserved";
                                }) &&
            !capture.Snapshot().Elements.FindNode(100),
        "consumed closure preserves original tags without pinning unrelated source objects");
  StructureBuildQueue queue;
  queue.Opens(&compute, nullptr);
  bool ready = false;
  for (int attempt = 0; attempt < 100 && !ready; ++attempt) {
    const auto prepared = queue.PrepareOriginal(native, 14);
    CHECK(prepared, "native cells enter the runtime structure preparation contract");
    if (!prepared) { return Report(); }
    ready = *prepared;
    if (!ready) { (void)queue.AwaitSlice(0.01); }
  }
  const auto heights = queue.OriginalHeightTiles(14);
  CHECK(ready && heights && !heights->empty(),
        "closed native footprints produce real terrain demand");
  ApiWire changedWire;
  changedWire.Xml = wire.Xml;
  const auto value = changedWire.Xml.find("v='" + std::string(16, 'x'));
  CHECK(value != std::string::npos, "independent conflict changes an unconsumed original tag");
  if (value == std::string::npos) { return Report(); }
  changedWire.Xml[value + 3] = 'y';
  Loader changedSource({.Compute = compute, .Io = io},
                       &changedWire,
                       directory + "/changed",
                       Loader::Target::Inputs,
                       compiler);
  CHECK(changedSource.RequestCells(
            catalogue, std::span(cells).last(1), {4, rawBytes}, ".", &registry) &&
            Wait(changedSource),
        "changed original source independently compiles through the same registered pipeline");
  if (changedSource.CurrentCells().empty()) { return Report(); }
  native.back() = std::static_pointer_cast<const Generators::Osm::StructureCell>(
      changedSource.CurrentCells().front().Product);
  CHECK(native.back()->Description.Footprints.LatLon ==
            native.front()->Description.Footprints.LatLon,
        "unconsumed conflict leaves consumed building geometry identical");
  CHECK(!Generators::Osm::VerifyStructureCells(native, {}),
        "contradictory unconsumed originals still reject the whole candidate after archive "
        "retirement");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  return Report();
}
