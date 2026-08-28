#include "EngineHeld.h"
#include "Species.h"

namespace outshine {

bool Engine::State::Grows(double atLat, double atLon) {
  if (World.Placed > 0 || World.Placing.empty() || !World.Table ||
      World.Stack.Vectors() == nullptr) {
    return false;
  }
  const Generators::Tile region =
      Generators::Tile::Of(World.Stack.Vectors()->Zoom(), atLat, atLon);
  Generators::Fields stands;
  stands.Vectors = World.Stack.Vectors();
  stands.Footprints = &World.Stack.Footprints();
  stands.WaterBodies = &World.Stack.WaterBodies();
  stands.Ways = &World.Stack.Ways();
  Generators::Ground::Snapshot snapshot;
  const Generators::Snapped how = Generators::SnapshotOver(
      region, World.Stack.Ground(), World.Stack.Classes(), stands, World.Table, &snapshot);
  World.Reached = 40 + (snapshot.Patch ? 1 : 0) + (snapshot.Classes ? 2 : 0) +
                  (snapshot.Features ? 4 : 0);
  if (how != Generators::Snapped::Taken) { return false; }
  const std::optional<Generators::Ground> over = Generators::Ground::Of(region, snapshot);
  if (!over) { return false; }
  Generators::RegionPool::Shape shape;
  Generators::RegionPool::Extent extent{over->Where(), over->Where()};
  Generators::RegionPool pool(extent, shape);
  std::optional<Generators::RegionPool::Lease> lease = pool.TryAcquire(*over);
  if (!lease) { return false; }
  for (const Generators::Making *const stood : World.Placing) {
    std::vector<Generators::Yield::Note> notes(stood->NoteNames().Size());
    Generators::Yield yield(lease->Sink(), stood->NoteNames(),
                            Span<Generators::Yield::Note>(notes.data(), notes.size()));
    stood->Occupy(*over, yield);
    World.Placed += yield.Placed().Count;
  }
  return World.Placed > 0;
}

bool Engine::State::Composes(void) {
  World.GroundTiles = 0;
  if (!Picture.Standing) {
    Error = "nothing stands to compose a world around";
    return false;
  }
  const Scenario &declared = Session.Declared;
  const Sim::Corridor &way = Ticking.Drive.Way;
  const bool overADrive = Ticking.Drove && !way.Fine.empty();
  if (!declared.Ground.Declared && !overADrive) {
    Error = "the scenario declares neither a sphere nor a drive that laid a corridor, so there "
            "is nowhere for a ground to be composed";
    return false;
  }
  const double atLat = overADrive ? way.FrameLat : declared.Ground.Lat;
  const double atLon = overADrive ? way.FrameLon : declared.Ground.Lon;
  if (!World.Wire) {
    if (Session.Under.Offline) {
      Error = "the ground is FETCHED and the engine was declared offline";
      return false;
    }
    World.Wire = std::make_unique<Fetching>(Fetching::Config{});
  }

  Collecting say;
  if (!World.Stack.Opened() &&
      !World.Stack.Open(Session.Under.Cache, Session.Under.Shipped,
                      {Data::ShippedProviders().begin(), Data::ShippedProviders().end()},
                      atLat, atLon, *World.Wire, say)) {
    Error = say.WhyNot();
    return false;
  }

  if (World.Placing.empty() && World.Stack.Vegetated()) {
    const Ground::VegetationTemplates &veg = World.Stack.Vegetation();
    World.TreesPerM2.clear();
    for (size_t row = 0; row < veg.TemplateCount(); ++row) {
      World.TreesPerM2.push_back(veg.Rows()[row].Edge[2]);
    }
    std::vector<Generators::TreeSpecies> species;
    std::string why;
    if (Generators::ReadSpecies((std::string(Session.Under.Shipped) + "/world/species").c_str(),
                                species, why)) {
      World.Stems.clear();
      for (const Generators::TreeSpecies &one : species) {
        Generators::Forest::Stem stem;
        stem.HeightM = (double)one.HeightM();
        World.Stems.push_back(stem);
      }
      if (!World.Stems.empty() && !World.TreesPerM2.empty()) {
        World.Woods = std::make_unique<Generators::Forest>(
            Span<const Generators::Forest::Stem>(World.Stems.data(), World.Stems.size()),
            Span<const float>(World.TreesPerM2.data(), World.TreesPerM2.size()), veg.Limit());
        World.Placing.push_back(World.Woods.get());
      }
    }
    World.Table = Generators::TableOf(veg);
  }

  Around over;
  over.LatDeg = atLat;
  over.LonDeg = atLon;
  over.Zoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  over.Ring = 1;
  over.Awaited = true;
  if (Picture.Frame.HeightPx > 0) {
    const double halfFov = 0.5 * 55.0 * std::numbers::pi / 180.0;
    over.FocalPx = (float)(0.5 * (double)Picture.Frame.HeightPx / std::tan(halfFov));
  }
  auto laid = LayPatchwork(World.Stack.Pool(), over);
  if (!laid) {
    Error = laid.error();
    return false;
  }

  std::vector<float> inFrame;
  if (overADrive) {
    inFrame.resize(laid->PositionM.size());
    for (size_t at = 0; at + 2 < laid->PositionM.size(); at += 3) {
      const Ground::Ecef held{laid->OriginEcef[0] + (double)laid->PositionM[at],
                              laid->OriginEcef[1] + (double)laid->PositionM[at + 1],
                              laid->OriginEcef[2] + (double)laid->PositionM[at + 2]};
      const Ground::Geo where = Ground::EcefToGeoWgs84(held);
      inFrame[at] = (float)((where.LonDeg - way.FrameLon) * way.PerLonM);
      inFrame[at + 1] = (float)where.AltM;
      inFrame[at + 2] = (float)(-(where.LatDeg - way.FrameLat) * way.PerLatM);
    }
  }

  if (overADrive) {
    double nearest = 1.0e30, atUp = 0.0;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const double east = (double)inFrame[at], south = (double)inFrame[at + 2];
      const double away = east * east + south * south;
      if (away >= nearest) { continue; }
      nearest = away;
      atUp = (double)inFrame[at + 1];
    }
    Published.Places("the ring's nearest vertex to the frame origin", std::sqrt(nearest), "m");
    Published.Places("and its up", atUp, "m");
  }
  if (overADrive) {
    for (size_t at = 0; at + 2 < laid->Index.size(); at += 3) {
      std::swap(laid->Index[at + 1], laid->Index[at + 2]);
    }
    const double lat = way.FrameLat * std::numbers::pi / 180.0;
    const double lon = way.FrameLon * std::numbers::pi / 180.0;
    const double sinLat = std::sin(lat), cosLat = std::cos(lat);
    const double sinLon = std::sin(lon), cosLon = std::cos(lon);
    const double east[3] = {-sinLon, cosLon, 0.0};
    const double north[3] = {-sinLat * cosLon, -sinLat * sinLon, cosLat};
    const double upward[3] = {cosLat * cosLon, cosLat * sinLon, sinLat};
    for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
      const double held[3] = {(double)laid->NormalM[at], (double)laid->NormalM[at + 1],
                              (double)laid->NormalM[at + 2]};
      double alongEast = 0.0, alongUp = 0.0, alongNorth = 0.0;
      for (int axis = 0; axis < 3; ++axis) {
        alongEast += east[axis] * held[axis];
        alongUp += upward[axis] * held[axis];
        alongNorth += north[axis] * held[axis];
      }
      laid->NormalM[at] = (float)alongEast;
      laid->NormalM[at + 1] = (float)alongUp;
      laid->NormalM[at + 2] = (float)(-alongNorth);
    }
  }
  {
    double up = 0.0, down = 0.0, sideways = 0.0, unlengthed = 0.0;
    for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
      const double x = laid->NormalM[at], y = laid->NormalM[at + 1], z = laid->NormalM[at + 2];
      const double length = std::sqrt(x * x + y * y + z * z);
      if (!(length > 0.5)) { unlengthed += 1.0; continue; }
      const double upward = y / length;
      if (upward > 0.5) { up += 1.0; }
      else if (upward < -0.5) { down += 1.0; }
      else { sideways += 1.0; }
    }
    Published.Places("the ring's normals that point up", up, "normals");
    Published.Places("its normals that point DOWN", down, "normals");
    Published.Places("its normals that lie sideways", sideways, "normals");
    {
      double steepest = 0.0, mean = 0.0, counted = 0.0;
      for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
        const double x = laid->NormalM[at], y = laid->NormalM[at + 1], z = laid->NormalM[at + 2];
        const double length = std::sqrt(x * x + y * y + z * z);
        if (!(length > 1.0e-6)) { continue; }
        const double leanDeg = std::acos(std::fmin(1.0, y / length)) * 180.0 / std::numbers::pi;
        steepest = leanDeg > steepest ? leanDeg : steepest;
        mean += leanDeg;
        counted += 1.0;
      }
      Published.Places("the steepest the ring's surface leans", steepest, "deg");
      Published.Places("how far it leans on average", counted > 0.0 ? mean / counted : 0.0, "deg");
    }
    Published.Places("its normals with no length at all", unlengthed, "normals");
    Published.Places("its normals in all", (double)(laid->NormalM.size() / 3), "normals");
    {
      double least = 1.0e30, most = -1.0e30;
      const std::vector<float> &held = overADrive ? inFrame : laid->PositionM;
      for (size_t at = 1; at < held.size(); at += 3) {
        const double y = (double)held[at];
        if (y < least) { least = y; }
        if (y > most) { most = y; }
      }
      Published.Places("the ring's lowest vertex", least, "m");
      Published.Places("its highest", most, "m");
    }
  }
  Geometry ground;
  const int ringPart = ground.Part("ground", 0);
  (void)ground.Positions(ringPart, overADrive
                                       ? std::span<const float>(inFrame.data(), inFrame.size())
                                       : std::span<const float>(laid->PositionM.data(),
                                                                laid->PositionM.size()));
  (void)ground.Normals(ringPart,
                       std::span<const float>(laid->NormalM.data(), laid->NormalM.size()));
  (void)ground.Triangles(ringPart, std::span<const uint32_t>(laid->Index.data(),
                                                             laid->Index.size()));

  Gltf::Subject laidGround;
  if (!laidGround.Assemble(ground)) {
    Error = laidGround.Error();
    return false;
  }
  const Render::Medium air;
  Material wearing;
  for (int channel = 0; channel < 3; ++channel) {
    wearing.BaseColour[channel] = air.GroundAlbedo[channel];
  }
  const size_t drivenParts = Picture.Standing->Shown().Parts().size();
  if (!Picture.Standing->Restand(laidGround, drivenParts, wearing, Error)) { return false; }
  World.GroundTiles = laid->Tiles;
  Published.Places("tiles the ring laid", (double)laid->Tiles, "tiles");
  Published.Places("tiles it is still waiting for", (double)laid->Pending, "tiles");
  Published.Places("tiles the stack does not hold", (double)laid->Absent, "tiles");
  Published.Places("tiles it refused", (double)laid->Refused, "tiles");
  return true;
}

