#ifndef PRUNE_H
#define PRUNE_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Json.h"

namespace outshine::Prune {

enum class Verdict : uint8_t { Kept, Prunable, Stays };

enum class Proof : uint8_t { Untaken, StoreHoldsTheseBytes, ThisRunWroteIt };

struct CaseUnderPrune;
class Removable;
struct Examination;

Examination Examine(const CaseUnderPrune &subject, const std::filesystem::path &file);

class Removable {
public:
  [[nodiscard]] const std::filesystem::path &Path() const { return Path_; }

  [[nodiscard]] Proof How() const { return How_; }

  [[nodiscard]] std::uintmax_t Bytes() const { return Bytes_; }

  [[nodiscard]] const std::string &Evidence() const { return Evidence_; }

private:
  Removable(std::filesystem::path path, Proof how, std::uintmax_t bytes, std::string evidence)
      : Path_(std::move(path)), How_(how), Bytes_(bytes), Evidence_(std::move(evidence)) {}

  friend Examination Examine(const CaseUnderPrune &subject, const std::filesystem::path &file);

  std::filesystem::path Path_;
  Proof How_ = Proof::Untaken;
  std::uintmax_t Bytes_ = 0;
  std::string Evidence_;
};

struct Examination {
  std::filesystem::path Path;
  std::uintmax_t Bytes = 0;
  Verdict What = Verdict::Stays;

  std::string Why;
  std::optional<Removable> Ticket;
};

struct CaseUnderPrune {
  std::filesystem::path Directory;
  std::filesystem::path StoreDirectory;
  std::map<std::string, std::string> KeyByLeaf;
  std::set<std::string> SubjectLeaves;

  std::filesystem::file_time_type RunStarted{};
};

inline bool IsInTheKeepSet(const CaseUnderPrune &subject, const std::filesystem::path &file) {
  const std::string leaf = file.filename().string();
  return file.extension() == ".png" || leaf == "manifest.json" || leaf == "provenance.json" ||
         subject.SubjectLeaves.count(leaf) == 1;
}

inline bool ReadWholeFile(const std::filesystem::path &path, std::string &into) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) { return false; }
  into.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
  return !stream.bad();
}

enum class Sameness : uint8_t { Same, DifferentSize, DifferentBytes, Unreadable };

inline Sameness SameContent(const std::filesystem::path &left,
                            const std::filesystem::path &right,
                            std::uintmax_t &firstDifference) {
  std::error_code failed;
  const std::uintmax_t leftBytes = std::filesystem::file_size(left, failed);
  if (failed) { return Sameness::Unreadable; }
  const std::uintmax_t rightBytes = std::filesystem::file_size(right, failed);
  if (failed) { return Sameness::Unreadable; }
  if (leftBytes != rightBytes) { return Sameness::DifferentSize; }

  std::ifstream one(left, std::ios::binary), two(right, std::ios::binary);
  if (!one || !two) { return Sameness::Unreadable; }
  constexpr std::streamsize kBlock = 1 << 20;
  std::string a(static_cast<size_t>(kBlock), '\0'), b(static_cast<size_t>(kBlock), '\0');
  std::uintmax_t at = 0;
  while (one.read(&a[0], kBlock) || one.gcount() > 0) {
    const std::streamsize got = one.gcount();
    two.read(&b[0], kBlock);
    if (two.gcount() != got) { return Sameness::Unreadable; }
    for (std::streamsize i = 0; i < got; ++i) {
      if (a[static_cast<size_t>(i)] != b[static_cast<size_t>(i)]) {
        firstDifference = at + static_cast<std::uintmax_t>(i);
        return Sameness::DifferentBytes;
      }
    }
    at += static_cast<std::uintmax_t>(got);
  }
  return Sameness::Same;
}

inline std::string Decimal(std::uintmax_t value) {
  char digits[32];
  std::snprintf(digits, sizeof digits, "%llu", static_cast<unsigned long long>(value));
  return digits;
}

inline std::string StoreVouches(const CaseUnderPrune &subject,
                                const std::string &key,
                                const std::filesystem::path &file,
                                bool &held) {
  held = false;
  const std::filesystem::path object = subject.StoreDirectory / key;
  if (!std::filesystem::is_regular_file(object)) {
    return "the store holds no object under key " + key;
  }
  std::uintmax_t at = 0;
  switch (SameContent(object, file, at)) {
    case Sameness::Same: held = true; return "the store holds these bytes under key " + key;
    case Sameness::DifferentSize: return "the object under key " + key + " is a different size";
    case Sameness::DifferentBytes:
      return "the object under key " + key + " differs at byte " + Decimal(at);
    case Sameness::Unreadable: break;
  }
  return "the object under key " + key + " could not be read against the file";
}

