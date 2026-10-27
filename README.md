# binary_p2p

A small C++23 library for parsing Bitcoin P2P binary messages.

## Requirements

- CMake 3.22+
- A C++23 compiler
- Boost.Test (only needed for the tests)

```bash
# macOS
brew install cmake boost

# Debian/Ubuntu
sudo apt-get install cmake libboost-test-dev
```

## Building and running the tests

### Debug

```bash
cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug
ctest --test-dir build/Debug --output-on-failure
```

### Release

```bash
cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release
ctest --test-dir build/Release --output-on-failure
```

Pass `-DBUILD_TESTING=OFF` to skip the tests (and the Boost dependency), or `-DBINARY_P2P_BUILD_EXAMPLES=ON` to build the example programs.

### Running a test binary directly

CTest treats each Boost.Test executable as one entry. To see individual cases and richer output, run the binary yourself:

```bash
# Debug build
./build/Debug/test/byte_reader_test --report_level=short
./build/Debug/test/message_header_parser_test --log_level=all

# Release build
./build/Release/test/byte_reader_test --report_level=short
```

## Installing

```bash
cmake --install build/Release --prefix /path/to/prefix
```
