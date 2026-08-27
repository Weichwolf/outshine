#include "EngineHeld.h"

namespace outshine {

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

  Quietly say;
  if (!World.Stack.Opened() &&
      !World.Stack.Open(Session.Under.Cache, Session.Under.Shipped,
                      {Data::ShippedProviders().begin(), Data::ShippedProviders().end()},
                      atLat, atLon, *World.Wire, say)) {
    Error = say.WhyNot();
    return false;
  }

  Around over;
  over.LatDeg = atLat;
  over.LonDeg = atLon;
  over.Zoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  over.Ring = 1;
  over.Awaited = true;
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
  if (!Picture.Canvas) {
    Error = "no canvas stands, so there is nowhere to draw -- the client hands one in through "
            "DrawsInto";
    return false;
  }
  Core::Declaration wanted = Picture.Shown;
  wanted.SurfaceWidthPx = Picture.Frame.WidthPx;
  wanted.SurfaceHeightPx = Picture.Frame.HeightPx;
  return Core::Live::Open(Picture.Device, std::move(wanted), &Picture.Face, Picture.Standing,
                          Error);
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