bool Engine::State::Stood(void) {
  if (Picture.Standing) { return true; }
  if (!Picture.Targeted) {
    Error = "no canvas stands, so there is nowhere to draw -- the client hands one in through "
            "DrawsInto";
    return false;
  }
  Core::Declaration wanted = Picture.Shown;
  wanted.SurfaceWidthPx = Picture.Frame.WidthPx;
  wanted.SurfaceHeightPx = Picture.Frame.HeightPx;
  if (!Core::Live::Open(Picture.Device, std::move(wanted), &Picture.Face, Picture.Standing,
                        Error)) {
    return false;
  }
  if (!Picture.Carrying) { return true; }
  Picture.Carrying = false;
  return Picture.Standing->Restand(Picture.Handed, 0, Error);
}

void Engine::State::Blocks(const Gltf::Subject &standing) {
  const std::vector<double> &positionsM = standing.PositionsM();
  std::vector<float> corners(positionsM.size());
  for (size_t at = 0; at < positionsM.size(); ++at) { corners[at] = (float)positionsM[at]; }
  World.Blocking = TriangleBvh::Over(Span<const float>(corners.data(), corners.size()),
                                     Span<const uint32_t>(standing.Indices().data(),
                                                          standing.Indices().size()));
}

void Engine::State::Tells(void) {
  const unsigned next = (Session.Told.load(std::memory_order_relaxed) + 1u) & 1u;
  std::vector<Audio::Heard> &sources = Session.Sources[next];
  sources.clear();
  sources.reserve(Session.Declared.Sounds.size());
  for (const Sound &declared : Session.Declared.Sounds) {
    Audio::Heard where;
    where.Id = declared.Id;
    if (declared.On.empty()) {
      where.Standing = !declared.Heard.Positional;
      sources.push_back(where);
      continue;
    }
    const Physics::Rigid *stood = nullptr;
    if (Ticking.Drove && Session.Declared.Bodies.size() == 1) {
      stood = &Ticking.Drive.State.Body;
    } else if (!Ticking.Freestanding.empty()) {
      stood = &Ticking.Freestanding.front();
    }
    if (stood != nullptr) {
      where.Standing = true;
      for (int axis = 0; axis < 3; ++axis) {
        where.AtM[axis] = stood->PositionM[axis];
        where.VelocityMs[axis] = stood->VelocityMs[axis];
      }
      where.Blocked = Blocked(where.AtM) ? 1.0 : 0.0;
    }
    sources.push_back(where);
  }

  Audio::Listening &ear = Session.Ear[next];
  ear = Audio::Listening{};
  if (Picture.Standing) {
    const Gltf::Viewpoint &eye = Picture.Standing->Aimed();
    for (int axis = 0; axis < 3; ++axis) {
      ear.AtM[axis] = eye.EyeM[axis];
      ear.ForwardXyz[axis] = eye.Forward[axis];
      ear.RightXyz[axis] = eye.Right[axis];
    }
  }  Session.Told.store(next, std::memory_order_release);
}

