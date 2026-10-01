#include "CopernicusTerrain.h"
#include "CopernicusRaster.h"
#include "TilePool.h"
#include "TileGeodesy.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <memory>
#include <iterator>
#include <string_view>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Ground {
namespace {
constexpr uint32_t kFieldSide = 129;
constexpr size_t kCellMost = 128;
constexpr size_t kDecodedBudget = size_t{32} * 1024 * 1024;
constexpr uint64_t kHeaderBytes = 16384;
constexpr size_t kPartMost = 64;
constexpr uint64_t kMetadataMostBytes = uint64_t{128} * 1024;
constexpr double kGeoTolerance = 1e-8;
constexpr double kRowTolerance = 1e-6;
constexpr int kLongitudeHalfTurn = 180;
constexpr int kLongitudeTurn = 360;

struct Issue {
  std::optional<Data::FetchFailure> Failure;
};

template <class T> using Step = std::expected<T, Issue>;

int WrappedWest(int west) {
  return ((west + kLongitudeHalfTurn) % kLongitudeTurn + kLongitudeTurn) % kLongitudeTurn -
         kLongitudeHalfTurn;
}

double WrappedLongitude(double longitude) {
  return longitude - std::floor((longitude + kLongitudeHalfTurn) / kLongitudeTurn) * kLongitudeTurn;
}

struct OriginalPart {
  Data::RangeResponse Range;
  std::vector<uint8_t> Bytes;
};

struct Block {
  size_t Level = 0;
  uint32_t Index = 0;
  uint64_t Touched = 0;
  std::unique_ptr<Data::CopernicusBlock> Samples = nullptr;
};

struct Cell {
  Data::CellId At;
  uint64_t Touched = 0, Provenance = 0;
  std::string ObjectKey, SourceId, Revision, SourceKey;
  std::vector<OriginalPart> Parts;
  Data::CopernicusRaster Metadata;
  std::vector<Block> Blocks;
};

struct RasterRow {
  Cell *Source = nullptr;
  size_t Level = 0;
  uint32_t Index = 0;
};

struct RasterPost {
  RasterRow Line;
  uint32_t Column = 0;
};

struct Bracket {
  RasterPost First, Second;
};

size_t CellBytes(const Cell &cell) {
  size_t bytes = sizeof(Cell) + cell.ObjectKey.capacity() + cell.SourceId.capacity() +
                 cell.Revision.capacity() + cell.SourceKey.capacity() +
                 cell.Parts.capacity() * sizeof(OriginalPart) +
                 cell.Blocks.capacity() * sizeof(Block);
  for (const auto &part : cell.Parts) {
    bytes += part.Bytes.capacity() + part.Range.EntityTag.capacity();
  }
  if (!cell.Metadata.Levels.empty()) {
    bytes += cell.Metadata.ObjectKey.capacity() + cell.Metadata.EntityTag.capacity() +
             cell.Metadata.Levels.capacity() * sizeof(Data::CopernicusLevel);
    for (const auto &level : cell.Metadata.Levels) {
      bytes += level.Blocks.capacity() * sizeof(Data::ByteRange);
    }
  }
  for (const auto &block : cell.Blocks) {
    if (block.Samples != nullptr) {
      bytes += sizeof(Data::CopernicusBlock) + block.Samples->HeightsM.capacity() * sizeof(float);
    }
  }
  return bytes;
}

size_t LevelFor(const Data::CopernicusRaster &raster, double rowStep) {
  size_t selected = 0;
  for (size_t index = 1; index < raster.Levels.size(); ++index) {
    if (raster.Levels[index].RowStepDeg <= rowStep) { selected = index; }
  }
  return selected;
}

Data::CopernicusBlock Compact(Data::CopernicusBlock samples) {
  if (samples.Stride == samples.Columns &&
      samples.HeightsM.size() == static_cast<size_t>(samples.Rows) * samples.Columns) {
    return samples;
  }
  std::vector<float> compact(static_cast<size_t>(samples.Rows) * samples.Columns);
  for (uint32_t row = 0; row < samples.Rows; ++row) {
    std::ranges::copy(std::span(samples.HeightsM)
                          .subspan(static_cast<size_t>(row) * samples.Stride, samples.Columns),
                      compact.begin() +
                          static_cast<ptrdiff_t>(static_cast<size_t>(row) * samples.Columns));
  }
  samples.Stride = samples.Columns;
  samples.HeightsM = std::move(compact);
  return samples;
}
}

