#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Check.h"
#include "Task.h"

namespace {

// RAGE WINS THIS ROW OVER UNREAL AND TARGET SAYS SO. A `CTask` owns sub-tasks, runs until it
// yields, and is abandoned as a whole SUBTREE when the situation changes: `drive to X` owns
// `follow the corridor` owns `keep the lane`, and each level knows only its own concern. Unreal's
// Behavior Tree re-decides from the root every tick and keeps its state in a blackboard beside the
// tree rather than in it.
//
// For a PHYSICAL actor the hierarchy is the act's own shape. A driver overtaking is not a different
// leaf of one flat selector -- it is a sub-task that owns the steering for its duration and hands
// control back when it finishes or is abandoned.
//
// THE ORACLE IS THE SEMANTICS AND NOT OUR IMPLEMENTATION, so the case counts what ACTED:
//
//   a parent does not act while its child runs   otherwise both levels steer at once
//   a finished child returns control upward      otherwise the tree stalls at its deepest leaf
//   abandoning a parent abandons what is under   otherwise an abandoned manoeuvre keeps steering
//                                                after the decision to stop it -- which is the
//                                                failure a flat selector cannot even express
//
// ONE LINE WAS DELETED BECAUSE NOTHING COULD SEE IT. `Abandon` first called `Under_->Abandon()`
// before dropping the child, and the negative control refused to confirm that call: `Step` tests
// its own abandoned flag before it reaches a child, so a subtree left un-flagged still never acts.
// Dropping the child destroys it, which is what abandonment IS, and the recursive call did nothing
// a test could distinguish. The case checks `Holds()` instead -- the drop is observable, the
// recursion was not.
constexpr double kStepS = 1.0 / 60.0;

class Counts final : public outshine::Control::Task {
public:
  Counts(std::vector<std::string> &into, std::string named, int forSteps)
      : Into_(into), Named_(std::move(named)), Left_(forSteps) {}

  void Under(std::unique_ptr<Task> task) { Hands(std::move(task)); }

protected:
  [[nodiscard]] outshine::Control::Doing Act(double) override {
    Into_.push_back(Named_);
    if (--Left_ > 0) { return outshine::Control::Doing::Running; }
    return outshine::Control::Doing::Done;
  }

private:
  std::vector<std::string> &Into_;
  std::string Named_;
  int Left_;
};

[[nodiscard]] size_t Acted(const std::vector<std::string> &log, const char *named) {
  size_t many = 0;
  for (const std::string &one : log) {
    if (one == named) { ++many; }
  }
  return many;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  using outshine::Control::Doing;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::vector<std::string> log;
  auto lane = std::make_unique<Counts>(log, "keep-the-lane", 3);
  auto corridor = std::make_unique<Counts>(log, "follow-the-corridor", 2);
  corridor->Under(std::move(lane));
  Counts drive(log, "drive-to-X", 2);
  drive.Under(std::move(corridor));
  const size_t deep = drive.Deep();

  std::printf("THE TREE STANDS %zu deep\n", deep);
  Doing was = Doing::Running;
  for (int step = 0; step < 12 && was == Doing::Running; ++step) { was = drive.Step(kStepS); }

  std::printf("KEPT THE LANE          %zu time(s)\n", Acted(log, "keep-the-lane"));
  std::printf("FOLLOWED THE CORRIDOR  %zu time(s)\n", Acted(log, "follow-the-corridor"));
  std::printf("DROVE TO X             %zu time(s)\n", Acted(log, "drive-to-X"));
  std::printf("AND THE ORDER WAS      ");
  for (const std::string &one : log) { std::printf("%s ", one.c_str()); }
  std::printf("\n");
  CHECK(deep == 3 && drive.Deep() == 1,
        "the tree stood three deep and stands one now, so what follows is read from a hierarchy "
        "that UNWOUND rather than from one task pretending to be several -- a depth measured "
        "after the run would read 1 and prove nothing about the shape it had");
  CHECK(log.size() >= 3 && log[0] == "keep-the-lane" && log[1] == "keep-the-lane" &&
            log[2] == "keep-the-lane",
        "**A PARENT DOES NOT ACT WHILE ITS CHILD RUNS**: the deepest task acts alone until it is "
        "done, which is what lets each level know only its own concern -- two levels steering in "
        "the same tick is a car pulled two ways");
  CHECK(Acted(log, "follow-the-corridor") == 2 && Acted(log, "drive-to-X") == 2,
        "and a finished child RETURNS CONTROL UPWARD: the corridor acts once the lane is done and "
        "the drive once the corridor is, so the tree unwinds rather than stalling at its deepest "
        "leaf");
  CHECK(was == Doing::Done,
        "and the root reports Done when everything under it has finished, so a caller learns the "
        "act is over without inspecting the tree");

  std::vector<std::string> after;
  auto held = std::make_unique<Counts>(after, "overtake", 100);
  Counts along(after, "along-the-road", 100);
  along.Under(std::move(held));
  (void)along.Step(kStepS);
  const size_t beforeAbandon = after.size();
  along.Abandon();
  const Doing left = along.Step(kStepS);
  std::printf("ABANDONED AFTER %zu act(s), then %zu more, reporting %s\n",
              beforeAbandon,
              after.size() - beforeAbandon,
              left == Doing::Abandoned ? "Abandoned" : "something else");

  CHECK(beforeAbandon == 1,
        "the manoeuvre acted once before it was abandoned, so the count below is a stop and not "
        "a tree that never ran");
  CHECK(!along.Holds(),
        "and the subtree is GONE rather than merely stopped: dropping it is what abandonment IS, "
        "and a tree that kept an abandoned child would hold whatever that child holds");
  CHECK(after.size() == beforeAbandon && left == Doing::Abandoned,
        "**ABANDONING A TASK ABANDONS EVERYTHING UNDER IT**: nothing acts after the decision to "
        "stop, and the root says so. An overtake that kept steering after being called off is "
        "the failure a flat selector cannot even express -- it has no subtree to abandon");

  Covers("the actor: a mind decomposes its act into a task tree -- a parent yields to its child, "
         "a finished child returns control upward, and abandoning a task abandons its whole "
         "subtree, which is RAGE's CTask and not a behaviour tree re-deciding from the root");
  return Report();
}
