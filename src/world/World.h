#ifndef WORLD_H
#define WORLD_H

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "BuildingField.h"
#include "StreetField.h"
#include "WaterField.h"
#include "ClassField.h"
#include "ClusterDag.h"

namespace outshine { class WeatherProvider; }

namespace outshine::Data {
class SourceSet;
class Transport;
}

namespace outshine::World {

class VegetationTemplates;

class World {
public:

  explicit World(double pixelFocalLength);

  ~World();

  void SetVegetation(const VegetationTemplates *veg) { Veg_ = veg; Cls_.SetVegetation(veg); }

  const ClassField &Classes() const { return Cls_; }

  void SetWeather(const WeatherProvider *weather) { Weather_ = weather; }
  const WeatherProvider *Weather() const { return Weather_; }

  [[nodiscard]] bool Open(Data::SourceSet &sources, Data::Transport &transport, double lat,
                          double lon, double viewMeters);

  void Update(double camLat, double camLon);

  struct Eye {
    double LatDeg = 0.0, LonDeg = 0.0;
    const double *PosEcef = nullptr;
    const double *FwdEcef = nullptr;
  };

  void Refine(const Eye &eye, double nowMs);

  struct TileMesh {
    uint64_t Id = 0;
    const float *Verts = nullptr;
    uint32_t VertCount = 0;
    const uint32_t *Idx = nullptr;
    uint32_t IdxCount = 0;
    const DagCluster *Clusters = nullptr;
    int ClusterCount = 0;
    double OriginEcef[3] = {};
    double AnchorEcef[3] = {};
  };
  const std::vector<TileMesh> &Uncollected() const { return Uncollected_; }

  void Collect(uint64_t id, int handle);

  const std::vector<int> &Drawn() const { return DrawnHandles_; }

  std::vector<int> TakeRetired();

  struct WaterSurface {
    const float *Verts = nullptr;
    uint32_t VertCount = 0;
    const double *AnchorEcef = nullptr;
    uint64_t Seq = 0;
  };
  WaterSurface Water() const;

  struct BuildingSurface {
    const float *Verts = nullptr;
    uint32_t VertCount = 0;
    const uint32_t *Idx = nullptr;
    uint32_t IdxCount = 0;
    const DagCluster *Clusters = nullptr;
    int ClusterCount = 0;
    const double *AnchorEcef = nullptr;
    uint64_t Seq = 0;
  };
  BuildingSurface Buildings() const;

  float LoadProgress() const { return TargetTot > 0 ? (float)TargetRdy / (float)TargetTot : 0.0f; }
  int TargetTotal() const { return TargetTot; }
  int TargetSettledN() const { return TargetRdy; }

  int TargetInViewN() const { return TargetView; }

  enum class Await { Nothing, TargetCut, BuildingDag, BuildingBlocks, Vectors, Class };

  [[nodiscard]] Await SimWaiting() const;
  [[nodiscard]] bool VectorsResident() const { return SimWaiting() == Await::Nothing; }

  [[nodiscard]] Await Waiting() const;
  [[nodiscard]] bool Resident() const { return Waiting() == Await::Nothing; }
  int BuildingPendingTiles() const { return Vectors_.PendingTiles(); }

  const OsmField &Vectors() const { return Vectors_; }
  const BuildingField &Footprints() const { return Buildings_; }

  void StructureShapes(const StructureMesher *mesher) { Buildings_.Shapes(mesher); }
  const WaterField &WaterBodies() const { return Water_; }
  const StreetField &Ways() const { return Streets_; }

  [[nodiscard]] bool EyeInMercatorBand() const { return EyeInBand_; }

  int NodeCount() const { return (int)Nodes.size(); }
  int DrawnLeafCount() const { return (int)DrawnLeaves.size(); }
  long long BuiltCount() const { return Built; }
  long long EvictedCount() const { return Evicted; }

  struct Admission {

    long long Wanted = 0;
    long long Asked = 0;
    long long Admitted = 0;
    long long Waiting = 0;

    long long Absent = 0;
    long long Capped = 0;
  };
  const Admission &Admissions() const { return Adm_; }

  static constexpr int kMeshBuildsPerPass = 2;

  double PassMs() const { return UpdateMs_ + RefineMs_; }
  double ClassMs() const { return ClassMs_; }
  double MeshMs() const { return MeshMs_; }
  double BuildingMs() const { return BuildingMs_ + BuildingDagMs_; }
  double BuildingDecodeMs() const { return BuildingDecodeMs_; }

  struct Pools {
    size_t TileNodes = 0, Vectors = 0, Buildings = 0, Water = 0, Streets = 0, Class = 0;
    size_t ByteCache = 0, DemCache = 0, Scheduler = 0;
    size_t Sum() const {
      return TileNodes + Vectors + Buildings + Water + Streets + Class + ByteCache + DemCache +
             Scheduler;
    }
  };
  Pools HeapPools() const;

