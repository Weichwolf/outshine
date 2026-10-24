#include "RuntimeScene.h"
#include "FrameCapture.h"
#include "SceneRenderer.h"
#include "Check.h"
#include "test/harness/render/NativeUploadFaultDevice.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using outshine::Test::Gpu::UploadFailurePoint;
constexpr size_t kExtentPx = 32;
constexpr double kNearM = 0.1;
constexpr double kFarM = 100;
constexpr std::array<uint8_t, 4> kWhite{255, 255, 255, 255};
constexpr std::array<uint8_t, 4> kBlack{0, 0, 0, 255};
constexpr std::array kUploadFailures{UploadFailurePoint::UploadBuffer,
                                     UploadFailurePoint::Map,
                                     UploadFailurePoint::Unmap,
                                     UploadFailurePoint::Acquire,
                                     UploadFailurePoint::Copy,
                                     UploadFailurePoint::Submit};

struct OverlayFixture {
  outshine::Test::Gpu::UploadFailure Failure{.Message = "injected native overlay upload failure"};
  SceneRenderer Renderer{GpuSubmission{
      .Context = &Failure, .Create = outshine::Test::Gpu::NativeUploadFaultDevice::Create}};
  UploadQueue::WaitScope UploadBudget = Renderer.OfflineUploads(std::chrono::seconds(5));
  std::unique_ptr<Core::RuntimeScene> Scene;
  std::string Error;
  OverlayQuad Quad;
  std::array<OverlayQuad, 2> Replacement;
  std::vector<uint8_t> Baseline;

  bool Open() {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kExtentPx;
    declaration.Outputs = {"surface"};
    CHECK(Core::RuntimeScene::Open(Renderer, declaration, nullptr, Scene, Error),
          "empty scene opens");
    if (!Scene) { return false; }
    Viewpoint eye;
    eye.EyeM = {{0, 0, 5}};
    eye.YfovRad = 1;
    eye.ZNearM = kNearM;
    eye.ZFarM = kFarM;
    Scene->Eye(eye);
    Quad.WidthPx = Quad.HeightPx = Quad.ClipWidthPx = Quad.ClipHeightPx = kExtentPx;
    Quad.U1 = Quad.V1 = 1;
    Replacement = {Quad, Quad};
    Replacement[0].Red = Replacement[0].Green = Replacement[0].Blue = 0;
    Replacement[1] = Replacement[0];
    CHECK(Renderer.ReplaceOverlay(std::span(&Quad, 1), nullptr, Error),
          "combined overlay replacement provisions its own default atlas");
    Baseline = Read();
    CHECK(Baseline.size() == kExtentPx * kExtentPx * 4 && Baseline[0] == 255,
          "the default atlas produces white overlay pixels");
    return true;
  }

  std::vector<uint8_t> Read() {
    std::vector<uint8_t> pixels;
    CHECK(Scene->Draw(Error), Error.c_str());
    CHECK(Core::ReadFrame(Renderer, pixels, Error), Error.c_str());
    return pixels;
  }

  unsigned Arm(UploadFailurePoint point, unsigned skip = 0) {
    Failure.Point = point;
    Failure.Skip = skip;
    Failure.Armed = true;
    Error.clear();
    return Failure.Rejected;
  }

  void Reached(unsigned before) const {
    CHECK(Failure.Rejected == before + 1 && !Failure.Armed, "requested failure point was reached");
  }

  void AtlasFailure(UploadFailurePoint point) {
    const auto before = Arm(point);
    CHECK(!Renderer.SetOverlayAtlas(kBlack.data(), 1, 1, Error) && !Error.empty(),
          "atlas upload failure is reported");
    Reached(before);
    CHECK(Read() == Baseline, "rejected upload preserves resident atlas pixels");
    CHECK(Renderer.SetOverlayAtlas(kBlack.data(), 1, 1, Error), "atlas retry succeeds");
    CHECK(Read() != Baseline, "retry publishes a genuinely different atlas");
    CHECK(Renderer.SetOverlayAtlas(kWhite.data(), 1, 1, Error), "white atlas restored");
  }

