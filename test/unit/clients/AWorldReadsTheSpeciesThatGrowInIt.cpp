#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"

#include "Forest.h"
#include "Species.h"

using outshine::Clients::ReadSpecies;
using outshine::Generators::TreeSpecies;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // board:1541: the reader opened ONE file with fopen, so a world stood on one species while
  // src/assets/world/species/ carried thirty-one. Pointing it at the directory refused with
  // an EMPTY reason, because the directory open failed with nothing to say about it.
  const char *kDir = "src/assets/world/species";
  size_t onDisk = 0;
  for (const auto &entry : std::filesystem::directory_iterator(kDir)) {
    onDisk += entry.path().extension() == ".json" ? 1 : 0;
  }
  Note("species the tree carries", (double)onDisk, "files");
  CHECK(onDisk >= 30, "the tree carries a wood's worth of declarations to read");

  std::vector<TreeSpecies> grown;
  std::string error;
  const bool read = ReadSpecies(kDir, grown, error);
  if (!read) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(read, "**A WORLD READS THE SPECIES DIRECTORY**, so a forest can be mixed (board:1541)");
  Note("species the reader returned", (double)grown.size(), "species");
  CHECK(grown.size() == onDisk,
        "and it returns EVERY one of them -- 0 or 1..N, never the first that opened");

  // board:1781: Forest holds a FIXED table of stems and refuses what will not fit. The bound
  // is a declared capacity, not a derivation -- kMostSpecies [SET] = 64, which is twice the
  // catalogue this tree ships -- and what makes a declared capacity safe is that exceeding it
  // refuses loudly rather than truncating. What makes it USEFUL is that the tree notices
  // before an artist does, which is this check: the shipped catalogue must stay clear of it.
  Note("the species a wood may hold at once", (double)outshine::Generators::Forest::kMostSpecies,
       "species");
  Note("the bytes that table costs", (double)outshine::Generators::Forest::kSpeciesTableBytes,
       "bytes");
  Note("the headroom the shipped catalogue leaves",
       (double)outshine::Generators::Forest::kMostSpecies / (double)onDisk, "x");
  CHECK(onDisk < outshine::Generators::Forest::kMostSpecies,
        "**AND THE SHIPPED CATALOGUE FITS THE TABLE THAT HOLDS IT**: a wood refuses more "
        "species than it can hold, and the day the catalogue reaches that bound the gate says "
        "so here rather than an artist discovering it -- a declared capacity is only safe if "
        "something watches the distance to it (board:1781)");

  {
    std::vector<TreeSpecies> one;
    std::string why;
    CHECK(ReadSpecies("src/assets/world/species/beech.json", one, why) && one.size() == 1,
          "a single file still reads, and answers one");
  }
  {
    std::vector<TreeSpecies> none;
    std::string why;
    CHECK(!ReadSpecies("src/assets/world/species/nosuchtree.json", none, why) &&
              why.find("nosuchtree.json") != std::string::npos,
          "**A REFUSAL NAMES THE FILE IT COULD NOT READ** -- the empty reason that filed this "
          "item is a refusal that tells the author nothing (board:1541)");
    std::printf("NOTE the refusal reads: %s\n", why.c_str());
  }
  {
    std::vector<TreeSpecies> none;
    std::string why;
    CHECK(!ReadSpecies("src/assets/world", none, why) && why.find("src/assets/world") != std::string::npos,
          "and a directory holding no .json refuses naming itself, rather than answering an "
          "empty wood");
    std::printf("NOTE the empty-directory refusal reads: %s\n", why.c_str());
  }
  {
    std::vector<TreeSpecies> none;
    std::string why;
    CHECK(!ReadSpecies("", none, why) && !why.empty(), "and an empty path refuses with a reason");
  }

  // board:1541, sharpened by the reviewer: the error check sat INSIDE the loop body of a
  // directory_iterator whose constructor sets the code AND returns the end iterator -- so the
  // body never ran and the check was dead in exactly the case it was written for. An
  // unreadable directory then refused with "holds no .json", the wrong cause, which is the
  // same shape as the empty why= that filed this item.
  {
    const std::filesystem::path shut =
        std::filesystem::temp_directory_path() / "outshine-species-shut";
    std::error_code trouble;
    std::filesystem::remove_all(shut, trouble);
    std::filesystem::create_directories(shut, trouble);
    std::filesystem::permissions(shut, std::filesystem::perms::none, trouble);

    std::vector<TreeSpecies> none;
    std::string said;
    const bool opened = ReadSpecies(shut.string().c_str(), none, said);
    std::filesystem::permissions(shut, std::filesystem::perms::owner_all, trouble);
    std::filesystem::remove_all(shut, trouble);

    std::printf("NOTE the unreadable-directory refusal reads: %s\n", said.c_str());
    CHECK(!opened, "an unreadable directory refuses");
    CHECK(said.find("does not open") != std::string::npos,
          "**A REFUSAL NAMES THE CAUSE IT ACTUALLY MET**: a directory that cannot be opened "
          "says so, rather than reporting the emptiness that follows from it (board:1541)");
  }

  Covers("II.14 a world reads the species that grow in it -- 0 or 1..N from a directory or a "
         "file, and every refusal names the path it could not read (board:1541)");
  return Report();
}
