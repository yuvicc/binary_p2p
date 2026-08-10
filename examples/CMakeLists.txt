add_executable(demo demo.cpp)
target_link_libraries(demo PRIVATE binary_p2p)

add_executable(connect connect.cpp)
target_link_libraries(connect PRIVATE binary_p2p)