bool Engine::State::Blocked(const double sourceM[3]) const {
  if (World.Blocking.Empty() || !Picture.Standing) { return false; }
  const Gltf::Viewpoint &eye = Picture.Standing->Aimed();
  float fromM[3], along[3];
  double awayM = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double step = sourceM[axis] - eye.EyeM[axis];
    awayM += step * step;
  }
  awayM = std::sqrt(awayM);
  if (!(awayM > 0.0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) {
    fromM[axis] = (float)eye.EyeM[axis];
    along[axis] = (float)((sourceM[axis] - eye.EyeM[axis]) / awayM);
  }
  return World.Blocking.Occludes(fromM, along, 0.01f, (float)awayM);
}

bool Engine::Mixes(std::span<float> stereo, int rate) {
  if (!S_->Session.Mixing) {
    if (!S_->Session.Sounding.Stands(S_->Session.Declared.Buses, S_->Session.Declared.Sounds,
                                     rate, S_->Error)) {
      return false;
    }
    S_->Session.Mixing = true;
    S_->Tells();
  }
  const unsigned told = S_->Session.Told.load(std::memory_order_acquire);
  return S_->Session.Sounding.Fills(stereo, S_->Session.Sources[told],
                                    S_->Session.Ear[told], S_->Error);

}

bool Engine::RenderTo(Extent frame) {
  if (!S_->Stood()) { return false; }
  if (frame.WidthPx > 0 && frame.HeightPx > 0 &&
      (frame.WidthPx != S_->Picture.Frame.WidthPx || frame.HeightPx != S_->Picture.Frame.HeightPx)) {
    S_->Error = "this engine stands on a " + std::to_string(S_->Picture.Frame.WidthPx) + "x" +
                std::to_string(S_->Picture.Frame.HeightPx) + " canvas and was asked to draw " +
                std::to_string(frame.WidthPx) + "x" + std::to_string(frame.HeightPx) +
                " -- a canvas is declared before a scenario stands on it";
    return false;
  }
  if (!S_->Picture.Standing->Draw(S_->Error)) { return false; }
  S_->Drew();
  return true;
}

bool Engine::Pixels(std::vector<uint8_t> &rgba) {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be read -- a scenario is declared before a frame carries pixels";
    return false;
  }
  S_->Picture.Device.WantsPixels();
  if (!S_->Picture.Standing->Draw(S_->Error)) { return false; }
  return S_->Picture.Standing->ReadPixels(rgba, S_->Error);
}

bool Engine::Capture(std::string_view path) {
  if (!S_->Stood()) { return false; }
  if (!S_->Picture.Standing) {
    S_->Error = "nothing stands to be captured -- a scenario is declared before a frame is kept";
    return false;
  }
  S_->Picture.Device.WantsPixels();
  if (!S_->Picture.Standing->Draw(S_->Error)) { return false; }
  return S_->Picture.Standing->Screenshot(std::string(path), S_->Error);
}

}
