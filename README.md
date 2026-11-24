# Neon Vortex

A 2D arcade space shooter built with the [Gorgon](https://github.com/Efendiminati/Gorgon) C++ framework, featuring procedural CGI sprites, Gaussian glow halos, procedural WAV audio synthesis, JSON save/high-score storage, and a built-in feature self-test.

## Building

Requires an installed Gorgon SDK (`find_package(Gorgon REQUIRED)`) and a C++17 compiler (MSVC / Visual Studio 2022 on Windows, GCC 11+ / Clang 14+ on Linux):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The executable is written to `Bin/NeonVortex.exe` (or `Bin/NeonVortex` on Linux).
