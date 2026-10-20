#include "RoadPageConstruction.h"
#include "ProfiledRoadMesher.h"
#include "Sha256.h"
#include "Log.h"
#include <content/GeometryAsset.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
RoadPageConstruction::RoadPageConstruction(Tasks &pool,
                                           std::shared_ptr<Generators::PreparedRoadPages> pages)
    : Queue_(pool, std::move(pages), {.PendingMost = 1, .PageBytesMost = PageBytesMost}) {}

std::expected<std::unique_ptr<RoadPageConstruction>, std::string>
RoadPageConstruction::Begin(Tasks &pool,
                            std::shared_ptr<Generators::PreparedRoadPages> pages,
                            const Generators::Corridors::Job &plan,
                            const Generators::Corridors::Site &site,
                            const std::string &planKey) {
  if (!pages || planKey.size() != 64) {
    return std::unexpected("road construction needs its native cache and complete plan identity");
  }
  auto parts = Generators::Corridors::PlannedParts(plan);
  if (!parts) { return std::unexpected(std::string(parts.error())); }
  auto result =
      std::unique_ptr<RoadPageConstruction>(new RoadPageConstruction(pool, std::move(pages)));
  result->Parts_ = std::move(*parts);
  result->Material_ = Generators::Corridors::StreetMaterial(site.Materials);
  Geometry materials;
  if (!materials.addSurface("streets", result->Material_)) {
    return std::unexpected("could not prepare the shared road material");
  }
  const auto encoded = EncodeGeometryAsset(materials, 4096);
  if (!encoded) { return std::unexpected("could not identify the shared road material"); }
  result->Address_.PlanKey = Sha256Hex("world-road-pages-1\n" + planKey);
  result->Address_.MaterialKey = Sha256Hex(encoded->data(), encoded->size());
  result->Address_.Anchor = site.Standing.Anchor();
  return result;
}

std::expected<bool, std::string>
RoadPageConstruction::Advance(const Generators::Corridors::Job &plan,
                              const Generators::Corridors::Site &site,
                              Geometry &ground) {
  if (Stage_ == Stage::Lookup || Stage_ == Stage::Generating) { return Collect(ground); }
  if (Stage_ != Stage::Request && Stage_ != Stage::Generate) { return Transfer(ground); }
  if (Next_ == Parts_.size()) { return true; }
  const auto count = std::min(PartsMost, Parts_.size() - Next_);
  const auto requested = std::span(Parts_).subspan(Next_, count);
  if (Stage_ == Stage::Request) {
    Address_.Parts.assign(requested.begin(), requested.end());
    PendingKey_ = Generators::RoadPageKey(Address_);
    if (!Queue_.RequestCached(PendingKey_)) {
      return std::unexpected("road page lookup admission failed");
    }
    Stage_ = Stage::Lookup;
    return false;
  }
  auto recipe = Generators::Corridors::PageRecipe(plan, site, requested);
  if (!recipe) { return std::unexpected(std::string(recipe.error())); }
  const bool admitted =
      Queue_.Request(Address_,
                     {},
                     {},
                     [input = std::move(*recipe)](size_t, const std::stop_token &stop)
                         -> std::expected<Generators::RoadPageGeometry, std::string> {
                       const Generators::ProfiledRoadMesher mesher;
                       auto made = Generators::MeshRoadPage(mesher, input, stop);
                       if (!made) { return std::unexpected(std::string(made.error())); }
                       return std::move(*made);
                     });
  if (!admitted) { return std::unexpected("road page worker admission failed"); }
  ++MissRequests_;
  Stage_ = Stage::Generating;
  return false;
}

std::expected<bool, std::string> RoadPageConstruction::Collect(Geometry &ground) {
  auto completed = Queue_.Collect();
  if (completed.empty()) { return false; }
  auto &one = completed.front();
  if (completed.size() != 1 || one.Key != PendingKey_) {
    return std::unexpected("road page completion differs from the admitted world request");
  }
  if (!one.Result) { return std::unexpected(std::move(one.Result.error())); }
  if (!*one.Result) {
    if (Stage_ != Stage::Lookup) {
      return std::unexpected("generated road page is missing from its cache");
    }
    Stage_ = Stage::Generate;
    return false;
  }
  Loaded_ = std::move((**one.Result).Page);
  if (!Loaded_ || Generators::RoadPageKey(Loaded_->Address) != PendingKey_ ||
      Loaded_->Address.MaterialKey != Address_.MaterialKey || Loaded_->Address.MaterialIndex != 0) {
    return std::unexpected("road page references an unavailable material or different plan");
  }
  NativeBytes_ += (**one.Result).NativeBytes;
  Meshing_ += Loaded_->Geometry.Swept;
  for (const auto &diagnostic : Loaded_->Geometry.Diagnostics) {
    Log::Warn(
        LogTag::World,
        "road_page_part_rejected",
        {{"source", std::to_string(diagnostic.Source)},
         {"kind",
          diagnostic.Part.Kind == Generators::RoadPagePartKind::Interval ? "interval" : "junction"},
         {"part", static_cast<int64_t>(diagnostic.Part.Index)},
         {"reason", diagnostic.Empty ? "empty_native_mesh" : "partial_native_mesh"},
         {"refused", static_cast<int64_t>(diagnostic.Stats.Refused)}});
  }
  if (Loaded_->Geometry.Mesh.Index.empty()) {
    Next_ += Address_.Parts.size();
    ++PageCount_;
    Loaded_.reset();
    Stage_ = Stage::Request;
    return false;
  }
  if (!Surface_) {
    const auto surface = ground.addSurface("streets", Material_);
    if (!surface) { return std::unexpected("could not admit the shared road material"); }
    Surface_ = *surface;
  }
  const auto part = ground.addPart("streets", *Surface_);
  if (!part) { return std::unexpected("could not admit the native road page"); }
  Part_ = *part;
  Stage_ = Stage::Positions;
  return false;
}

std::expected<bool, std::string> RoadPageConstruction::Transfer(Geometry &ground) {
  const auto &mesh = Loaded_->Geometry.Mesh;
  bool copied = false;
  switch (Stage_) {
    case Stage::Positions:
      copied = ground.setPositions(Part_, mesh.PositionM).has_value();
      Stage_ = Stage::Normals;
      break;
    case Stage::Normals:
      copied = ground.setNormals(Part_, mesh.NormalM).has_value();
      Stage_ = Stage::Colours;
      break;
    case Stage::Colours:
      copied = ground.setColours(Part_, mesh.ColourRgba).has_value();
      Stage_ = Stage::Triangles;
      break;
    case Stage::Triangles:
      copied = ground.setTriangles(Part_, mesh.Index).has_value();
      Next_ += Address_.Parts.size();
      ++PageCount_;
      Loaded_.reset();
      Stage_ = Stage::Request;
      break;
    default: return std::unexpected("road page transfer is not active");
  }
  if (!copied) { return std::unexpected("could not transfer a native road page into the world"); }
  return false;
}
}
