#include "SceneCandidateEdit.h"
#include "SceneRenderer.h"
#include "Lens.h"
#include "Viewing.h"
#include "Check.h"
#include <optional>
#include <string>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;

void SetNear(SceneRenderer &renderer, float nearM) {
  renderer.SetCamera(CameraBasis{},
                     Lens{.WidePx = 32, .HighPx = 32, .FovDeg = 60, .NearM = nearM, .FarM = 100});
}

void NestedMoves() {
  SceneRenderer renderer;
  SetNear(renderer, 1);
  std::string error;
  const bool began = renderer.BeginsWorldCandidate(error);
  CHECK(began, error.c_str());
  if (!began) { return; }
  {
    std::optional<SceneCandidateEdit> initial(renderer.EditWorldCandidate());
    SetNear(renderer, 2);
    CHECK(renderer.NearMetres() == 2, "a live edit selects the candidate");
    {
      const auto published = renderer.PublishedWorld();
      CHECK(renderer.NearMetres() == 1, "published reads override an active candidate edit");
    }
    CHECK(renderer.NearMetres() == 2, "leaving the published view resumes the candidate");
    SceneCandidateEdit moved(std::move(*initial));
    initial.reset();
    CHECK(renderer.NearMetres() == 2, "resetting a moved-from lease preserves the active edit");
    {
      auto nested = renderer.EditWorldCandidate();
      moved = std::move(nested);
    }
    CHECK(renderer.NearMetres() == 2, "move assignment releases one lease and retains the other");
    auto &same = moved;
    moved = std::move(same);
    CHECK(renderer.NearMetres() == 2, "self-move preserves the edit");
  }
  CHECK(renderer.NearMetres() == 1, "the last lease restores the published selection");
  {
    const auto abandoned = renderer.EditWorldCandidate();
    renderer.AbandonsWorldCandidate();
    CHECK(!renderer.HasWorldCandidate() && renderer.NearMetres() == 1,
          "abandoning the candidate restores published state before the lease retires");
  }
  CHECK(renderer.BeginsWorldCandidate(error), "the abandoned edit does not block a new candidate");
  CHECK(renderer.NearMetres() == 1, "the new candidate has no inherited edit lease");
  renderer.AbandonsWorldCandidate();
}

void DifferentOwners() {
  SceneRenderer first;
  SceneRenderer second;
  SetNear(first, 1);
  SetNear(second, 10);
  std::string error;
  const bool began = first.BeginsWorldCandidate(error) && second.BeginsWorldCandidate(error);
  CHECK(began, error.c_str());
  if (!began) { return; }
  std::optional<SceneCandidateEdit> owner(first.EditWorldCandidate());
  auto incoming = second.EditWorldCandidate();
  SetNear(first, 2);
  SetNear(second, 20);
  *owner = std::move(incoming);
  CHECK(first.NearMetres() == 1 && second.NearMetres() == 20,
        "assignment releases the old renderer and transfers the new renderer's lease");
  owner.reset();
  CHECK(first.NearMetres() == 1 && second.NearMetres() == 10,
        "reset releases only the transferred renderer's edit");
  first.AbandonsWorldCandidate();
  second.AbandonsWorldCandidate();
}
}

int main() {
  NestedMoves();
  DifferentOwners();
  return outshine::Test::Report();
}
