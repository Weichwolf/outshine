#ifndef OUTSHINE_ENGINE_STREAMTELEMETRY_H
#define OUTSHINE_ENGINE_STREAMTELEMETRY_H

#include "Telemetry.h"
#include "TilePool.h"
#include "World.h"

namespace outshine::Clients {

class StreamTelemetry : public TelemetrySource {
public:

  struct Pass {
    double WorldMs = 0.0, MeshMs = 0.0, UploadMs = 0.0;
    double BuildingMs = 0.0, BuildingDecodeMs = 0.0, ClassMs = 0.0;

    double PopulateMs = 0.0;
    int TilesTotal = 0, TilesSettled = 0, TilesInView = 0, VectorTilesPending = 0;
    long long Built = 0, Evicted = 0;
    bool Resident = false;

    bool EyeInBand = true;

    Ground::TilePool::Ledger Pool;

    Ground::World::Admission Admission;
  };

  void Open(double nowMs) { OpenedMs_ = nowMs; }
  void AddPass(const Pass &p);

  void MarkResident(double nowMs);
  double LoadMs() const { return LoadMs_; }
  long long PassCount() const { return Passes_; }

  void Reset();

  const char *TelemetryName() const override { return "stream"; }
  void DeclareTelemetry(TelemetrySchema &schema) const override;
  void SampleTelemetry(TelemetryRow &row) const override;

private:
  struct Stat {
    double Sum = 0.0, Max = 0.0;
    int N = 0;
    void Add(double v) { Sum += v; N++; if (v > Max) Max = v; }
    double Mean() const { return N ? Sum / (double)N : 0.0; }
    void Reset() { Sum = 0.0; Max = 0.0; N = 0; }
  };

  Stat World_, Mesh_, Upload_, Building_, Decode_, Class_, Populate_;
  Pass Last_;
  long long Passes_ = 0, WindowBuilt_ = 0, WindowEvicted_ = 0, PrevBuilt_ = 0, PrevEvicted_ = 0;
  int WindowPasses_ = 0;
  double OpenedMs_ = 0.0, LoadMs_ = 0.0;
};

}
#endif
