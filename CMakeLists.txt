# Agentite Engine - CMake Build System
# Supports macOS, Linux, and Windows
#
# Usage:
#   mkdir build && cd build
#   cmake ..
#   cmake --build .
#
# Options:
#   -DAGENTITE_BUILD_EXAMPLES=ON   Build example programs (default: ON)
#   -DAGENTITE_BUILD_TESTS=ON      Build test suite (default: ON)
#   -DAGENTITE_BUILD_GAME=ON       Build main game template (default: ON)
#
# As a subdirectory (for consuming projects):
#   add_subdirectory(agentite)
#   target_link_libraries(my_game PRIVATE agentite)

cmake_minimum_required(VERSION 3.16)
project(agentite
    VERSION 0.1.0
    DESCRIPTION "C-based game engine for strategy games and AI agent development"
    LANGUAGES C CXX
)

# Options
option(AGENTITE_BUILD_EXAMPLES "Build example programs" ON)
option(AGENTITE_BUILD_TESTS "Build test suite" ON)
option(AGENTITE_BUILD_GAME "Build main game template" ON)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C/C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

#============================================================================
# Dependencies
#============================================================================

# Find SDL3 via pkg-config
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL3 IMPORTED_TARGET sdl3)
endif()

# Fallback: try find_package
if(NOT SDL3_FOUND)
    find_package(SDL3 QUIET)
endif()

# Final fallback: manual paths
if(NOT SDL3_FOUND AND NOT TARGET PkgConfig::SDL3)
    message(WARNING "SDL3 not found via pkg-config or find_package, using fallback paths")
    set(SDL3_INCLUDE_DIRS /usr/local/include /opt/homebrew/include)
    set(SDL3_LIBRARY_DIRS /usr/local/lib /opt/homebrew/lib)
    set(SDL3_LIBRARIES SDL3)
endif()

#============================================================================
# Third-party libraries (compiled as static libraries)
#============================================================================

# Flecs ECS library
add_library(flecs STATIC lib/flecs.c)
target_include_directories(flecs
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>
)
set_target_properties(flecs PROPERTIES
    C_STANDARD 11
    POSITION_INDEPENDENT_CODE ON
)
# Suppress warnings for third-party code
if(NOT MSVC)
    target_compile_options(flecs PRIVATE -w)
endif()

# TOML parser library
add_library(toml STATIC lib/toml.c)
target_include_directories(toml
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>
        $<INSTALL_INTERFACE:include>
)
set_target_properties(toml PROPERTIES
    C_STANDARD 11
    POSITION_INDEPENDENT_CODE ON
)
if(NOT MSVC)
    target_compile_options(toml PRIVATE -w)
endif()

#============================================================================
# Agentite Engine Library
#============================================================================

