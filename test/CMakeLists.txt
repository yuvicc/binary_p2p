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
add_boost_test(byte_writer_test)
add_boost_test(compact_size_test)
add_boost_test(message_header_parser_test)
add_boost_test(raw_message_parser_test)
add_boost_test(inventory_parser_test)
add_boost_test(inventory_serializer_test)
add_boost_test(headers_parser_test)
add_boost_test(headers_serializer_test)
add_boost_test(getheaders_test)
add_boost_test(block_test)
add_boost_test(block_hash_test)
add_boost_test(block_locator_test)
add_boost_test(message_stream_decoder_test)
add_boost_test(message_serializer_test)
add_boost_test(version_parser_test)
add_boost_test(version_serializer_test)
add_boost_test(payload_parser_test)

# These tests open sockets and spawn background threads.
find_package(Threads REQUIRED)

add_boost_test(net_test)
target_link_libraries(net_test PRIVATE Threads::Threads)

add_boost_test(handshake_test)
target_link_libraries(handshake_test PRIVATE Threads::Threads)

add_boost_test(header_sync_test)
target_link_libraries(header_sync_test PRIVATE Threads::Threads)

add_boost_test(block_download_test)
target_link_libraries(block_download_test PRIVATE Threads::Threads)
