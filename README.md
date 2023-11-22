[![Actions Status](https://github.com/nikezono/retlock/workflows/MacOS/badge.svg)](https://github.com/nikezono/retlock/actions)
[![Actions Status](https://github.com/nikezono/retlock/workflows/Ubuntu/badge.svg)](https://github.com/nikezono/retlock/actions)
[![Actions Status](https://github.com/nikezono/retlock/workflows/Style/badge.svg)](https://github.com/nikezono/retlock/actions)
[![codecov](https://codecov.io/gh/nikezono/retlock/graph/badge.svg?token=9PBB727WMZ)](https://codecov.io/gh/nikezono/retlock)

# ReTLock

Fast and Efficient C++ Implementation of Reentrant Locking

## Usage

You can use `include/retlock/retlock.hpp` as the single-file header-only library for your project.
To build the benchmark & test cases, use the followings:

```bash
# build
cmake -S all -B build
cmake --build build

# run benchmark
./build/benchmark/ReTLockBench --help
# run tests
./build/test/ReTLockTests

# format code
cmake --build build --target format
cmake --build build --target fix-format
```

### Additional tools

The test and benchmark subprojects include the [tools.cmake](cmake/tools.cmake) file which is used to import additional tools on-demand through CMake configuration arguments.
The following are currently supported.

#### Sanitizers

Sanitizers can be enabled by configuring CMake with `-DUSE_SANITIZER=<Address | Memory | MemoryWithOrigins | Undefined | Thread | Leak | 'Address;Undefined'>`.

#### Static Analyzers

Static Analyzers can be enabled by setting `-DUSE_STATIC_ANALYZER=<clang-tidy | iwyu | cppcheck>`, or a combination of those in quotation marks, separated by semicolons.
By default, analyzers will automatically find configuration files such as `.clang-format`.
Additional arguments can be passed to the analyzers by setting the `CLANG_TIDY_ARGS`, `IWYU_ARGS` or `CPPCHECK_ARGS` variables.

#### Ccache

Ccache can be enabled by configuring with `-DUSE_CCACHE=<ON | OFF>`.
