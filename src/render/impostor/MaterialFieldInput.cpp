#include "MaterialFieldInputData.h"
#include "MaterialResolution.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace outshine::Render {
MaterialFieldInput::MaterialFieldInput(std::unique_ptr<Data> data) noexcept
    : Data_(std::move(data)) {}

MaterialFieldInput::~MaterialFieldInput() = default;

std::expected<std::shared_ptr<const MaterialFieldInput>, std::string>
MaterialFieldInput::Prepare(Geometry source, Extent raster) {
  if (raster.WidthPx <= 0 || raster.HeightPx <= 0) {
    return std::unexpected("material field raster must be positive");
  }
  auto data = std::make_unique<Data>();
  data->Source = std::make_shared<Geometry>(std::move(source));
  data->Raster = raster;
  if (auto ready = data->Prepare(); !ready) { return std::unexpected(std::move(ready.error())); }
  return std::shared_ptr<const MaterialFieldInput>(new MaterialFieldInput(std::move(data)));
}

std::expected<void, std::string> MaterialFieldInput::Data::Prepare() {
  if (auto prepared = PrepareRenderGeometryStorage(Source, Storage); !prepared) {
    return std::unexpected("material field geometry cannot be prepared");
  }
  if (auto bound = BindRenderGeometryStorage(Storage); !bound) {
    return std::unexpected("material field geometry has invalid attribute storage");
  }
  View = MakeRenderGeometryView(Storage);
  if (View.Empty() || View.VertexCount() > std::numeric_limits<uint32_t>::max() / 3u ||
      View.Indices.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("material field geometry is empty or exceeds native addressing");
  }
  Materials.resize(View.Surfaces.size());
  for (size_t at = 0; at < Materials.size(); ++at) { Materials[at].Row = View.Surfaces[at]; }
  std::string error;
  if (!ResolveNativeTextures(*Source, Materials, error)) {
    return std::unexpected(std::move(error));
  }
  if (auto textures = PrepareTextures(); !textures) { return textures; }
  for (const auto &part : View.Parts) {
    if (part.IndexCount == 0) { continue; }
    if (part.Material < 0 || static_cast<size_t>(part.Material) >= Materials.size()) {
      return std::unexpected("material field part has no declared material");
    }
    VertexLayout layout;
    if (!LayoutOf({.Uv = part.HasUv,
                   .Uv1 = part.HasUv1,
                   .Normal = part.HasNormal,
                   .Tangent = part.HasTangent,
                   .Colour = part.HasColour},
                  layout)) {
      return std::unexpected("material field has an unsupported vertex layout");
    }
    const auto &row = Materials[static_cast<size_t>(part.Material)];
    if (!Draws.Add({.Order = {.Surface = row.State(),
                              .MaterialSlot = static_cast<uint32_t>(part.Material)},
                    .SourceFirstIndex = static_cast<uint32_t>(part.FirstIndex),
                    .IndexCount = static_cast<uint32_t>(part.IndexCount),
                    .Layout = layout},
                   error)) {
      return std::unexpected(std::move(error));
    }
  }
  Draws.Compile();
  Indices.reserve(View.Indices.size());
  for (const auto &run : Draws.Runs()) {
    const auto range = View.Indices.subspan(run.SourceFirst, run.Count);
    Indices.insert(Indices.end(), range.begin(), range.end());
  }
  Channels = {{{.From = &View, .Channel = &RenderGeometryPart::PositionsM, .Wide = 3},
               {.From = &View, .Channel = &RenderGeometryPart::Normals, .Wide = 3},
               {.From = &View, .Channel = &RenderGeometryPart::Tangents, .Wide = 4},
               {.From = &View, .Channel = &RenderGeometryPart::Uv, .Wide = 2},
               {.From = &View, .Channel = &RenderGeometryPart::Uv1, .Wide = 2},
               {.From = &View, .Channel = &RenderGeometryPart::Colours, .Wide = 4}}};
  Positions.resize(View.VertexCount() * 3);
  GeometryChannel::Write(
      Channels.data(), Positions.data(), static_cast<uint32_t>(Positions.size()));
  return {};
}

void MaterialFieldInput::Data::Emit(const void *carrying, float *into, uint32_t floats) {
  const auto &data = *static_cast<const Data *>(carrying);
  uint32_t at = 0;
  for (const auto &part : data.View.Parts) {
    const Material *row =
        part.Material >= 0 && static_cast<size_t>(part.Material) < data.Materials.size()
            ? &data.Materials[static_cast<size_t>(part.Material)].Row
            : nullptr;
    for (size_t vertex = 0; vertex < part.VertexCount && at + 3u <= floats; ++vertex) {
      for (size_t channel = 0; channel < 3; ++channel) {
        into[at++] = row != nullptr && row->Unlit ? row->BaseColour[channel] : 0;
      }
    }
  }
  if (at < floats) { std::memset(into + at, 0, size_t{floats - at} * sizeof(float)); }
}

MeshInput MaterialFieldInput::Data::Mesh() const {
  MeshInput mesh;
  mesh.Verts.From = Positions.data();
  mesh.Positions = Positions;
  const auto channel = [this](size_t index) {
    return VertexSource{.Writes = GeometryChannel::Write, .Carrying = &Channels[index]};
  };
  if (View.CarriesNormal) { mesh.Normals = channel(1); }
  if (View.CarriesTangent) { mesh.Tangents = channel(2); }
  if (View.CarriesUv) { mesh.Uv = channel(3); }
  if (View.CarriesUv1) { mesh.Uv1 = channel(4); }
  if (View.CarriesColour) { mesh.Colours = channel(5); }
  mesh.Emitted = {.Writes = Emit, .Carrying = this};
  mesh.VertexCount = static_cast<uint32_t>(View.VertexCount());
  mesh.Indices = Indices.data();
  mesh.IndexCount = static_cast<uint32_t>(Indices.size());
  mesh.Draws = &Draws;
  return mesh;
}
}
