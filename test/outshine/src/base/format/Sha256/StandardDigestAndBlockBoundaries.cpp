#include "Sha256.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(Sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA-256 standard abc vector");
  CHECK(Sha256Hex(nullptr, 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "empty input needs no accessible byte");

  struct Case {
    size_t Bytes;
    std::string_view Digest;
  };

  // Pinned independently with Python hashlib.sha256 over byte(i % 251).
  constexpr std::array cases{
      Case{0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
      Case{1, "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d"},
      Case{55, "463eb28e72f82e0a96c0a4cc53690c571281131f672aa229e0d45ae59b598b59"},
      Case{56, "da2ae4d6b36748f2a318f23e7ab1dfdf45acdc9d049bd80e59de82a60895f562"},
      Case{63, "29af2686fd53374a36b0846694cc342177e428d1647515f078784d69cdb9e488"},
      Case{64, "fdeab9acf3710362bd2658cdc9a29e8f9c757fcf9811603a8c447cd1d9151108"},
      Case{65, "4bfd2c8b6f1eec7a2afeb48b934ee4b2694182027e6d0fc075074f2fabb31781"},
      Case{127, "92ca0fa6651ee2f97b884b7246a562fa71250fedefe5ebf270d31c546bfea976"},
      Case{128, "471fb943aa23c511f6f72f8d1652d9c880cfa392ad80503120547703e56a2be5"},
      Case{129, "5099c6a56203f9687f7d33f4bfdf576d31dc91f6b695ecea38b2770c87631135"},
      Case{4096, "d67c656e01756650d77717b0839985a056ec28ffe174601d690fc407a2ceffca"},
      Case{4194304, "a117210941a0b00dcb2d8577e680d84b6fa0eaf760d2afc654c953b9859d54fa"},
  };
  for (const auto &test : cases) {
    std::string bytes(test.Bytes, '\0');
    for (size_t at = 0; at < bytes.size(); ++at) { bytes[at] = static_cast<char>(at % 251); }
    CHECK(Sha256Hex(bytes) == test.Digest,
          "digest matches independent padding and block-boundary vectors");
  }
  return Report();
}
