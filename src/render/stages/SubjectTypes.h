#ifndef OUTSHINE_RENDER_STAGES_SUBJECTTYPES_H
#define OUTSHINE_RENDER_STAGES_SUBJECTTYPES_H

#include <cstdint>
#include "ClusterDag.h"
#include <span>

#include "PunctualLight.h"
#include "UvTransform.h"

#include "DrawList.h"

namespace outshine::Render {

struct SubjectLight {
  outshine::PunctualLight Light;
  double PositionEcefM[3] = {0, 0, 0};
};

struct SubjectEnvironment {
  double RadianceLinear[3] = {0, 0, 0};
  double GroundLinear[3] = {0, 0, 0};
  double UpUnit[3] = {0, 1, 0};
};

constexpr size_t kMaxSubjectLights = 16;

constexpr uint32_t kSubjectMaterialImages = 6;
constexpr uint32_t kSubjectImages = kSubjectMaterialImages + 2;
constexpr uint32_t kSubjectFragmentUniforms = 2;
constexpr uint32_t kSubjectStorageBuffers = 2;

enum class SubjectWrap { ClampToEdge, MirroredRepeat, Repeat };
enum class SubjectFilter { Nearest, Linear };

enum class SubjectMip { None, Nearest, Linear };

struct SubjectTexture {
  const uint8_t *Rgba = nullptr;
  uint32_t Width = 0;
  uint32_t Height = 0;
  SubjectWrap WrapU = SubjectWrap::Repeat;
  SubjectWrap WrapV = SubjectWrap::Repeat;
  SubjectFilter Magnify = SubjectFilter::Linear;

  SubjectFilter Minify = SubjectFilter::Linear;
  SubjectMip Mip = SubjectMip::Linear;

  outshine::UvTransform Uv;

  outshine::UvSet Set = outshine::UvSet::First;
};

struct SubjectMaterial {

  Material Row;
  SubjectTexture Colour;

  SubjectTexture SpecularStrength;
  SubjectTexture SpecularTint;

  SubjectTexture Normal;
  SubjectTexture MetalRough;
  SubjectTexture Emissive;

  float NormalScale = 1.0f;

  [[nodiscard]] SurfaceState State() const { return StateOf(Row); }
  [[nodiscard]] float Coverage() const { return Row.BaseColour[3]; }

  [[nodiscard]] bool ReadsSecondUv() const {
    return Colour.Set == outshine::UvSet::Second || Normal.Set == outshine::UvSet::Second ||
           MetalRough.Set == outshine::UvSet::Second || Emissive.Set == outshine::UvSet::Second ||
           SpecularStrength.Set == outshine::UvSet::Second ||
           SpecularTint.Set == outshine::UvSet::Second;
  }

  [[nodiscard]] bool ReadsAnyImage() const {
    return Colour.Rgba || Normal.Rgba || MetalRough.Rgba || Emissive.Rgba ||
           SpecularStrength.Rgba || SpecularTint.Rgba;
  }
};

// A STREAM THAT WRITES ITSELF INTO THE DEVICE'S OWN MAPPING. A pointer says "copy these bytes
// from over there"; a writer says "you hold the memory, fill it". The producer's data is already
// float and already in the device's layout, so the only work left was CONCATENATING the parts into
// one run -- and SDL's mapped transfer buffer is as good a place to concatenate into as a vector
// of ours, minus the vector. On Shibuya that vector is 900 MB and the copy out of it is 1932 ms.
//
// The writer is a plain function pointer and an opaque context because the thing that can pack a
// channel knows both the shape and the proxy, and the stage that owns the mapping must know
// neither -- `src/render/stages/` does not name `SubjectProxy` and this keeps it that way.
struct SubjectStream {
  const float *From = nullptr;
  void (*Writes)(const void *carrying, float *into, uint32_t floats) = nullptr;
  const void *Carrying = nullptr;

  [[nodiscard]] bool Stands() const { return From != nullptr || Writes != nullptr; }
};

struct SubjectPose {
  SubjectStream Verts;

  // THE POSITIONS, CONTIGUOUS, FOR THE WORK THAT HAPPENS ON THIS SIDE. The visibility structure is
  // built and refitted on the CPU and wants one run it can index; every other channel goes from
  // the producer's spans into the device's mapping and is never assembled here at all.
  std::span<const float> Positions;
  SubjectStream Uv;

  SubjectStream Uv1;
  SubjectStream Normals;

  SubjectStream Tangents;

  SubjectStream Colours;
  SubjectStream Emitted;

  SubjectStream PrevVerts;
  uint32_t VertexCount = 0;
  double Anchor[3] = {0, 0, 0};
  double Model[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

struct SubjectMesh : SubjectPose {
  const uint32_t *Indices = nullptr;
  uint32_t IndexCount = 0;
  const DrawList *Draws = nullptr;

  // THE CUT, CROSSING WITH THE REST. The cooker made these where the shape was built; they reach
  // the device as two storage buffers and nothing between here and there reshapes them.
  std::span<const DagCluster> Clusters;

  // WHAT THE DEVICE BINDS. `Clusters` is the cooker's record and stays on this side; this is its
  // layout, written where the cut was taken.
  std::span<const float> ClusterSpheres;
};

}

#endif
