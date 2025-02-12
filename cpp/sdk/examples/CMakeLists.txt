add_executable(simple_client
    simple_client.cpp
)

target_link_libraries(simple_client
    PRIVATE
        mcp
        Flatbuffers::flatbuffers
)

# Copy schema file to build directory for testing
configure_file(
    ${CMAKE_SOURCE_DIR}/../../all.fbs
    ${CMAKE_CURRENT_BINARY_DIR}/all.fbs
    COPYONLY
) 