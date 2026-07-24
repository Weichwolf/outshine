# 3rdparty

Vendored single-header libraries. Each entry pins an exact upstream commit
so the build is reproducible without a network fetch.

## stb_image.h

- Upstream: https://github.com/nothings/stb/blob/master/stb_image.h
- Version:  v2.30
- Commit:   013ac3beddff3dbffafd5177e7972067cd2b5083
- License:  dual-licensed MIT / public domain (unlicense). We use the
            public-domain option.

Only the PNG decoder path is compiled in. `libosmmesh/src/terrain.c` is the
single translation unit that sets `STB_IMAGE_IMPLEMENTATION` and also sets
`STBI_NO_JPEG`, `STBI_NO_BMP`, `STBI_NO_PSD`, `STBI_NO_TGA`, `STBI_NO_GIF`,
`STBI_NO_HDR`, `STBI_NO_PIC`, `STBI_NO_PNM` so the resulting object file
contains only the PNG loader and its zlib dependency.

No other source file may include `stb_image.h` with the implementation
define. Header-only consumers are fine but currently none exist.
