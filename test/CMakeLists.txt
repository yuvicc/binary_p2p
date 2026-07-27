find_package(Boost REQUIRED COMPONENTS unit_test_framework)

# Register a Boost.Test executable and its CTest entry from a single source file.
function(add_boost_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name}
        PRIVATE
            binary_p2p
            Boost::unit_test_framework
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_boost_test(byte_reader_test)
add_boost_test(message_header_parser_test)
add_boost_test(raw_message_parser_test)
add_boost_test(version_parser_test)
add_boost_test(payload_parser_test)