struct CopernicusTerrain::Impl {
  TilePool &Pool;
  std::vector<std::unique_ptr<Cell>> Cells;
  std::vector<Data::TileSourceIdentity> Sources;
  std::optional<Data::Fetch> Awaited;
  uint64_t Sequence = 0, Scope = 0;
  size_t DecodedBytes = 0;

  explicit Impl(TilePool &pool) : Pool(pool) {}

  static Issue
  Failed(Data::CellId at, Data::FetchFailureReason reason, const Cell *cell = nullptr) {
    return {.Failure = Data::FetchFailure{
                .Kind = Data::DataKind::Elevation,
                .Requested = Data::Address::AtCell(at),
                .Served = Data::Address::AtCell(at),
                .SourceId = cell != nullptr ? cell->SourceId : std::string{},
                .SourceRevision = cell != nullptr ? cell->Revision : std::string{},
                .SourceKey = cell != nullptr ? cell->SourceKey : std::string{},
                .Reason = reason}};
  }

  Step<TilePool::Landing> Ask(const Data::Fetch &request) {
    const auto at = request.Where().Cell();
    if (!at) {
      return std::unexpected(
          Issue{.Failure = Data::FetchFailure{.Kind = request.Kind(),
                                              .Requested = request.Where(),
                                              .Served = std::nullopt,
                                              .SourceId = {},
                                              .SourceRevision = {},
                                              .SourceKey = {},
                                              .Reason = Data::FetchFailureReason::InvalidRequest}});
    }
    TilePool::Landing landing;
    switch (Pool.Bytes(request, &landing)) {
      case TilePool::Reply::Ready: return landing;
      case TilePool::Reply::Pending:
      case TilePool::Reply::Deferred:
        if (!Awaited) { Awaited = request; }
        return std::unexpected(Issue{});
      case TilePool::Reply::Absent:
        return std::unexpected(Failed(*at, Data::FetchFailureReason::ConfirmedAbsent));
      case TilePool::Reply::Undeclared:
      case TilePool::Reply::Refused:
        return std::unexpected(landing.Failure
                                   ? Issue{.Failure = std::move(landing.Failure)}
                                   : Failed(*at, Data::FetchFailureReason::InvalidRequest));
    }
    std::unreachable();
  }

  Step<void> Append(Cell &cell, Data::ByteRange range) {
    if (cell.Parts.size() >= kPartMost) {
      return std::unexpected(Failed(cell.At, Data::FetchFailureReason::CapacityRefused, &cell));
    }
    uint64_t held = 0;
    for (const auto &part : cell.Parts) { held += part.Range.Bytes.Length; }
    if (range.Length > kMetadataMostBytes - held) {
      return std::unexpected(Failed(cell.At, Data::FetchFailureReason::CapacityRefused, &cell));
    }
    const std::string_view pin =
        cell.Parts.empty() ? std::string_view{} : cell.Parts.front().Range.EntityTag;
    auto landing = Ask(Data::Fetch(
        Data::DataKind::Elevation, Data::Address::AtCell(cell.At), range, std::string(pin)));
    if (!landing) { return std::unexpected(std::move(landing.error())); }
    if (!landing->Range || landing->At != Data::Address::AtCell(cell.At) ||
        (!cell.Parts.empty() &&
         (landing->SourceId != cell.SourceId ||
          landing->Range->TotalBytes != cell.Parts.front().Range.TotalBytes ||
          landing->Range->EntityTag != pin))) {
      return std::unexpected(Failed(cell.At, Data::FetchFailureReason::CorruptPayload, &cell));
    }
    if (cell.Parts.empty()) {
      cell.SourceId = std::move(landing->SourceId);
      cell.Revision = landing->SourceRevision + ":" + landing->Range->EntityTag;
      cell.SourceKey = std::move(landing->SourceKey);
      cell.ObjectKey = cell.SourceId + ":" + Data::Address::AtCell(cell.At).Text();
    }
    cell.Parts.push_back({.Range = std::move(*landing->Range), .Bytes = std::move(landing->Bytes)});
    std::ranges::sort(
        cell.Parts, {}, [](const OriginalPart &part) { return part.Range.Bytes.First; });
    return {};
  }

