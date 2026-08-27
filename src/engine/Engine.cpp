#include "EngineHeld.h"

namespace outshine {




namespace {


}

Engine::Engine() : S_(std::make_unique<State>()) {}

bool Engine::Assemble() {
  const Scenario &declared = S_->Declared;
  const size_t named = AssembledCapacity(declared);
  if (named == 0) {
    S_->Drove = false;
    return S_->Routes();
  }
  if (!S_->Scene.Open(named) || !S_->Bodies.Open(S_->Scene) ||
      !S_->Drives.Open(S_->Scene) || !S_->Kinds.Open(S_->Scene)) {
    S_->Error = "the scene did not open for the " + std::to_string(named) +
                " entities the declaration names";
    return false;
  }
  if (!outshine::Assemble(declared, S_->Scene, S_->Bodies, S_->Drives, S_->Kinds, S_->Stood,
                          S_->Error)) {
    return false;
  }
  if (!declared.Tables.empty()) {
    auto book = TableBook::Stand(declared.Tables);
    if (!book) {
      S_->Error = std::move(book).error();
      return false;
    }
    S_->Tabled.emplace(*std::move(book));
  }
  if (!declared.Buses.empty() || !declared.Sounds.empty()) {
    if (!S_->Sounding.Build(declared.Buses, declared.Sounds, S_->Error)) { return false; }
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
  const Scenario &declared = Declared;
  Drove = false;
  Freestanding.clear();
  for (const Body &stands : declared.Bodies) {
    if (!stands.Placed) { continue; }
    Physics::Body held;
    held.MassKg = stands.MassKg;
    for (int axis = 0; axis < 3; ++axis) {
      held.PositionM[axis] = stands.AtM[axis];
      held.InertiaKgM2[axis] = stands.InertiaKgM2[axis];
    }
    held.OrientationQ[0] = stands.FacingXyzw[3];
    held.OrientationQ[1] = stands.FacingXyzw[0];
    held.OrientationQ[2] = stands.FacingXyzw[1];
    held.OrientationQ[3] = stands.FacingXyzw[2];
    Freestanding.push_back(held);
  }
  if (!declared.Routed.Declared) { return true; }
  if (Assembles(declared.Routed.By) == nullptr) {
    Error = std::string("the scenario declares a journey travelling by ") +
            Travelling(declared.Routed.By) +
            ", and nothing assembles that -- a mode the engine cannot lay a corridor for is a "
            "refusal, never a journey that quietly does not happen";
    return false;
  }
  if (!Wire) {
    if (Under.Offline) {
      Wire = std::make_unique<Unwired>();
    } else {
      Wire = std::make_unique<Fetching>(Fetching::Config{});
    }
  }
  Quietly say;
  const Sim::Provision kept{Under.Cache, Under.Shipped,
                            {Data::ShippedProviders().begin(), Data::ShippedProviders().end()}};
  const bool routed = Assembles(declared.Routed.By)(Scene, Stood, Bodies, Drives, declared.Ground,
                                                    Stack, *Wire, kept, say, Drive);
  if (routed) {
    Stack.Restand(Drive.Way.FrameLat, Drive.Way.FrameLon);
    Surface = std::make_unique<Sim::GroundUnderfoot>(Stack, Drive.Surfaces);
    Surface->Restand();
  }
  Carried.insert(Carried.end(), std::make_move_iterator(say.Lines().begin()),
                     std::make_move_iterator(say.Lines().end()));
  Published.Stands(std::move(say.Numbers()));
  if (!routed) {
    Error = say.WhyNot();
    return false;
  }
  Published.Places("how long the corridor is", Drive.Way.Line.LengthM(), "m");
  Published.Places("how far along it the body has come", 0.0, "m");
  if (Standing && Drive.Stood.MetresPerAssetUnit > 0.0) {
    Standing->ScaledBy(Drive.Stood.MetresPerAssetUnit);
  }
  Drove = true;
  {
    const double slowestMs = Drive.Way.Profile.Quantile(0.01);
    if (!(slowestMs > 0.0)) {
      Error = "a hundredth of this speed plan stands still, so the drive has no pace to be "
              "bounded by and would never arrive -- p01 is " + Said(slowestMs) + " m/s";
      return false;
    }
    const double stepS = Declared.Motion.StepS > 0.0 ? Declared.Motion.StepS : 1.0;
    MostSteps = (size_t)(Drive.Way.Line.LengthM() / slowestMs / stepS) + 1u;
    Published.Places("the steps the plan allows at its slowest station", (double)MostSteps, "steps");
  }
  if (!Composes()) {
    Carried.push_back("the ground did not compose: " + Error);
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
  const auto standing = S_->Device.DrawsInto(widthPx, heightPx, presents);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return false;
  }
  S_->Frame = Extent{widthPx, heightPx};
  return true;
}

bool Engine::DrawsInto(Extent offscreen) {
  const auto standing = S_->Device.DrawsInto(offscreen.WidthPx, offscreen.HeightPx, nullptr);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return false;
  }
  S_->Frame = offscreen;
  return true;
}

void Engine::Under(Roots roots) { S_->Under = std::move(roots); }








namespace {


}

void Engine::Offers(Host *host) { S_->Offered = host; }



namespace {





}


void Engine::Offers(const Generates &maker) {
  for (const Generates *const stood : S_->Making) {
    if (stood->Kind() == maker.Kind()) { return; }
  }
  S_->Making.push_back(&maker);
}



namespace {


}







const std::vector<std::string> &Engine::Carried() const { return S_->Carried; }


const std::vector<Measure> &Engine::Numbers() const { return S_->Published.Numbers(); }

bool Engine::Takes(std::string_view view) {
  if (!S_->Views) {
    S_->Error = "the scenario declares no views, so there is none to take";
    return false;
  }
  if (!S_->Views->Take(view)) {
    S_->Error = "the scenario declares no view by that name";
    return false;
  }
  return true;
}













bool Engine::Standing() const { return S_->Standing != nullptr; }
const std::string &Engine::Error() const { return S_->Error; }

}
