# binary_p2p

A small C++23 library for parsing Bitcoin-style P2P binary messages. It provides:

## Requirements

| Tool | Version used |
|------|--------------|
| A C++23 compiler | GCC 14 (GCC 13+ works) |
| [CMake](https://cmake.org/) | ≥ 3.23 (needed for `--preset`) |
| [Conan](https://conan.io/) | 2.x |
| [Boost.Test](https://www.boost.org/) | pulled in automatically by Conan (test-only) |

## Building and running the tests

The workflow is the standard Conan 2 + CMake preset flow: Conan resolves dependencies and generates a CMake preset, then CMake configures, builds, and CTest runs the suites. Debug and Release live in separate build trees (`build/Debug`, `build/Release`) and can coexist.

### Debug

```bash
conan install . --build=missing -s build_type=Debug
cmake --preset conan-debug
cmake --build --preset conan-debug
ctest --preset conan-debug --output-on-failure
```

### Release

```bash
conan install . --build=missing -s build_type=Release
cmake --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release --output-on-failure
```

Notes:
- `--build=missing` builds any dependency (e.g. Boost) from source if no prebuilt binary matches your compiler; the first run may take a while.
- You only need to re-run `conan install` when dependencies change. Day to day, just repeat the `cmake --build` and `ctest` steps.
- If a preset name isn't found, list the available ones with `cmake --list-presets`.

### Running a test binary directly

CTest treats each Boost.Test executable as one entry. To see individual cases and richer output, run the binary yourself:

```bash
# Debug build
./build/Debug/test/byte_reader_test --report_level=short
./build/Debug/test/message_header_parser_test --log_level=all

# Release build
./build/Release/test/byte_reader_test --report_level=short
```

Useful Boost.Test flags: `--list_content` (list all cases), `--run_test=<name>` (run one case), `--log_level=all` (verbose).

## Using the library from another Conan package

`conanfile.py` packages the library so it can be consumed elsewhere:

```bash
conan create .
```

This builds the package and runs the `test_package/` consumer test, which links against `binary_p2p` and executes a small example to confirm the package is usable.
