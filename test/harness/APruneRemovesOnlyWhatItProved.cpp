/* THE PRUNE'S PRECONDITION, EXERCISED ON THE DANGEROUS PATH (board:1181).
 *
 * The runner prunes every case as it finishes, pass or fail, so a mistake here is not a re-render --
 * it is a file nothing can produce again. The whole safety argument is therefore the proof, and a
 * test that only showed a provable file being removed would say nothing about the case this exists
 * for. So the subject here is mostly the refusals: a key the store has no object for, an object of
 * the wrong size, an object that differs in one byte, a file no producer claims -- each stays, and
 * each says which proof refused it.
 *
 * IT BUILDS ITS OWN CASE AND ITS OWN STORE under the system temp directory rather than pruning
 * anything real: a test whose subject is deletion must not be able to delete the corpus, and a
 * fabricated store is the only way to spell "the store does NOT hold this" on purpose.
 *
 * THE MARKER'S TIME IS SET RATHER THAN TAKEN. "This run wrote it" is an mtime comparison, and a test
 * that raced the clock's resolution would be a flake wearing a proof's hat. */
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "Prune.h"

namespace {

using outshine::Prune::Examination;
using outshine::Prune::Proof;
using outshine::Prune::Verdict;
namespace fs = std::filesystem;

const fs::file_time_type kBeforeTheRun = fs::file_time_type::clock::now() - std::chrono::hours(2);
const fs::file_time_type kDuringTheRun = fs::file_time_type::clock::now() - std::chrono::hours(1);

void Write(const fs::path &path, const std::string &content, fs::file_time_type when) {
  std::ofstream out(path, std::ios::binary);
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
  out.close();
  std::error_code failed;
  fs::last_write_time(path, when, failed);
}

const Examination *Find(const std::vector<Examination> &examinations, const char *leaf) {
  for (const Examination &examination : examinations) {
    if (examination.Path.filename() == leaf) { return &examination; }
  }
  return nullptr;
}

/* One claim per file, spelled as the sentence the file's own row must satisfy, so a failure names
 * the file and the verdict rather than an index into a vector. */
void Claim(const std::vector<Examination> &examinations, const char *leaf, Verdict wanted,
           const char *claim) {
  const Examination *row = Find(examinations, leaf);
  CHECK(row != nullptr, claim);
  if (!row) { return; }
  CHECK(row->What == wanted, claim);
  if (row->What != wanted) {
    std::printf("NOTE %s: %s\n", leaf,
                row->Ticket.has_value() ? row->Ticket->Evidence().c_str() : row->Why.c_str());
  }
}

std::string Repeated(char of, size_t times) { return std::string(times, of); }

struct Fabricated {
  fs::path Root, Case, Store, Marker;
};

/* A CASE CARRYING ONE FILE PER PROOF AND ONE PER REFUTATION. Everything the prune can meet is here,
 * including the three ways a named key fails, because those are the states that decide whether a
 * file survives. */
Fabricated Fabricate(const std::string &storeName) {
  Fabricated made;
  made.Root = fs::temp_directory_path() / ("outshine-prune-" + storeName);
  made.Case = made.Root / "case";
  made.Store = made.Root / "content";
  made.Marker = made.Root / "marker";
  std::error_code failed;
  fs::remove_all(made.Root, failed);
  fs::create_directories(made.Case);
  fs::create_directories(made.Store);

  const std::string oracle = Repeated('o', 4096);
  const std::string subject = Repeated('s', 2048);
  Write(made.Store / "0000000000000000000000000000000000000000000000000000000000000001", oracle,
        kBeforeTheRun);
  Write(made.Store / "0000000000000000000000000000000000000000000000000000000000000003",
        Repeated('o', 4095), kBeforeTheRun);
  Write(made.Store / "0000000000000000000000000000000000000000000000000000000000000004",
        Repeated('o', 2048) + "X" + Repeated('o', 2047), kBeforeTheRun);

  Write(made.Case / "oracle.raw", oracle, kBeforeTheRun);
  Write(made.Case / "oracle.normal.raw", oracle, kBeforeTheRun);
  Write(made.Case / "oracle.uv.raw", oracle, kBeforeTheRun);
  Write(made.Case / "oracle.materialIndex.raw", oracle, kBeforeTheRun);
  Write(made.Case / "oracle.objectIndex.raw", oracle, kDuringTheRun);
  Write(made.Case / "scene.glb", subject, kBeforeTheRun);
  Write(made.Case / "grown.gltf", Repeated('g', 512), kBeforeTheRun);
  Write(made.Case / "manifest.json", "{}", kBeforeTheRun);
  Write(made.Case / "0-reference.png", subject, kBeforeTheRun);

  Write(made.Marker, "", kDuringTheRun - std::chrono::minutes(1));
  Write(made.Case / "outshine.raw", Repeated('u', 1024), kDuringTheRun);
  return made;
}

/* The provenance the preparer would have written, with one key per named product: two that hold,
 * three that do not, and one product the store never received at all. */
std::string Provenance(const Fabricated &made) {
  const std::string directory = made.Case.string() + "/";
  return std::string("{\n\"contentStore\": \"") + made.Store.string() +
         "\",\n\"report\": {\n"
         " \"fetch\": {\"files\": [{\"as\": \"scene.glb\"}]},\n"
         " \"render\": [{\n  \"keys\": {\n"
         "   \"raw\": \"0000000000000000000000000000000000000000000000000000000000000001\",\n"
         "   \"normalRaw\": \"0000000000000000000000000000000000000000000000000000000000000002\",\n"
         "   \"uvRaw\": \"0000000000000000000000000000000000000000000000000000000000000003\",\n"
         "   \"materialIndexRaw\": \"0000000000000000000000000000000000000000000000000000000000000004\",\n"
         "   \"objectIndexRaw\": \"0000000000000000000000000000000000000000000000000000000000000005\"\n"
         "  },\n  \"products\": {\n"
         "   \"raw\": {\"path\": \"" + directory + "oracle.raw\"},\n"
         "   \"normalRaw\": {\"path\": \"" + directory + "oracle.normal.raw\"},\n"
         "   \"uvRaw\": {\"path\": \"" + directory + "oracle.uv.raw\"},\n"
         "   \"materialIndexRaw\": {\"path\": \"" + directory + "oracle.materialIndex.raw\"},\n"
         "   \"objectIndexRaw\": {\"path\": \"" + directory + "oracle.objectIndex.raw\"}\n"
         "  }\n }]\n}\n}\n";
}

bool Exists(const Fabricated &made, const char *leaf) { return fs::exists(made.Case / leaf); }

} // namespace

