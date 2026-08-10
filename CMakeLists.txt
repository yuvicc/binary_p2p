cmake_minimum_required(VERSION 3.22)

project(
    binary_p2p
    VERSION 0.1.0
    LANGUAGES CXX
)

add_library(binary_p2p
    src/binary_p2p.cpp
    src/message_header.cpp
    src/message_header_parser.cpp
    src/block_parser.cpp
    src/block_serializer.cpp
    src/getheaders_parser.cpp
    src/getheaders_serializer.cpp
    src/headers_parser.cpp
    src/headers_serializer.cpp
    src/inventory_parser.cpp
    src/inventory_serializer.cpp
    src/message_checksum.cpp
    src/message_serializer.cpp
    src/message_stream_decoder.cpp
    src/raw_message_parser.cpp
    src/compact_size.cpp
    src/version_parser.cpp
    src/version_serializer.cpp
    src/payload_parser.cpp
    src/util/reader.cpp
    src/util/writer.cpp
    src/util/sha256.cpp
)

target_compile_definitions(binary_p2p
    PRIVATE DISABLE_OPTIMIZED_SHA256
)

target_compile_features(binary_p2p
    PUBLIC cxx_std_23
)

set_target_properties(binary_p2p PROPERTIES
    CXX_EXTENSIONS OFF
)

target_include_directories(binary_p2p
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

include(GNUInstallDirs)

install(
    TARGETS binary_p2p
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

option(BINARY_P2P_BUILD_EXAMPLES "Build the example programs" OFF)

# The examples are developer-only and are not exported into the Conan package
if (BINARY_P2P_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples)
endif()

include(CTest)


if (BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
    add_subdirectory(test)
endif()

message(STATUS "")
message(STATUS "binary_p2p ${PROJECT_VERSION} configuration:")
message(STATUS "  Build type      : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build examples  : ${BINARY_P2P_BUILD_EXAMPLES}")
message(STATUS "  Build tests     : ${BUILD_TESTING}")
message(STATUS "")

