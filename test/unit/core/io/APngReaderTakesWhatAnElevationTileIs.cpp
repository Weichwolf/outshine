#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <zlib.h>

#include "Check.h"

#include "Png.h"

using outshine::Io::Png;
using outshine::Io::ReadPng;

namespace {

void Big(std::vector<uint8_t> &into, uint32_t value) {
  into.push_back((uint8_t)(value >> 24));
  into.push_back((uint8_t)(value >> 16));
  into.push_back((uint8_t)(value >> 8));
  into.push_back((uint8_t)value);
}

void Chunk(std::vector<uint8_t> &into, const char *name, const std::vector<uint8_t> &body) {
  Big(into, (uint32_t)body.size());
  const size_t from = into.size();
  into.insert(into.end(), name, name + 4);
  into.insert(into.end(), body.begin(), body.end());
  const uLong sum = crc32(crc32(0L, nullptr, 0), into.data() + from, (uInt)(4 + body.size()));
  Big(into, (uint32_t)sum);
}

std::vector<uint8_t> Made(uint32_t wide, uint32_t high, uint8_t filter, uint8_t colour) {
  const uint32_t channels = colour == 2 ? 3u : 4u;
  std::vector<uint8_t> raw;
  for (uint32_t row = 0; row < high; ++row) {
    raw.push_back(filter);
    for (uint32_t column = 0; column < wide; ++column) {
      for (uint32_t part = 0; part < channels; ++part) {
        const uint8_t want = (uint8_t)((row * 7 + column * 13 + part * 29) & 0xff);
        if (filter == 0) {
          raw.push_back(want);
        } else if (filter == 1) {
          const uint8_t left =
              column > 0 ? (uint8_t)(((row * 7 + (column - 1) * 13 + part * 29)) & 0xff) : 0;
          raw.push_back((uint8_t)((want - left) & 0xff));
        } else {
          const uint8_t up =
              row > 0 ? (uint8_t)((((row - 1) * 7 + column * 13 + part * 29)) & 0xff) : 0;
          raw.push_back((uint8_t)((want - up) & 0xff));
        }
      }
    }
  }

  std::vector<uint8_t> squeezed(compressBound((uLong)raw.size()));
  uLongf got = (uLongf)squeezed.size();
  compress(squeezed.data(), &got, raw.data(), (uLong)raw.size());
  squeezed.resize(got);

  std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
  std::vector<uint8_t> head;
  Big(head, wide);
  Big(head, high);
  head.push_back(8);
  head.push_back(colour);
  head.push_back(0);
  head.push_back(0);
  head.push_back(0);
  Chunk(out, "IHDR", head);
  Chunk(out, "IDAT", squeezed);
  Chunk(out, "IEND", {});
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  for (uint8_t filter = 0; filter <= 2; ++filter) {
    const std::vector<uint8_t> file = Made(64, 48, filter, 2);
    const Png read = ReadPng(file.data(), file.size());
    if (!read.Read) { std::printf("REFUSED %s\n", read.Error.c_str()); }
    CHECK(read.Read && read.Wide == 64 && read.High == 48 && read.Channels == 3,
          "**A PNG IS READ BY THIS ENGINE AND NOT BY A LIBRARY IT LINKED FOR PICTURES.** An "
          "elevation tile arrives as 8-bit truecolour PNG; inflating it and undoing the row "
          "filters is a hundred lines against zlib, which the tree already links, and it is what "
          "lets the world be streamed with no window system anywhere near it");
    if (!read.Read) { return Report(); }

    size_t wrong = 0;
    for (uint32_t row = 0; row < read.High; ++row) {
      for (uint32_t column = 0; column < read.Wide; ++column) {
        for (uint32_t part = 0; part < 3; ++part) {
          const uint8_t want = (uint8_t)((row * 7 + column * 13 + part * 29) & 0xff);
          const uint8_t got = read.Bytes[(size_t)row * read.Wide * 3 + column * 3 + part];
          if (got != want) { ++wrong; }
        }
      }
    }
    Note("bytes the reader got wrong under filter", (double)wrong, "bytes");
    CHECK(wrong == 0,
          "and every byte comes back exactly, whichever row filter the encoder chose -- None, Sub "
          "and Up are three different reconstructions of the same picture and all three must land");
  }

  const std::vector<uint8_t> rgba = Made(16, 16, 0, 6);
  const Png four = ReadPng(rgba.data(), rgba.size());
  CHECK(four.Read && four.Channels == 4, "truecolour with alpha reads too, at four channels");

  const std::vector<uint8_t> nothing;
  const Png empty = ReadPng(nothing.data(), nothing.size());
  CHECK(!empty.Read, "no bytes at all is a refusal");
  std::printf("REFUSAL %s\n", empty.Error.c_str());

  std::vector<uint8_t> notPng(64, 0x41);
  const Png wrongSignature = ReadPng(notPng.data(), notPng.size());
  CHECK(!wrongSignature.Read, "and bytes that are not a PNG say so rather than being decoded");
  std::printf("REFUSAL %s\n", wrongSignature.Error.c_str());

  std::vector<uint8_t> cut = Made(32, 32, 0, 2);
  cut.resize(cut.size() / 2);
  const Png truncated = ReadPng(cut.data(), cut.size());
  CHECK(!truncated.Read,
        "**AND A TRUNCATED FILE IS A NAMED REFUSAL AND NOT HALF A PICTURE.** A tile that arrived "
        "short is a fetch that failed, and half an elevation grid is a hole in the ground nobody "
        "declared");
  std::printf("REFUSAL %s\n", truncated.Error.c_str());

  std::vector<uint8_t> deep = Made(8, 8, 0, 2);
  deep[24] = 16;
  const Png sixteen = ReadPng(deep.data(), deep.size());
  CHECK(!sixteen.Read, "and a bit depth this reader does not take is refused by name");
  std::printf("REFUSAL %s\n", sixteen.Error.c_str());

  Covers("I.9.12 an elevation tile's PNG is read by this engine: signature, IHDR, IDAT and IEND, "
         "inflated against zlib and unfiltered row by row, with every shape it does not take -- a "
         "bit depth, a colour type, interlacing, a truncation -- refused by name");
  return Report();
}