int main() {
  using namespace outshine::Test;

  const Fabricated made = Fabricate("proof");
  Write(made.Case / "provenance.json", Provenance(made), kBeforeTheRun);

  const outshine::Prune::CaseReading reading = outshine::Prune::ReadCase(made.Case, made.Marker);
  CHECK(reading.Subject.has_value(), "a case carrying a provenance document can be examined at all");
  if (!reading.Subject.has_value()) {
    std::printf("NOTE %s\n", reading.Refusal.c_str());
    return Report();
  }
  CHECK(reading.Subject->KeyByLeaf.size() == 5,
        "every product the preparer named is mapped from the leaf it was placed as");
  CHECK(reading.Subject->SubjectLeaves.size() == 1,
        "every leaf the preparer placed as an input is read out of the same document");

  const std::vector<Examination> examinations = outshine::Prune::ExamineCase(*reading.Subject);

  Claim(examinations, "oracle.raw", Verdict::Prunable,
        "a product whose bytes the store holds under the key provenance names is prunable");
  Claim(examinations, "scene.glb", Verdict::Kept,
        "a subject the preparer placed is kept, because it is this case's input and every other "
        "suite's");
  Claim(examinations, "outshine.raw", Verdict::Prunable,
        "our own output is prunable because this run's arms wrote it, and re-running the case is "
        "its producer");

  Claim(examinations, "oracle.normal.raw", Verdict::Stays,
        "a product whose key names no object in the store stays, and the store is not asked twice");
  Claim(examinations, "oracle.uv.raw", Verdict::Stays,
        "a product whose stored object is a different size stays");
  Claim(examinations, "oracle.materialIndex.raw", Verdict::Stays,
        "a product whose stored object differs in one byte stays");
  Claim(examinations, "grown.gltf", Verdict::Stays,
        "a file in neither class -- no key, not written by this run, not in the store under its own "
        "digest -- stays");
  /* THE ONE THE FALL-THROUGH WOULD HAVE COST. A named product that was rewritten during the run
   * looks exactly like our own output to an mtime check, and the store cannot vouch for it. */
  Claim(examinations, "oracle.objectIndex.raw", Verdict::Stays,
        "a named product the store cannot vouch for stays even when this run's clock would have "
        "called it ours");

  Claim(examinations, "0-reference.png", Verdict::Kept,
        "a picture is kept even when the store provably holds it");
  Claim(examinations, "manifest.json", Verdict::Kept, "the declaration is kept");
  Claim(examinations, "provenance.json", Verdict::Kept,
        "the provenance is kept, because the keys the next prune reads are in it");

  const outshine::Prune::Ledger ledger = outshine::Prune::Count(examinations);
  Note("files a fabricated case may lose", (double)ledger.Pruned, "files");
  Note("files it may not", (double)(ledger.Stayed + ledger.Kept), "files");

  size_t removed = 0;
  for (const Examination &examination : examinations) {
    if (examination.Ticket.has_value() && outshine::Prune::Remove(*examination.Ticket)) {
      ++removed;
      CHECK(examination.Ticket->How() == Proof::StoreHoldsTheseBytes ||
                examination.Ticket->How() == Proof::ThisRunWroteIt,
            "every removal carries the proof that permitted it");
    }
  }
  CHECK(removed == ledger.Pruned, "every file the examination proved is the file that was removed");

  CHECK(!Exists(made, "oracle.raw") && !Exists(made, "outshine.raw"),
        "what was proven is gone from the case directory");
  CHECK(Exists(made, "oracle.normal.raw") && Exists(made, "oracle.uv.raw") &&
            Exists(made, "oracle.materialIndex.raw") && Exists(made, "oracle.objectIndex.raw") &&
            Exists(made, "grown.gltf"),
        "every file whose proof failed is still on disk after the prune ran");
  CHECK(Exists(made, "0-reference.png") && Exists(made, "manifest.json") &&
            Exists(made, "provenance.json") && Exists(made, "scene.glb"),
        "the keep set is still on disk after the prune ran");
  CHECK(fs::exists(made.Store /
                   "0000000000000000000000000000000000000000000000000000000000000001"),
        "the store keeps the object the case stopped keeping a second copy of");

  /* A PRUNE THAT CANNOT PROVE ITS PRECONDITION DOES NOT PROCEED, and the case with no provenance is
   * the live shape of that: nothing names a key, so nothing may be decided from a mtime alone. */
  const Fabricated undocumented = Fabricate("undocumented");
  const outshine::Prune::CaseReading refused =
      outshine::Prune::ReadCase(undocumented.Case, undocumented.Marker);
  CHECK(!refused.Subject.has_value() && !refused.Refusal.empty(),
        "a case with no provenance.json is refused with a reason, and nothing is examined");
  CHECK(fs::exists(undocumented.Case / "outshine.raw"),
        "a refused prune removes nothing at all, including what another proof would have allowed");

  std::error_code failed;
  fs::remove_all(made.Root, failed);
  fs::remove_all(undocumented.Root, failed);

  Covers("board:1181 the runner prunes test by test, and a file whose producer cannot be proven "
         "stays with the reason its proof failed");
  return Report();
}
