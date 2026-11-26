#ifndef OUTSHINE_APP_BENCHMARKREPORT_H
#define OUTSHINE_APP_BENCHMARKREPORT_H

#include "BenchmarkMotion.h"
#include "GpuFrameSample.h"
#include "GpuFrameTiming.h"
#include "ProcessMemory.h"
#include "MemoryStatus.h"
#include <Engine.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace outshine::App {

struct BenchmarkFrameSample {
  double WallMs = 0;
  double AdvanceMs = 0;
  double RenderMs = 0;
  double SyncWaitMs = 0;
  bool Refined = false;
};

struct BenchmarkWorldGap {
  size_t FirstFrame = 0;
  std::string Blockers;
};

struct BenchmarkTileCoverage {
  std::array<uint32_t, 3> Address;
  std::array<double, 2> OriginLonLat;
  std::string Revision;
  size_t Triangles = 0;
  size_t OverlayTriangles = 0;
  double GeometricErrorM = 0;
  size_t StoredBytes = 0;
};

struct BenchmarkPanoramaCoverage {
  size_t Side = 0;
  Vec3 AnchorM;
  std::string Revision;
  double AxialNearM = 0;
  double AxialOverlapEndM = 0;
  double MaximumRangeM = 0;
  double MaximumTranslationM = 0;
  size_t StoredBytes = 0;
};

struct BenchmarkPanoramaHandoff {
  BenchmarkPanoramaCoverage Next;
  std::array<double, 2> BlendWindowFrames{};
};

struct BenchmarkSurfaceStream {
  std::optional<size_t> FirstRefinedFrame;
  size_t Tiles = 0;
  size_t ReusedTiles = 0;
  size_t InitialCanopySites = 0;
  size_t Triangles = 0;
  size_t DecodedBytes = 0;
  double ReadyMs = 0;
  std::string Error;
};

struct BenchmarkGroupCoverage {
  size_t Groups = 0;
  size_t ResidentViews = 0;
  size_t AvailableViews = 0;
  size_t PeakTriangles = 0;
  size_t PeakSurfaceAndGroupTriangles = 0;
  size_t HandoffGroups = 0;
  size_t BlendedFrames = 0;
  std::array<size_t, 2> SurfaceEndpointTriangles{};
  std::array<size_t, 2> GroupEndpointTriangles{};
  size_t RequestedBytes = 0;
  size_t CanopySites = 0;
  size_t CanopyMeshGroups = 0;
  size_t CanopyCoarseGroups = 0;
  size_t CanopyDetailHandoffs = 0;
  size_t CanopyInstances = 0;
  size_t CanopyRequestedBytes = 0;
};

struct ScenarioBenchmarkReport {
  std::string Scenario;
  std::string DeclarationSha256;
  std::string View;
  std::string Cpu;
  std::string Gpu;
  Extent Frame;
  BenchmarkMotionInput Motion;
  Camera Start;
  Camera Finish;
  std::vector<BenchmarkTileCoverage> Tiles;
  std::optional<BenchmarkPanoramaCoverage> Panorama;
  std::optional<BenchmarkPanoramaHandoff> PanoramaHandoff;
  std::optional<BenchmarkSurfaceStream> SurfaceStream;
  BenchmarkGroupCoverage GroupCoverage;
  std::optional<uint64_t> ProductStoreReadBytes;
  std::optional<size_t> IndexedPanoramaRequestedBytes;
  size_t TileRequestedBytes = 0;
  size_t ProductUploadBytes = 0;
  double ProductReadMs = 0;
  double ProductUploadMs = 0;
  Loading Prepared;
  Loading Finished;
  double SetupMs = 0;
  double PreloadMs = 0;
  double WindowMs = 0;
  double FlushMs = 0;
  double InspectionMs = 0;
  double ImageMs = 0;
  std::optional<double> ImageRowVariation;
  double PathPreparationMs = 0;
  double HardwareMs = 0;
  std::optional<std::array<double, 3>> LoadBefore;
  std::optional<std::array<double, 3>> LoadAfter;
  ProcessMemorySnapshot Memory{};
  Render::Native::MemoryStatus GpuMemory;
  std::vector<BenchmarkFrameSample> Frames;
  std::vector<Render::GpuFrameSample> GpuSamples;
  GpuFrameTiming GpuTiming;
  std::vector<DiagnosticSample> Measures;
  std::vector<BenchmarkWorldGap> WorldGaps;
  std::string FinalWorldBlockers;
};

[[nodiscard]] std::string ScenarioBenchmarkJson(const ScenarioBenchmarkReport &report);

}

#endif
