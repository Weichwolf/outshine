#include "EngineHeld.h"

namespace outshine {




namespace {


}

Engine::Engine() : S_(std::make_unique<State>()) {}

bool Engine::Assemble() {
  if (!S_->Session.Taken) {
    S_->Error = "a declaration was read and never DECLARED, so this engine holds a scenario it "
                "was not asked to stand -- Read fills the declaration, Declare hands it over, "
                "and Assemble builds what Declare stood";
    return false;
  }
  const Scenario &declared = S_->Session.Declared;
  const size_t named = AssembledCapacity(declared);
  if (named == 0) {
    S_->Ticking.Drove = false;
    return S_->Routes();
  }
  if (!S_->Cast.Scene.Open(named) || !S_->Cast.Bodies.Open(S_->Cast.Scene) ||
      !S_->Cast.Drives.Open(S_->Cast.Scene) || !S_->Cast.Kinds.Open(S_->Cast.Scene)) {
    S_->Error = "the scene did not open for the " + std::to_string(named) +
                " entities the declaration names";
    return false;
  }
  if (!outshine::Assemble(declared, S_->Cast.Scene, S_->Cast.Bodies, S_->Cast.Drives, S_->Cast.Kinds, S_->Cast.Stood,
                          S_->Error)) {
    return false;
  }
  if (!declared.Tables.empty()) {
    auto book = TableBook::Stand(declared.Tables);
    if (!book) {
      S_->Error = std::move(book).error();
      return false;
    }
    S_->Session.Tabled.emplace(*std::move(book));
  }
  if (!declared.Buses.empty() || !declared.Sounds.empty()) {
    S_->Session.Mixing = false;
  }

  return S_->Routes();
}

namespace {

using Assembler = bool (*)(const Store &, const Assembled &, const Column<Body> &,
                           const Column<Journey> &, const WorldSettings &, Ground::GroundStack &,
                           Data::Transport &, const Sim::Provision &, Sink &, Sim::DriveProduct &);

struct Travelling_ {
  Travels By;
  const char *Named;
  Assembler How;
};

constexpr size_t kTravels = 4;

const Travelling_ kAssemblers[kTravels] = {
    {Travels::Walk, "foot", nullptr},
    {Travels::Drive, "road", &Sim::AssembleDrive},
    {Travels::Fly, "air", nullptr},
    {Travels::Rail, "rail", nullptr},
};

[[nodiscard]] Assembler Assembles(Travels by) {
  for (const Travelling_ &one : kAssemblers) {
    if (one.By == by) { return one.How; }
  }
  return nullptr;
}

[[nodiscard]] const char *Travelling(Travels by) {
  for (const Travelling_ &one : kAssemblers) {
    if (one.By == by) { return one.Named; }
  }
  return "an unnamed way";
}

}

bool Engine::State::Routes(void) {
  const Scenario &declared = Session.Declared;
  Ticking.Drove = false;
  Ticking.Freestanding.clear();
  for (const Body &stands : declared.Bodies) {
    if (!stands.Placed) { continue; }
    Physics::Rigid held;
    held.MassKg = stands.MassKg;
    for (int axis = 0; axis < 3; ++axis) {
      held.PositionM[axis] = stands.Stands.AtM[axis];
      held.InertiaKgM2[axis] = stands.InertiaKgM2[axis];
    }
    held.OrientationQ[0] = stands.Stands.FacingXyzw[3];
    held.OrientationQ[1] = stands.Stands.FacingXyzw[0];
    held.OrientationQ[2] = stands.Stands.FacingXyzw[1];
    held.OrientationQ[3] = stands.Stands.FacingXyzw[2];
    Ticking.Freestanding.push_back(held);
  }
  if (!declared.Routed.Declared) { return true; }
  if (Assembles(declared.Routed.By) == nullptr) {
    Error = std::string("the scenario declares a journey travelling by ") +
            Travelling(declared.Routed.By) +
            ", and nothing assembles that -- a mode the engine cannot lay a corridor for is a "
            "refusal, never a journey that quietly does not happen";
    return false;
  }
  if (!World.Wire) {
    if (Session.Under.Offline) {
      World.Wire = std::make_unique<Unwired>();
    } else {
      World.Wire = std::make_unique<Fetching>(Fetching::Config{});
    }
  }
  Collecting say;
  const Sim::Provision kept{Session.Under.Cache, Session.Under.Shipped,
                            {Data::ShippedProviders().begin(), Data::ShippedProviders().end()}};
  const bool routed = Assembles(declared.Routed.By)(Cast.Scene, Cast.Stood, Cast.Bodies, Cast.Drives, declared.Ground,
                                                    World.Stack, *World.Wire, kept, say, Ticking.Drive);
  if (routed) {
    World.Stack.Restand(Ticking.Drive.Way.FrameLat, Ticking.Drive.Way.FrameLon);
    Ticking.Surface = std::make_unique<Sim::GroundSupport>(World.Stack, Ticking.Drive.Surfaces);
    Ticking.Surface->Restand();
  }
  Session.Carried.insert(Session.Carried.end(), std::make_move_iterator(say.Lines().begin()),
                     std::make_move_iterator(say.Lines().end()));
  Published.Stands(std::move(say.Numbers()));
  if (!routed) {
    Error = say.WhyNot();
    return false;
  }
  Published.Places("how long the corridor is", Ticking.Drive.Way.Line.LengthM(), "m");
  Published.Places("how far along it the body has come", 0.0, "m");
  if (Picture.Standing && Ticking.Drive.Stood.MetresPerAssetUnit > 0.0) {
    Picture.Standing->ScaledBy(Ticking.Drive.Stood.MetresPerAssetUnit);
  }
  Ticking.Drove = true;
  {
    const double slowestMs = Ticking.Drive.Way.Profile.Quantile(0.01);
    if (!(slowestMs > 0.0)) {
      Error = "a hundredth of this speed plan stands still, so the drive has no pace to be "
              "bounded by and would never arrive -- p01 is " + Said(slowestMs) + " m/s";
      return false;
    }
    const double stepS = Session.Declared.Motion.StepS > 0.0 ? Session.Declared.Motion.StepS : 1.0;
    Ticking.MostSteps = (size_t)(Ticking.Drive.Way.Line.LengthM() / slowestMs / stepS) + 1u;
    Published.Places("the steps the plan allows at its slowest station", (double)Ticking.MostSteps, "steps");
  }
  if (!Composes()) {
    Session.Carried.push_back("the ground did not compose: " + Error);
    Error.clear();
  } else if (!Rides()) {
    return false;
  }
  return true;
}

Engine::~Engine() = default;
Engine::Engine(Engine &&) noexcept = default;
Engine &Engine::operator=(Engine &&) noexcept = default;

bool Engine::DrawsInto(SDL_Window *presents) {
  if (presents == nullptr) {
    S_->Error = "a window is what DrawsInto presents on, and this one is none -- an engine that "
                "draws nowhere is declared with an Extent instead";
    return false;
  }
  int widthPx = 0, heightPx = 0;
  SDL_GetWindowSizeInPixels(presents, &widthPx, &heightPx);
  S_->Picture.Targeted = true;
  const auto standing = S_->Picture.Device.DrawsInto(widthPx, heightPx, presents);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return false;
  }
  S_->Picture.Frame = Extent{widthPx, heightPx};
  return true;
}

bool Engine::DrawsInto(Extent offscreen) {
  S_->Picture.Targeted = true;
  const auto standing = S_->Picture.Device.DrawsInto(offscreen.WidthPx, offscreen.HeightPx, nullptr);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return false;
  }
  S_->Picture.Frame = offscreen;
  return true;
}

void Engine::Under(Roots roots) { S_->Session.Under = std::move(roots); }








namespace {


}

void Engine::Offers(Host *host) { S_->Offered = host; }



namespace {





}


void Engine::Offers(const Generates &maker) { (void)S_->World.Offering.Offers(maker); }



namespace {


}







const std::vector<std::string> &Engine::Carried() const { return S_->Session.Carried; }


const std::vector<Measure> &Engine::Numbers() const { return S_->Published.Numbers(); }

bool Engine::Takes(std::string_view view) {
  if (!S_->Session.Views) {
    S_->Error = "the scenario declares no views, so there is none to take";
    return false;
  }
  if (!S_->Session.Views->Take(view)) {
    S_->Error = "the scenario declares no view by that name";
    return false;
  }
  return true;
}













bool Engine::Standing() const { return S_->Picture.Standing != nullptr; }
const std::string &Engine::Error() const { return S_->Error; }

}
