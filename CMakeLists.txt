cmake_minimum_required(VERSION 3.16)
project(TurnThem VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(TURNTHEM_BUILD_CLIENT "Build the raylib game client (off for a headless server box)" ON)
option(TURNTHEM_BUILD_TESTS "Build the unit tests" ON)
option(TURNTHEM_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(TURNTHEM_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ENet (and raylib's fallback) declare cmake_minimum_required < 3.5, which CMake 4 rejects
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
include(FetchContent)

# ENet for networking
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG v1.3.17
)
FetchContent_MakeAvailable(enet)

function(turnthem_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Simulation, protocol and client-side match state: plain C++, no raylib or enet,
# so the server builds on a headless box
add_library(turnthem_core STATIC
    core/src/grid.cpp
    core/src/collision.cpp
    core/src/weapons.cpp
    core/src/match.cpp
)
target_include_directories(turnthem_core PUBLIC ${PROJECT_SOURCE_DIR}/core/include)
turnthem_warnings(turnthem_core)

# Game client
if(TURNTHEM_BUILD_CLIENT)
    find_package(nlohmann_json REQUIRED)

    find_package(raylib QUIET)
    if (NOT raylib_FOUND)
        FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 5.5
        )
        FetchContent_MakeAvailable(raylib)
    endif()

    add_executable(TurnThem
        src/main.cpp
        src/HomeScreen.cpp
        src/cannon.cpp
        src/collision.cpp
        src/deck.cpp
        src/mouse_handler.cpp
        src/projectile.cpp
        src/scannon.cpp
        src/sprites.cpp
        src/weapon.cpp
        src/weapon_card.cpp
    )
    target_include_directories(TurnThem PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(TurnThem PRIVATE raylib nlohmann_json::nlohmann_json)

    # assets are loaded relative to the working directory, so keep a copy next to the binary
    add_custom_command(TARGET TurnThem POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${PROJECT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:TurnThem>/assets)
endif()

# Tests
if(TURNTHEM_BUILD_TESTS)
    enable_testing()
    add_executable(turnthem_tests
        tests/main.cpp
        tests/test_grid.cpp
        tests/test_collision.cpp
        tests/test_weapons.cpp
        tests/test_match.cpp
    )
    target_include_directories(turnthem_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/third_party)
    target_link_libraries(turnthem_tests PRIVATE turnthem_core)
    turnthem_warnings(turnthem_tests)
    add_test(NAME turnthem_tests COMMAND turnthem_tests)
endif()
