/* THE CONTENT STORE NAMES A FILE BY THIS DIGEST, so a wrong implementation would serve one source's
 * bytes under another source's request and no other test in this tree could see it. The vectors
 * below are FIPS 180-4's own plus the three lengths where the padding decides — 55 fits the length
 * field, 56 does not and forces a second block, 64 is an exact block — and they are CORRECTNESS
 * against something outside this tree rather than agreement with it. */
#include "Check.h"
#include "Sha256.h"

#include <string>

using namespace outshine;

int main() {
  Test::Covers("I.22 the content store keys a file on a digest a collision must be impossible in");

  CHECK(Sha256Hex(std::string("")) ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "the empty message is FIPS 180-4's published digest");
  CHECK(Sha256Hex(std::string("abc")) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "'abc' is FIPS 180-4's one-block example");
  CHECK(Sha256Hex(std::string("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        "the 56-byte message is FIPS 180-4's two-block example");
  CHECK(Sha256Hex(std::string(55, 'x')) ==
            "d5e285683cd4efc02d021a5c62014694958901005d6f71e89e0989fac77e4072",
        "55 bytes still leave room for the length field in one block");
  CHECK(Sha256Hex(std::string(64, 'x')) ==
            "7ce100971f64e7001e8fe5a51973ecdfe1ced42befe7ee8d5fd6219506b5393c",
        "an exact block forces a whole second block of padding");
  CHECK(Sha256Hex(std::string(1000000, 'a')) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
        "one million 'a' is the standard's long-message vector");

  CHECK(Sha256Hex(std::string("a")) != Sha256Hex(std::string("b")),
        "two one-byte messages do not share a name");
  Test::Note("digest bits", 256.0, "bit");

  return Test::Report();
}
