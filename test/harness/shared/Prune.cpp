/* THE HARNESS'S OWN PRUNE (board:1181), beside its own clock and judged by neither.
 *
 *   prune <case directory> <marker file>
 *
 * IT IS NOT A TEST AND HOLDS NO VERDICT. `test/run.sh` runs a case's three arms and then runs this
 * over that one directory, always -- pass or fail -- so the working set is one case rather than the
 * whole corpus. What it decides is only whether a file's bytes exist somewhere else; what a case is
 * WORTH is the trailer's business and it never reads one.
 *
 * ONE LINE ON STDOUT FOR THE RUNNER, EVERYTHING ELSE ON STDERR FOR THE LOG. A file that stays is the
 * interesting output and it names the proof that refused it, because the next reader's question is
 * "why is this still here" and the answer must not be "read the header". */
#include <cstdio>
#include <string>
#include <vector>

#include "Prune.h"

namespace {

using outshine::Prune::Examination;
using outshine::Prune::Verdict;

const char *Word(const Examination &examination) {
  switch (examination.What) {
    case Verdict::Prunable: return "PRUNED";
    case Verdict::Kept: return "KEPT";
    case Verdict::Stays: break;
  }
  return "STAYS";
}

const char *Because(const Examination &examination) {
  if (examination.Ticket.has_value()) { return examination.Ticket->Evidence().c_str(); }
  return examination.Why.c_str();
}

/* THE RECOVERY STEP, IN THE RUNNER'S OWN OUTPUT, so a diagnosis does not begin with "how do I see
 * the pixels again". Two producers, two commands, and neither rebuilds anything on a warm store. */
void NameTheRecovery(const std::string &directory) {
  std::fprintf(stderr, "  the oracle half comes back with: python3 test/outshine/corpus/prepare.py all "
                       "--manifest %smanifest.json\n",
               directory.c_str());
  std::fprintf(stderr, "  our own dumps come back by re-running this case: sh test/run.sh\n");
}

/* The removal and the line that says it happened are one loop on purpose: a report written from the
 * examination alone would say a file went when the unlink had refused. */
size_t RemoveAndSay(const std::vector<Examination> &examinations) {
  size_t refused = 0;
  for (const Examination &examination : examinations) {
    if (examination.Ticket.has_value() && !outshine::Prune::Remove(*examination.Ticket)) {
      ++refused;
      std::fprintf(stderr, "  UNLINK %s could not be removed\n",
                   examination.Path.filename().string().c_str());
      continue;
    }
    std::fprintf(stderr, "  %-6s %12llu  %-28s %s\n", Word(examination),
                 static_cast<unsigned long long>(examination.Bytes),
                 examination.Path.filename().string().c_str(), Because(examination));
  }
  return refused;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::fprintf(stderr, "prune: usage: prune <case directory> <marker file>\n");
    return 2;
  }
  const std::string directory = std::string(argv[1]) + "/";
  const outshine::Prune::CaseReading reading = outshine::Prune::ReadCase(directory, argv[2]);
  if (!reading.Subject.has_value()) {
    std::fprintf(stderr, "prune %s: nothing pruned -- %s\n", argv[1], reading.Refusal.c_str());
    return 2;
  }

  const std::vector<Examination> examinations = outshine::Prune::ExamineCase(*reading.Subject);
  std::fprintf(stderr, "prune %s\n", argv[1]);
  const size_t removalsRefused = RemoveAndSay(examinations);
  const outshine::Prune::Ledger ledger = outshine::Prune::Count(examinations);
  NameTheRecovery(directory);

  std::printf("PRUNE %llu %llu %llu %llu\n", static_cast<unsigned long long>(ledger.Pruned),
              static_cast<unsigned long long>(ledger.PrunedBytes),
              static_cast<unsigned long long>(ledger.Stayed),
              static_cast<unsigned long long>(ledger.StayedBytes));
  return removalsRefused == 0 ? 0 : 2;
}
