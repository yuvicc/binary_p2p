cmake_minimum_required(VERSION 3.15)
project(PackageTest CXX)

find_package(binary_p2p CONFIG REQUIRED)

add_executable(example src/example.cpp)
target_link_libraries(example binary_p2p::binary_p2p)
