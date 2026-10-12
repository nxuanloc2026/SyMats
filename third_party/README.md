# Giac build

Symats pins [Giac](https://github.com/nxuanloc2026/giac) as a Git submodule. The
pin is based on upstream release `1.9.0.57+dfsg2` and includes a small C++20
and MSVC compatibility patch on the fork's `symats/msvc-cxx20-release` branch.
Giac is licensed GPL-3.0-or-later; the fork retains its source notices.

Initialize the submodule before enabling the optional build:

```sh
git submodule update --init third_party/giac
```

On Ubuntu, install `libgmp-dev`, `libmpfr-dev`, `flex`, and `bison`, then run:

```sh
cmake -S . -B build-giac -DSYMATS_USE_GIAC=ON
cmake --build build-giac --parallel 2
ctest --test-dir build-giac --output-on-failure
```

On Windows, install GMP and MPFR through vcpkg and use WinFlexBison 2.5.25.
Open a Visual Studio developer shell, then configure with the vcpkg toolchain:

```powershell
cmake -S . -B build-giac -G 'Visual Studio 17 2022' -A x64 `
  -DSYMATS_USE_GIAC=ON `
  "-DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  "-DGIAC_FLEX=C:/path/to/winflexbison/win_flex.exe" `
  "-DGIAC_BISON=C:/path/to/winflexbison/win_bison.exe"
cmake --build build-giac --config Release --parallel 2
ctest --test-dir build-giac -C Release --output-on-failure
```

The CMake target is `third_party::giac`. Its public headers and GMP/MPFR link
dependencies are propagated to consumers. The `giac-smoke` test links the
static library and checks a basic exact expression; Symats' existing tests run
alongside it. The Giac build is opt-in until the Expr bridge in T-012 uses it.

## SUNDIALS

The optional CVODE backend uses SUNDIALS 6.x from the system or package manager.
Configure with `-DSYMATS_USE_SUNDIALS=ON`; CMake locates the CVODE, serial
N_Vector, dense matrix, dense linear solver, and core libraries. On Debian/Ubuntu
install `libsundials-dev`; on Windows install the `sundials` vcpkg port.
The current adapter supports scalar first-order `NDSolve` requests and returns
an `InterpolatingFunction` sample table. Samples are represented as decimal
rationals until the core gains a floating-point `Expr` kind.