  [[nodiscard]] static std::vector<Data::CopernicusPart> PartsOf(const Cell &cell) {
    std::vector<Data::CopernicusPart> parts;
    parts.reserve(cell.Parts.size());
    for (const auto &part : cell.Parts) {
      parts.push_back({.ObjectKey = cell.ObjectKey, .Origin = part.Range, .Bytes = part.Bytes});
    }
    return parts;
  }

  [[nodiscard]] static Data::CopernicusObject
  ObjectOf(const Cell &cell, std::span<const Data::CopernicusPart> parts) {
    return {.ObjectKey = cell.ObjectKey,
            .TotalBytes = cell.Parts.front().Range.TotalBytes,
            .EntityTag = cell.Parts.front().Range.EntityTag,
            .Parts = parts};
  }

  Step<Cell *> Find(Data::CellId at) {
    for (const auto &cell : Cells) {
      if (cell->At == at) { return cell.get(); }
    }
    if (Cells.size() >= kCellMost) {
      const auto oldest =
          std::ranges::min_element(Cells, {}, [](const auto &cell) { return cell->Touched; });
      if ((*oldest)->Touched == Sequence) {
        return std::unexpected(Failed(at, Data::FetchFailureReason::CapacityRefused));
      }
      for (const auto &block : (*oldest)->Blocks) {
        if (block.Samples != nullptr) {
          DecodedBytes -= block.Samples->HeightsM.capacity() * sizeof(float);
        }
      }
      Cells.erase(oldest);
    }
    auto cell = std::make_unique<Cell>();
    cell->At = at;
    auto *created = cell.get();
    Cells.push_back(std::move(cell));
    return created;
  }

  Step<Cell *> Ready(Data::CellId at) {
    auto found = Find(at);
    if (!found) { return found; }
    Cell &cell = **found;
    if (!cell.Metadata.Levels.empty()) {
      cell.Touched = Sequence;
      return *found;
    }
    if (cell.Touched == Sequence) { return std::unexpected(Issue{}); }
    cell.Touched = Sequence;
    if (cell.Parts.empty()) {
      auto initial = Append(cell, {.First = 0, .Length = kHeaderBytes});
      if (!initial) { return std::unexpected(std::move(initial.error())); }
    }
    const auto parts = PartsOf(cell);
    auto metadata = Data::ReadCopernicusRaster(ObjectOf(cell, parts));
    if (!metadata) {
      const auto needed = metadata.error().Needed;
      if (needed) {
        auto appended = Append(cell, *needed);
        if (!appended) { return std::unexpected(std::move(appended.error())); }
        cell.Touched = 0;
        return Ready(at);
      }
      return std::unexpected(Failed(at, Data::FetchFailureReason::CorruptPayload, &cell));
    }
    const auto &base = metadata->Levels.front();
    if (std::abs(base.WestDeg - at.WestDeg) > kGeoTolerance ||
        std::abs(base.NorthDeg - (at.SouthDeg + 1)) > kGeoTolerance) {
      return std::unexpected(Failed(at, Data::FetchFailureReason::CorruptPayload, &cell));
    }
    cell.Metadata = std::move(*metadata);
    return *found;
  }

  bool Admit(size_t bytes) {
    while (DecodedBytes + bytes > kDecodedBudget) {
      Block *oldest = nullptr;
      for (const auto &cell : Cells) {
        for (auto &block : cell->Blocks) {
          if (block.Samples != nullptr && (oldest == nullptr || block.Touched < oldest->Touched)) {
            oldest = &block;
          }
        }
      }
      if (oldest == nullptr) { return false; }
      DecodedBytes -= oldest->Samples->HeightsM.capacity() * sizeof(float);
      oldest->Samples.reset();
    }
    return true;
  }

