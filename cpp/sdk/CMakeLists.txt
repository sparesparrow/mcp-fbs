cmake_minimum_required(VERSION 3.14)
project(mcp-cpp-sdk VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find FlatBuffers package
find_package(Flatbuffers REQUIRED)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Generate FlatBuffers code
set(FLATBUFFERS_FLATC_EXECUTABLE ${FLATBUFFERS_ROOT}/bin/flatc)
set(FLATBUFFERS_INCLUDE_DIR ${FLATBUFFERS_ROOT}/include)

# Copy and generate from the MCP schema
configure_file(${CMAKE_SOURCE_DIR}/../../all.fbs ${CMAKE_BINARY_DIR}/schemas/all.fbs COPYONLY)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/generated/mcp_generated.h
    COMMAND ${FLATBUFFERS_FLATC_EXECUTABLE} --cpp --gen-mutable --gen-object-api
            -o ${CMAKE_BINARY_DIR}/generated
            ${CMAKE_BINARY_DIR}/schemas/all.fbs
    DEPENDS ${CMAKE_BINARY_DIR}/schemas/all.fbs
    COMMENT "Generating C++ headers from FlatBuffers schema"
)

add_custom_target(generate_flatbuffers DEPENDS ${CMAKE_BINARY_DIR}/generated/mcp_generated.h)

# Create MCP library
add_library(mcp
    src/mcp_client.cpp
    src/mcp_server.cpp
    src/mcp_transport.cpp
    ${CMAKE_BINARY_DIR}/generated/mcp_generated.h
)

add_dependencies(mcp generate_flatbuffers)

target_include_directories(mcp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${FLATBUFFERS_INCLUDE_DIR}
        src
)

# Installation rules
install(TARGETS mcp
    EXPORT mcp-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(
    DIRECTORY include/mcp
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(
    FILES ${CMAKE_BINARY_DIR}/generated/mcp_generated.h
    DESTINATION include/mcp
)

# Export targets
install(EXPORT mcp-targets
    FILE mcp-targets.cmake
    NAMESPACE MCP::
    DESTINATION lib/cmake/mcp
)

# Add subdirectories
add_subdirectory(examples)
add_subdirectory(tests)

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "mcp-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mcp-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mcp-config.cmake"
    INSTALL_DESTINATION lib/cmake/mcp
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mcp-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mcp-config-version.cmake"
    DESTINATION lib/cmake/mcp
) 