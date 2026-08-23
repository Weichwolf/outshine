#include "Check.h"
#include "Mod.h"

#include <string>

using namespace outshine;
using namespace outshine::Test;

namespace {

const char *kOneOrder = R"({
  "schema": "outshine/mod/1", "name": "t", "subject": "one world scene and one studio scene",
  "scenes": [
    { "id": "w", "kind": "run", "fovDeg": 60,
      "stage": { "world": {
        "lat": 52.10602, "lon": 9.43453, "eyeM": 1.7, "yawDeg": 280, "pitchDeg": 0,
        "utc": "2026-08-06T17:40:00Z", "windDeg": 250, "windMs": 6.0, "cloudCover": 0.55,
        "viewKm": 12.5, "orthoM": 0 } },
      "jitter": [0.125, -0.375], "settleFrames": 7,
      "exposure": { "mode": "manual", "keyEv": -3.5 },
      "runs": [{ "kind": "motion", "frames": 1, "give": "stills", "path": "walk.png" }] },
    { "id": "s", "kind": "run", "fovDeg": 30,
      "stage": { "studio": {
        "substrate": { "class": "grass_thatch", "groundAslM": 100.6 },
        "keyLight": { "elevationDeg": 11.0 },
        "backdrop": "card",
        "subject": { "generator": "tree", "species": "beech", "leafMult": 2 } } },
      "runs": [{ "kind": "bench", "dir": "bench", "turnSteps": 4 }] }] })";

const char *kOtherOrder =
    "{\"scenes\":[{\"runs\":[{\"path\":\"walk.png\",\"give\":\"stills\",\"frames\":1,"
    "\"kind\":\"motion\"}],\"exposure\":{\"keyEv\":-3.5,\"mode\":\"manual\"},\"settleFrames\":7,"
    "\"jitter\":[0.125,-0.375],\"stage\":{\"world\":{\"orthoM\":0,\"viewKm\":12.5,"
    "\"cloudCover\":0.55,\"windMs\":6.0,\"windDeg\":250,\"utc\":\"2026-08-06T17:40:00Z\","
    "\"pitchDeg\":0,\"yawDeg\":280,\"eyeM\":1.7,\"lon\":9.43453,\"lat\":52.10602}},"
    "\"fovDeg\":60,\"kind\":\"run\",\"id\":\"w\"},"
    "{\"runs\":[{\"turnSteps\":4,\"dir\":\"bench\",\"kind\":\"bench\"}],"
    "\"stage\":{\"studio\":{\"subject\":{\"leafMult\":2,\"species\":\"beech\","
    "\"generator\":\"tree\"},\"backdrop\":\"card\",\"keyLight\":{\"elevationDeg\":11.0},"
    "\"substrate\":{\"groundAslM\":100.6,\"class\":\"grass_thatch\"}}},"
    "\"fovDeg\":30,\"kind\":\"run\",\"id\":\"s\"}],"
    "\"subject\":\"one world scene and one studio scene\",\"name\":\"t\","
    "\"schema\":\"outshine/mod/1\"}";

}

int main() {
  Covers("I.4 the same scenario declared twice in different arrival orders gives the same answer");

  SceneLegacy::Mod a, b;
  CHECK(a.Read(kOneOrder, "a.json"), "the declaration loads in one property order");
  CHECK(b.Read(kOtherOrder, "b.json"), "and in the reverse order, laid out differently");
  if (a.Scenes().size() != 2 || b.Scenes().size() != 2) return Report();

  const SceneLegacy::Scene &wa = a.Scenes()[0], &wb = b.Scenes()[0];
  const SceneLegacy::WorldStage *pa = wa.Staged().AsWorld(), *pb = wb.Staged().AsWorld();
  CHECK(pa && pb, "both read a world stage for the first scene");
  if (!pa || !pb) return Report();

  CHECK(wa.Id() == wb.Id() && wa.FovDeg() == wb.FovDeg(), "the scene's id and lens are the same");
  CHECK(pa->Where.LatDeg() == pb->Where.LatDeg() && pa->Where.LonDeg() == pb->Where.LonDeg(),
        "the standpoint is bit-identical");
  CHECK(pa->EyeAglM == pb->EyeAglM && pa->YawDeg == pb->YawDeg && pa->PitchDeg == pb->PitchDeg,
        "and so is the eye");
  CHECK(pa->UtcS == pb->UtcS && pa->WindFromDeg == pb->WindFromDeg && pa->WindMs == pb->WindMs &&
            pa->CloudCover == pb->CloudCover,
        "and so are the clock and the air");
  CHECK(pa->ViewM == pb->ViewM && pa->OrthoM == pb->OrthoM, "and so is the view");
  CHECK(wa.JitterPinX() == wb.JitterPinX() && wa.JitterPinY() == wb.JitterPinY(),
        "the pinned sub-pixel offset is bit-identical, which is what a depth channel stands on");
  CHECK(wa.SettleFrames() == wb.SettleFrames(), "and so is the temporal history every frame carries");
  CHECK(wa.Exposure().KeyEv == wb.Exposure().KeyEv, "and so is the exposure placement");

  const SceneLegacy::StudioStage *sa = a.Scenes()[1].Staged().AsStudio();
  const SceneLegacy::StudioStage *sb = b.Scenes()[1].Staged().AsStudio();
  CHECK(sa && sb, "both read a studio stage for the second scene");
  if (!sa || !sb) return Report();
  CHECK(sa->Ground.MaterialClass == sb->Ground.MaterialClass &&
            sa->Ground.GroundAslM == sb->Ground.GroundAslM,
        "the substrate is the same");
  CHECK(sa->Key.ElevationDeg == sb->Key.ElevationDeg, "and so is the key light");
  CHECK(sa->Behind == sb->Behind, "and so is the backdrop");
  const auto *ta = std::get_if<SceneLegacy::TreeSubject>(&sa->Stands.What);
  const auto *tb = std::get_if<SceneLegacy::TreeSubject>(&sb->Stands.What);
  CHECK(ta && tb && ta->Species == tb->Species && ta->LeafMult == tb->LeafMult &&
            ta->HeightM == tb->HeightM && ta->Leaf == tb->Leaf,
        "and so is the subject, generator and every parameter that generator declares");
  return Report();
}