  Step<Data::CopernicusBlock *> Samples(Cell &cell, size_t level, uint32_t index) {
    auto found = std::ranges::find_if(cell.Blocks, [level, index](const Block &block) {
      return block.Level == level && block.Index == index;
    });
    if (found == cell.Blocks.end()) {
      cell.Blocks.push_back({.Level = level, .Index = index});
      found = std::prev(cell.Blocks.end());
    }
    if (found->Samples != nullptr) {
      found->Touched = Sequence;
      return found->Samples.get();
    }
    if (found->Touched == Sequence) { return std::unexpected(Issue{}); }
    found->Touched = Sequence;
    auto parts = PartsOf(cell);
    auto decoded = Data::ReadCopernicusBlock(ObjectOf(cell, parts), cell.Metadata, level, index);
    if (!decoded) {
      const auto needed = decoded.error().Needed;
      if (!needed) {
        return std::unexpected(Failed(cell.At, Data::FetchFailureReason::CorruptPayload, &cell));
      }
      auto landing = Ask(Data::Fetch(Data::DataKind::Elevation,
                                     Data::Address::AtCell(cell.At),
                                     *needed,
                                     cell.Metadata.EntityTag));
      if (!landing) { return std::unexpected(std::move(landing.error())); }
      if (!landing->Range || landing->At != Data::Address::AtCell(cell.At) ||
          landing->SourceId != cell.SourceId) {
        return std::unexpected(Failed(cell.At, Data::FetchFailureReason::CorruptPayload, &cell));
      }
      parts.push_back(
          {.ObjectKey = cell.ObjectKey, .Origin = *landing->Range, .Bytes = landing->Bytes});
      std::ranges::sort(
          parts, {}, [](const Data::CopernicusPart &part) { return part.Origin.Bytes.First; });
      decoded = Data::ReadCopernicusBlock(ObjectOf(cell, parts), cell.Metadata, level, index);
    }
    if (!decoded) {
      return std::unexpected(Failed(cell.At, Data::FetchFailureReason::CorruptPayload, &cell));
    }
    auto compact = Compact(std::move(*decoded));
    const size_t bytes = compact.HeightsM.capacity() * sizeof(float);
    if (!Admit(bytes)) {
      return std::unexpected(Failed(cell.At, Data::FetchFailureReason::CapacityRefused, &cell));
    }
    found->Samples = std::make_unique<Data::CopernicusBlock>(std::move(compact));
    DecodedBytes += bytes;
    return found->Samples.get();
  }

  Step<float> Posting(RasterPost post) {
    Cell &cell = *post.Line.Source;
    const size_t levelIndex = post.Line.Level;
    const uint32_t row = post.Line.Index;
    const uint32_t col = post.Column;
    const auto &level = cell.Metadata.Levels[levelIndex];
    const uint32_t blockColumns = (level.Columns + level.BlockColumns - 1) / level.BlockColumns;
    const uint32_t blockIndex = (row / level.BlockRows) * blockColumns + col / level.BlockColumns;
    auto samples = Samples(cell, levelIndex, blockIndex);
    if (!samples) { return std::unexpected(std::move(samples.error())); }
    const auto height =
        (*samples)->HeightM(row - (*samples)->FirstRow, col - (*samples)->FirstColumn);
    if (!height) {
      return std::unexpected(Failed(cell.At, Data::FetchFailureReason::CorruptPayload, &cell));
    }
    if (cell.Provenance != Sequence) {
      Sources.push_back(
          {.NativeCell = cell.At, .SourceId = cell.SourceId, .Revision = cell.Revision});
      cell.Provenance = Sequence;
    }
    return *height;
  }

  Step<float> Blend(Bracket bracket, double weight) {
    if (weight <= 0) { return Posting(bracket.First); }
    if (weight >= 1) { return Posting(bracket.Second); }
    auto left = Posting(bracket.First);
    auto right = Posting(bracket.Second);
    if (!left) { return std::unexpected(std::move(left.error())); }
    if (!right) { return std::unexpected(std::move(right.error())); }
    return static_cast<float>(*left * (1 - weight) + *right * weight);
  }