# Collect engine source files
file(GLOB_RECURSE AGENTITE_ENGINE_SOURCES
    src/core/*.cpp
    src/platform/*.cpp
    src/graphics/*.cpp
    src/audio/*.cpp
    src/input/*.cpp
    src/ui/*.cpp
    src/ecs/*.cpp
    src/ai/*.cpp
    src/strategy/*.cpp
)

# Create the engine library
add_library(agentite STATIC ${AGENTITE_ENGINE_SOURCES})

target_include_directories(agentite
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/cglm/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# Link dependencies
target_link_libraries(agentite
    PUBLIC
        flecs
        toml
)

# SDL3 linkage
if(TARGET PkgConfig::SDL3)
    target_link_libraries(agentite PUBLIC PkgConfig::SDL3)
elseif(TARGET SDL3::SDL3)
    target_link_libraries(agentite PUBLIC SDL3::SDL3)
else()
    target_include_directories(agentite PUBLIC ${SDL3_INCLUDE_DIRS})
    target_link_directories(agentite PUBLIC ${SDL3_LIBRARY_DIRS})
    target_link_libraries(agentite PUBLIC ${SDL3_LIBRARIES})
endif()

# Platform-specific libraries
if(APPLE)
    target_link_libraries(agentite
        PUBLIC
            "-framework Cocoa"
            "-framework Metal"
            "-framework QuartzCore"
    )
elseif(UNIX)
    target_link_libraries(agentite
        PUBLIC
            m
            dl
            pthread
    )
elseif(WIN32)
    target_link_libraries(agentite
        PUBLIC
            mingw32
            SDL3main
    )
    if(NOT MSVC)
        target_link_options(agentite PUBLIC -mwindows)
    endif()
endif()

# Alias for use as subproject
add_library(agentite::agentite ALIAS agentite)

#============================================================================
# Main Game Template
#============================================================================

if(AGENTITE_BUILD_GAME)
    # Game template sources
    file(GLOB_RECURSE AGENTITE_GAME_SOURCES
        src/game/*.cpp
        src/game/systems/*.cpp
        src/game/states/*.cpp
        src/game/data/*.cpp
    )

    add_executable(agentite_game
        src/main.cpp
        ${AGENTITE_GAME_SOURCES}
    )

    target_include_directories(agentite_game PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/cglm/include
    )

    target_link_libraries(agentite_game PRIVATE agentite)

    # Set output name to match Makefile
    set_target_properties(agentite_game PROPERTIES
        OUTPUT_NAME agentite
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

#============================================================================
# Examples
#============================================================================

if(AGENTITE_BUILD_EXAMPLES)
    # Helper function to create example targets
    function(add_agentite_example name)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/${name}/main.cpp)
            add_executable(example-${name} examples/${name}/main.cpp)
            target_include_directories(example-${name} PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                ${CMAKE_CURRENT_SOURCE_DIR}/lib
                ${CMAKE_CURRENT_SOURCE_DIR}/lib/cglm/include
            )
            target_link_libraries(example-${name} PRIVATE agentite)
            set_target_properties(example-${name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endif()
    endfunction()

    # Build all examples
    add_agentite_example(minimal)
    add_agentite_example(sprites)
    add_agentite_example(animation)
    add_agentite_example(tilemap)
    add_agentite_example(ui)
    add_agentite_example(ui_node)
    add_agentite_example(strategy)
    add_agentite_example(strategy-sim)
    add_agentite_example(msdf)
    add_agentite_example(demo)
    add_agentite_example(pathfinding)
    add_agentite_example(ecs_custom_system)
endif()

#============================================================================
# Tests
#============================================================================

if(AGENTITE_BUILD_TESTS)
    enable_testing()

    # Catch2 library
    add_library(catch2 STATIC lib/catch_amalgamated.cpp)
    target_include_directories(catch2 PUBLIC lib)
    set_target_properties(catch2 PROPERTIES CXX_STANDARD 17)
    if(NOT MSVC)
        target_compile_options(catch2 PRIVATE -w)
    endif()

    # Collect test sources
    file(GLOB_RECURSE AGENTITE_TEST_SOURCES
        tests/core/*.cpp
        tests/strategy/*.cpp
        tests/ai/*.cpp
        tests/graphics/*.cpp
    )

    add_executable(test_runner
        tests/test_main.cpp
        ${AGENTITE_TEST_SOURCES}
    )

    target_include_directories(test_runner PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/cglm/include
    )

    target_link_libraries(test_runner
        PRIVATE
            agentite
            catch2
    )

    set_target_properties(test_runner PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Register with CTest
    add_test(NAME agentite_tests COMMAND test_runner)

    # Verbose test target
    add_custom_target(test-verbose
        COMMAND test_runner --success
        DEPENDS test_runner
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests with verbose output"
    )
endif()

#============================================================================
# Code Quality Targets (Carbide)
#============================================================================

# Collect source files for analysis (excluding third-party)
file(GLOB_RECURSE ANALYZE_SOURCES
    src/*.cpp
    include/agentite/*.h
)

# Static analysis with clang-tidy
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    add_custom_target(check
        COMMAND ${CLANG_TIDY}
            ${ANALYZE_SOURCES}
            --
            -std=c++17
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/lib
            -I${CMAKE_CURRENT_SOURCE_DIR}/lib/cglm/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/src
            ${SDL3_CFLAGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-tidy static analysis"
    )

    add_custom_target(safety
        COMMAND ${CLANG_TIDY}
            ${ANALYZE_SOURCES}
            -checks=-*,clang-analyzer-security.*,bugprone-*,cert-*
            --
            -std=c++17
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/lib
            -I${CMAKE_CURRENT_SOURCE_DIR}/lib/cglm/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/src
            ${SDL3_CFLAGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running security-focused checks"
    )
else()
    message(STATUS "clang-tidy not found - 'check' and 'safety' targets disabled")
endif()

# Code formatting with clang-format
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ANALYZE_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Formatting source files"
    )

    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${ANALYZE_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking code format"
    )
else()
    message(STATUS "clang-format not found - 'format' and 'format-check' targets disabled")
endif()

#============================================================================
# Installation
#============================================================================

include(GNUInstallDirs)

install(TARGETS agentite flecs toml
    EXPORT agentite-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/agentite
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT agentite-targets
    FILE agentite-targets.cmake
    NAMESPACE agentite::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/agentite
)

# Generate package config
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/agentite-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/agentite-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/agentite
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/agentite-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/agentite-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/agentite-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/agentite
)

#============================================================================
# Summary
#============================================================================

message(STATUS "")
message(STATUS "Agentite ${PROJECT_VERSION} Configuration:")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  C compiler:      ${CMAKE_C_COMPILER}")
message(STATUS "  C++ compiler:    ${CMAKE_CXX_COMPILER}")
message(STATUS "  Build examples:  ${AGENTITE_BUILD_EXAMPLES}")
message(STATUS "  Build tests:     ${AGENTITE_BUILD_TESTS}")
message(STATUS "  Build game:      ${AGENTITE_BUILD_GAME}")
message(STATUS "")
