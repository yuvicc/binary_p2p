find_package(Boost REQUIRED COMPONENTS unit_test_framework)

add_executable(byte_reader_test byte_reader_test.cpp)

target_link_libraries(byte_reader_test
    PRIVATE
        binary_p2p
        Boost::unit_test_framework
)

add_test(NAME byte_reader_test COMMAND byte_reader_test)