  Step<float> Horizontal(RasterRow line, double longitude) {
    Cell &cell = *line.Source;
    const size_t levelIndex = line.Level;
    const uint32_t row = line.Index;
    const auto &level = cell.Metadata.Levels[levelIndex];
    const double column = (longitude - level.WestDeg) / level.ColumnStepDeg;
    const auto last = level.Columns - 1;
    if (column == last) { return Posting({.Line = line, .Column = last}); }
    if (column >= 0 && column < last) {
      const auto first = static_cast<uint32_t>(column);
      return Blend(
          {.First = {.Line = line, .Column = first}, .Second = {.Line = line, .Column = first + 1}},
          column - first);
    }
    const bool west = column < 0;
    auto adjacent = Ready(
        {.SouthDeg = cell.At.SouthDeg, .WestDeg = WrappedWest(cell.At.WestDeg + (west ? -1 : 1))});
    if (!adjacent) { return std::unexpected(std::move(adjacent.error())); }
    Cell &other = **adjacent;
    const size_t otherIndex = LevelFor(other.Metadata, level.RowStepDeg * (1 + kGeoTolerance));
    const auto &otherLevel = other.Metadata.Levels[otherIndex];
    const double latitude = level.NorthDeg - row * level.RowStepDeg;
    const double otherRow = (otherLevel.NorthDeg - latitude) / otherLevel.RowStepDeg;
    if (otherRow < -kRowTolerance || otherRow > otherLevel.Rows - 1 + kRowTolerance ||
        std::abs(otherRow - std::round(otherRow)) > kRowTolerance) {
      return std::unexpected(Failed(other.At, Data::FetchFailureReason::CorruptPayload, &other));
    }
    const uint32_t otherCol = west ? otherLevel.Columns - 1 : 0;
    double otherLongitude = otherLevel.WestDeg + otherCol * otherLevel.ColumnStepDeg;
    if (other.At.WestDeg - cell.At.WestDeg > kLongitudeHalfTurn) {
      otherLongitude -= kLongitudeTurn;
    }
    if (cell.At.WestDeg - other.At.WestDeg > kLongitudeHalfTurn) {
      otherLongitude += kLongitudeTurn;
    }
    const uint32_t ownCol = west ? 0 : last;
    const double ownLongitude = level.WestDeg + ownCol * level.ColumnStepDeg;
    const double weight = (longitude - ownLongitude) / (otherLongitude - ownLongitude);
    return Blend({.First = {.Line = line, .Column = ownCol},
                  .Second = {.Line = {.Source = &other,
                                      .Level = otherIndex,
                                      .Index = static_cast<uint32_t>(std::round(otherRow))},
                             .Column = otherCol}},
                 weight);
  }

  Step<float> Height(LongitudeLatitude position, double resolution) {
    const double longitude = position.LongitudeDeg;
    const double latitude = position.LatitudeDeg;
    const Data::CellId at{.SouthDeg = static_cast<int>(std::floor(latitude)),
                          .WestDeg = static_cast<int>(std::floor(longitude))};
    auto ready = Ready(at);
    if (!ready) { return std::unexpected(std::move(ready.error())); }
    Cell &cell = **ready;
    const size_t levelIndex = LevelFor(cell.Metadata, resolution);
    const auto &level = cell.Metadata.Levels[levelIndex];
    const double row = (level.NorthDeg - latitude) / level.RowStepDeg;
    const uint32_t last = level.Rows - 1;
    if (row == last) {
      return Horizontal({.Source = &cell, .Level = levelIndex, .Index = last}, longitude);
    }
    if (row >= 0 && row < last) {
      const auto first = static_cast<uint32_t>(row);
      auto upper = Horizontal({.Source = &cell, .Level = levelIndex, .Index = first}, longitude);
      if (!upper || row == first) { return upper; }
      auto lower =
          Horizontal({.Source = &cell, .Level = levelIndex, .Index = first + 1}, longitude);
      if (!lower) { return lower; }
      return static_cast<float>(*upper * (1 - (row - first)) + *lower * (row - first));
    }
    const bool north = row < 0;
    auto adjacent = Ready({.SouthDeg = at.SouthDeg + (north ? 1 : -1), .WestDeg = at.WestDeg});
    if (!adjacent) { return std::unexpected(std::move(adjacent.error())); }
    Cell &other = **adjacent;
    const size_t otherIndex = LevelFor(other.Metadata, resolution);
    const auto &otherLevel = other.Metadata.Levels[otherIndex];
    const uint32_t ownRow = north ? 0 : last;
    const uint32_t otherRow = north ? otherLevel.Rows - 1 : 0;
    const double ownLatitude = level.NorthDeg - ownRow * level.RowStepDeg;
    const double otherLatitude = otherLevel.NorthDeg - otherRow * otherLevel.RowStepDeg;
    const double weight = (latitude - ownLatitude) / (otherLatitude - ownLatitude);
    if (weight <= 0) {
      return Horizontal({.Source = &cell, .Level = levelIndex, .Index = ownRow}, longitude);
    }
    if (weight >= 1) {
      return Horizontal({.Source = &other, .Level = otherIndex, .Index = otherRow}, longitude);
    }
    auto own = Horizontal({.Source = &cell, .Level = levelIndex, .Index = ownRow}, longitude);
    auto next = Horizontal({.Source = &other, .Level = otherIndex, .Index = otherRow}, longitude);
    if (!own) { return own; }
    if (!next) { return next; }
    return static_cast<float>(*own * (1 - weight) + *next * weight);
  }

