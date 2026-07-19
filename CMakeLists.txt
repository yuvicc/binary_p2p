cmake_minimum_required(VERSION 3.22)

project(
    binary_p2p
    VERSION 0.1.0
    LANGUAGES CXX
)

add_library(binary_p2p
    src/binary_p2p.cpp
    src/message_header.cpp
)

target_compile_features(binary_p2p
    PUBLIC cxx_std_23
)

set_target_properties(binary_p2p PROPERTIES
    CXX_EXTENSIONS OFF
    PUBLIC_HEADER "include/binary_p2p.h;include/message_header.h"
)

target_include_directories(binary_p2p
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

include(GNUInstallDirs)

install(
    TARGETS binary_p2p
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