  void QuadFailure(UploadFailurePoint point) {
    const auto before = Arm(point);
    CHECK(!Renderer.SetOverlay(Replacement.data(), Replacement.size(), Error) && !Error.empty(),
          "quad upload failure is reported");
    Reached(before);
    CHECK(Read() == Baseline, "quad failure preserves previous geometry and count");
    CHECK(Renderer.SetOverlay(Replacement.data(), Replacement.size(), Error),
          "quad retry succeeds");
    CHECK(Read() != Baseline, "quad retry changes the image");
    CHECK(Renderer.SetOverlay(&Quad, 1, Error), "original quad restored");
  }

  void QuadInputs() {
    CHECK(!Renderer.SetOverlay(nullptr, 1, Error), "nonempty null quad input rejected");
    CHECK(Read() == Baseline, "null input preserves overlay");
    CHECK(!Renderer.SetOverlay(&Quad, kMaxOverlayQuads + 1, Error),
          "oversized input rejected before reading");
    CHECK(Read() == Baseline, "oversized input preserves overlay");
    CHECK(Renderer.SetOverlay(nullptr, 0, Error), "empty input removes overlay");
    CHECK(Read() != Baseline, "empty overlay no longer draws white");
    CHECK(Renderer.SetOverlay(&Quad, 1, Error), "overlay restored after removal");
  }

  void CombinedFailure(UploadFailurePoint point, unsigned skip = 0) {
    const OverlayDraw::AtlasPixels black{.Rgba = kBlack.data(), .Width = 1, .Height = 1};
    const OverlayDraw::AtlasPixels white{.Rgba = kWhite.data(), .Width = 1, .Height = 1};
    const auto before = Arm(point, skip);
    CHECK(!Renderer.ReplaceOverlay(Replacement, &black, Error),
          "native upload failure rejects combined replacement");
    Reached(before);
    CHECK(Read() == Baseline, "combined failure preserves both previous atlas and geometry");
    CHECK(Renderer.ReplaceOverlay(Replacement, &black, Error), "combined retry succeeds");
    CHECK(Read() != Baseline, "combined retry publishes new pixels");
    CHECK(Renderer.ReplaceOverlay(std::span(&Quad, 1), &white, Error),
          "combined baseline restored");
  }

  void AtlasInputs() {
    for (const auto dimensions :
         {std::array{0, 1},
          std::array{1, -1},
          std::array{65536, 65536},
          std::array{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()}}) {
      CHECK(!Renderer.SetOverlayAtlas(kWhite.data(), dimensions[0], dimensions[1], Error),
            "invalid or overflowing dimensions rejected before reading texels");
      CHECK(Read() == Baseline, "invalid dimensions preserve old pixels");
    }
    CHECK(!Renderer.SetOverlayAtlas(nullptr, 1, 1, Error), "null texels rejected");
    CHECK(Read() == Baseline, "null texels preserve old pixels");
  }
};
}

int main() {
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  {
    OverlayFixture fixture;
    if (fixture.Open()) {
      fixture.AtlasFailure(UploadFailurePoint::Texture);
      for (const auto point : kUploadFailures) { fixture.AtlasFailure(point); }
      const auto allocated = fixture.Failure.VertexBufferAllocations;
      fixture.QuadFailure(UploadFailurePoint::DeviceBuffer);
      for (const auto point : kUploadFailures) { fixture.QuadFailure(point); }
      CHECK(fixture.Failure.VertexBufferAllocations == allocated + 1,
            "replacement reuses two geometry buffers after warming");
      fixture.QuadInputs();
      fixture.CombinedFailure(UploadFailurePoint::Texture);
      for (const auto point : kUploadFailures) { fixture.CombinedFailure(point); }
      fixture.CombinedFailure(UploadFailurePoint::Copy, 1);
      fixture.AtlasInputs();
    }
  }
  SDL_Quit();
  return Report();
}