  TerrainBytes Take(Data::TileId at) {
    if (at.Zoom < 0 || at.Zoom > Data::TileId::MaximumZoom ||
        at.X >= (uint32_t{1} << static_cast<uint32_t>(at.Zoom)) ||
        at.Y >= (uint32_t{1} << static_cast<uint32_t>(at.Zoom))) {
      return TerrainBytes::Wire(
          Data::FetchFailure{.Kind = Data::DataKind::Elevation,
                             .Requested = Data::Address::At(at),
                             .Served = std::nullopt,
                             .SourceId = {},
                             .SourceRevision = {},
                             .SourceKey = {},
                             .Reason = Data::FetchFailureReason::InvalidRequest});
    }
    if (Scope != Pool.TerrainScopeRevision()) {
      Cells.clear();
      DecodedBytes = 0;
      Scope = Pool.TerrainScopeRevision();
    }
    ++Sequence;
    Sources.clear();
    Awaited.reset();
    TerrainField field(kFieldSide, kFieldSide);
    const auto bounds = TileBounds(at);
    const double resolution = (bounds.MaxLatDeg - bounds.MinLatDeg) / (kFieldSide - 1);
    bool waiting = false;
    for (uint32_t row = 0; row < kFieldSide; ++row) {
      const double fractionY = static_cast<double>(row) / (kFieldSide - 1);
      for (uint32_t col = 0; col < kFieldSide; ++col) {
        const double fractionX = static_cast<double>(col) / (kFieldSide - 1);
        const Geo point = TileFracToGeo({.X = at.X + fractionX, .Y = at.Y + fractionY}, at.Zoom);
        auto height = Height({.LongitudeDeg = WrappedLongitude(point.LongitudeDeg),
                              .LatitudeDeg = point.LatitudeDeg},
                             resolution);
        if (!height) {
          if (height.error().Failure) {
            return TerrainBytes::Wire(std::move(height.error().Failure));
          }
          waiting = true;
        } else {
          field.SetM(row, col, *height);
        }
      }
    }
    if (waiting) { return TerrainBytes::Waiting(); }
    field.AddSources(Sources);
    const auto identity = field.Sources().front();
    return TerrainBytes::From(at, std::move(field), identity);
  }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    size_t bytes = sizeof(Impl) + Cells.capacity() * sizeof(std::unique_ptr<Cell>) +
                   Sources.capacity() * sizeof(Data::TileSourceIdentity);
    for (const auto &source : Sources) {
      bytes += source.SourceId.capacity() + source.Revision.capacity();
    }
    for (const auto &cell : Cells) { bytes += CellBytes(*cell); }
    if (Awaited) { bytes += Awaited->EntityTag().size(); }
    return bytes;
  }
};

CopernicusTerrain::CopernicusTerrain(TilePool &pool) : State_(std::make_unique<Impl>(pool)) {}

CopernicusTerrain::~CopernicusTerrain() = default;

TerrainBytes CopernicusTerrain::Take(Data::TileId at) {
  return State_->Take(at);
}

const std::optional<Data::Fetch> &CopernicusTerrain::Awaiting() const noexcept {
  return State_->Awaited;
}

size_t CopernicusTerrain::HeapBytes() const noexcept {
  return State_->HeapBytes();
}
}
