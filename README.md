# binary_p2p

A small C++23 library for parsing Bitcoin P2P binary messages.

## Building and running the tests

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

### Running a test binary directly

CTest treats each Boost.Test executable as one entry. To see individual cases and richer output, run the binary yourself:

```bash
# Debug build
./build/Debug/test/byte_reader_test --report_level=short
./build/Debug/test/message_header_parser_test --log_level=all

# Release build
./build/Release/test/byte_reader_test --report_level=short
```

## Using the library from another Conan package

```bash
conan create .
```
