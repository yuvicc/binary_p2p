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
    src/message_checksum.cpp
    src/raw_message_parser.cpp
    src/compact_size.cpp
    src/version_parser.cpp
    src/payload_parser.cpp
    src/util/reader.cpp
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

include(CTest)

if (BUILD_TESTING)
    add_subdirectory(test)
endif()