inline Examination Examine(const CaseUnderPrune &subject, const std::filesystem::path &file) {
  Examination examination;
  examination.Path = file;
  std::error_code failed;
  examination.Bytes = std::filesystem::file_size(file, failed);
  if (failed) { examination.Bytes = 0; }

  if (IsInTheKeepSet(subject, file)) {
    examination.What = Verdict::Kept;
    examination.Why = "the keep set: the pictures, the declaration, the provenance, the subject";
    return examination;
  }

  const std::map<std::string, std::string>::const_iterator named =
      subject.KeyByLeaf.find(file.filename().string());
  if (named != subject.KeyByLeaf.end()) {
    bool held = false;
    const std::string evidence = StoreVouches(subject, named->second, file, held);
    if (held) {
      examination.What = Verdict::Prunable;
      examination.Ticket =
          Removable(file, Proof::StoreHoldsTheseBytes, examination.Bytes, evidence);
    } else {
      examination.Why = evidence;
    }
    return examination;
  }

  const std::filesystem::file_time_type written = std::filesystem::last_write_time(file, failed);
  if (!failed && written > subject.RunStarted) {
    examination.What = Verdict::Prunable;
    examination.Ticket = Removable(file,
                                   Proof::ThisRunWroteIt,
                                   examination.Bytes,
                                   "this run's own arms wrote it, so re-running the case is its "
                                   "producer");
    return examination;
  }

  examination.Why = "no product key in provenance, no subject declares it, and this run did not "
                    "write it -- so nothing here can say what would produce it again";
  return examination;
}

inline bool Remove(const Removable &removable) {
  std::error_code failed;
  return std::filesystem::remove(removable.Path(), failed) && !failed;
}

inline void CollectKeys(const Json::Ref &report, std::map<std::string, std::string> &into) {
  const Json::Ref renders = report["render"];
  for (size_t row = 0; row < renders.Size(); ++row) {
    const Json::Ref keys = renders[row]["keys"];
    const Json::Ref products = renders[row]["products"];
    for (size_t product = 0; product < keys.Size(); ++product) {
      const std::string name = keys.Key(product);
      const std::string path = products[name.c_str()]["path"].Str();
      if (path.empty()) { continue; }
      into[std::filesystem::path(path).filename().string()] = keys[product].Str();
    }
  }
}

inline void CollectSubjectLeaves(const Json::Ref &report, std::set<std::string> &into) {
  const Json::Ref fetched = report["fetch"]["files"];
  for (size_t row = 0; row < fetched.Size(); ++row) { into.insert(fetched[row]["as"].Str()); }
  const Json::Ref generated = report["generate"];
  for (size_t row = 0; row < generated.Size(); ++row) { into.insert(generated[row]["as"].Str()); }
  const Json::Ref converted = report["convert"];
  for (size_t row = 0; row < converted.Size(); ++row) {
    into.insert(converted[row]["outputName"].Str());
  }
  into.erase("");
}

struct CaseReading {
  std::optional<CaseUnderPrune> Subject;

  std::string Refusal;
};

inline CaseReading ReadCase(const std::filesystem::path &directory,
                            const std::filesystem::path &marker) {
  CaseReading reading;
  std::string document;
  if (!ReadWholeFile(directory / "provenance.json", document)) {
    reading.Refusal = "no provenance.json, so no key names any product of this case";
    return reading;
  }
  Json provenance;
  if (!provenance.Parse(document.c_str(), document.size())) {
    reading.Refusal = "provenance.json stopped parsing at byte " + Decimal(provenance.StoppedAt());
    return reading;
  }
  std::error_code failed;
  const std::filesystem::file_time_type started = std::filesystem::last_write_time(marker, failed);
  if (failed) {
    reading.Refusal =
        "no marker at " + marker.string() + ", so nothing says which files this run wrote";
    return reading;
  }
  CaseUnderPrune subject;
  subject.Directory = directory;
  subject.StoreDirectory = provenance.Root()["contentStore"].Str();
  subject.RunStarted = started;
  if (subject.StoreDirectory.empty() || !std::filesystem::is_directory(subject.StoreDirectory)) {
    reading.Refusal = "provenance.json names no content store that exists";
    return reading;
  }
  CollectKeys(provenance.Root()["report"], subject.KeyByLeaf);
  CollectSubjectLeaves(provenance.Root()["report"], subject.SubjectLeaves);
  reading.Subject = subject;
  return reading;
}

inline std::vector<Examination> ExamineCase(const CaseUnderPrune &subject) {
  std::vector<std::filesystem::path> files;
  std::error_code failed;
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::directory_iterator(subject.Directory, failed)) {
    if (entry.is_regular_file()) { files.push_back(entry.path()); }
  }
  std::sort(files.begin(), files.end());
  std::vector<Examination> examinations;
  examinations.reserve(files.size());
  for (const std::filesystem::path &file : files) {
    examinations.push_back(Examine(subject, file));
  }
  return examinations;
}

struct Ledger {
  size_t Pruned = 0, Kept = 0, Stayed = 0;
  std::uintmax_t PrunedBytes = 0, KeptBytes = 0, StayedBytes = 0;
};

inline Ledger Count(const std::vector<Examination> &examinations) {
  Ledger ledger;
  for (const Examination &examination : examinations) {
    switch (examination.What) {
      case Verdict::Prunable:
        ++ledger.Pruned;
        ledger.PrunedBytes += examination.Bytes;
        break;
      case Verdict::Kept:
        ++ledger.Kept;
        ledger.KeptBytes += examination.Bytes;
        break;
      case Verdict::Stays:
        ++ledger.Stayed;
        ledger.StayedBytes += examination.Bytes;
        break;
    }
  }
  return ledger;
}

} // namespace outshine::Prune

#endif