  void SetPixelFocalLength(double px) { PixelFocal_ = px; }

private:

  enum class MeshState { Wanted, Held, Vacant };

  struct Node {
    int z;
    long x, y;
    unsigned touch;
    int stale;
    int handle;
    MeshState Mesh = MeshState::Wanted;

    bool SplitRefused = false;
    unsigned readyPass;
    unsigned emitPass;
    std::vector<float> verts;
    std::vector<uint32_t> idx;
    std::vector<DagCluster> clusters;
    int nverts, nidx;
    double origin[3];
    double anchor[3];
    float err;
  };
  struct Work { int idx; double prio; };

  int Ensure(int z, long x, long y);
  [[nodiscard]] bool Taken(const Node &n) const { return n.Mesh == MeshState::Held && n.handle >= 0; }

  [[nodiscard]] bool Ready(const Node &n) const { return Taken(n) && Pass > n.readyPass; }

  [[nodiscard]] bool Settled(const Node &n) const { return Ready(n) || n.Mesh == MeshState::Vacant; }
  [[nodiscard]] bool Wants(const Node &n) const { return n.Mesh != MeshState::Vacant && !Taken(n); }
  [[nodiscard]] bool Viable(int z, long x, long y, const double eye[3]) const;
  [[nodiscard]] bool WantSplit(int z, long x, long y, const double eye[3]) const;

  [[nodiscard]] bool Splits(int z, long x, long y, const double eye[3]) const;
  int  Find(int z, long x, long y) const;
  [[nodiscard]] bool CanCover(int z, long x, long y, const double eye[3]) const;
  void RequestSubtree(int z, long x, long y, const double eye[3], const double fwd[3]);
  void RootRing(const Eye &eye, uint32_t rx, uint32_t ry);
  void NoteBand(bool inBand, double latDeg, double lonDeg);
  int  Descend(int z, long x, long y, const double eye[3], const double fwd[3]);
  void DrawChildren(int z, long x, long y, const double eye[3], const double fwd[3]);
  void CountTargets(int z, long x, long y, const double eye[3], const double fwd[3], int &total,
                    int &ready, int &inView) const;
  void Emit(int idx);

  void AdmitMesh(Node &nd, int &budget);
  void AddWork(int idx, int z, long x, long y, const double eye[3], const double fwd[3]);
  void Center(int z, long x, long y, double out[3]) const;
  double SpanM(int z) const;
  void SurfaceAnchor(int z, long x, long y, double out[3]) const;

  const WeatherProvider *Weather_ = nullptr;
  const VegetationTemplates *Veg_ = nullptr;

  double PixelFocal_;
  double ViewM, Lat0, Lon0;
  std::vector<Node> Nodes;
  std::unordered_map<uint64_t, int> Index_;
  std::vector<int> DrawnHandles_;
  std::vector<TileMesh> Uncollected_;
  std::vector<int> Retired_;
  std::vector<Work> WorkList;
  unsigned Pass;
  long long Evicted;
  long long Built = 0;
  Admission Adm_;
  bool EyeInBand_ = true;
  long long PrevBuilt = 0, PrevEvicted = 0;
  double LastLog;
  int Leaves, DrawnReady, Pending;
  int TargetTot, TargetRdy;
  int TargetView = 0;
  long MeshVram;

  std::vector<int> DrawnLeaves;

  OsmField Vectors_{14, OsmLayerNames({OsmLayer::Buildings, OsmLayer::WaterPolygons,
                                       OsmLayer::WaterLines, OsmLayer::Streets,
                                       OsmLayer::StreetPolygons})};
  ClassField Cls_;
  BuildingField Buildings_;
  WaterField Water_;
  void CutKerbs();

  StreetField Streets_;

  std::vector<WayLine> Kerbs_;
  std::vector<float> WaterVerts;
  uint64_t WaterSeq_ = 0;
  bool WaterDirty_ = false;
  uint32_t BuildingVerts = 0;
  std::vector<float> BuildingDagVerts;
  std::vector<uint32_t> BuildingDagIdx;
  std::vector<DagCluster> BuildingClusters;
  std::vector<float> BuildingSoup;

  std::vector<uint32_t> FootprintTileEnds_;
  size_t DagDone_ = 0;
  int BuildingDagId = 0, BuildingDagSeq = 0;
  uint64_t BuildingSeq_ = 0;
  bool Opened = false;
  double UpdateMs_ = 0.0, RefineMs_ = 0.0;
  double ClassMs_ = 0.0;
  double MeshMs_ = 0.0, BuildingMs_ = 0.0, BuildingDagMs_ = 0.0, BuildingDecodeMs_ = 0.0;
};

const char *AwaitName(World::Await await);

}
#endif
