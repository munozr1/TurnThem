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
if(TURNTHEM_SANITIZE)
    # ENet 1.3.17 forms &peer->channels[0xFF] on a NULL array whenever it queues
    # a disconnect (enet_peer_setup_outgoing_command); never dereferenced, but UBSan flags it
    target_compile_options(enet PRIVATE -fno-sanitize=pointer-overflow)
endif()

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
    core/src/protocol.cpp
    core/src/match_host.cpp
    core/src/playback_clock.cpp
    core/src/match_view.cpp
    core/src/bot.cpp
    core/src/loopback_link.cpp
    core/src/bot_client.cpp
)
target_include_directories(turnthem_core PUBLIC ${PROJECT_SOURCE_DIR}/core/include)
turnthem_warnings(turnthem_core)

# Networking: the ENet client link and the server. enet.h stays inside these
# .cpp files (on Windows it pulls in windows.h, which clashes with raylib), and
# ENet doesn't export its include dir, so it's added here privately.
add_library(turnthem_net STATIC
    net/src/enet_link.cpp
    net/src/server.cpp
)
target_include_directories(turnthem_net PUBLIC ${PROJECT_SOURCE_DIR}/net/include)
target_include_directories(turnthem_net SYSTEM PRIVATE ${enet_SOURCE_DIR}/include)
target_link_libraries(turnthem_net PUBLIC turnthem_core PRIVATE enet)
if(WIN32)
    target_link_libraries(turnthem_net PRIVATE ws2_32 winmm)
endif()
turnthem_warnings(turnthem_net)

# Dedicated server and test bot: no raylib, so they build on a headless box
add_executable(TurnThemServer server/main.cpp)
target_link_libraries(TurnThemServer PRIVATE turnthem_net)
turnthem_warnings(TurnThemServer)

add_executable(TurnThemBot tools/bot_main.cpp)
target_link_libraries(TurnThemBot PRIVATE turnthem_net)
turnthem_warnings(TurnThemBot)

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
        tests/test_protocol.cpp
        tests/test_clock.cpp
        tests/test_view.cpp
        tests/test_net.cpp
    )
    target_include_directories(turnthem_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/third_party)
    # the network tests also poke the server with a raw ENet client
    target_include_directories(turnthem_tests SYSTEM PRIVATE ${enet_SOURCE_DIR}/include)
    target_link_libraries(turnthem_tests PRIVATE turnthem_net enet)
    turnthem_warnings(turnthem_tests)
    add_test(NAME turnthem_tests COMMAND turnthem_tests)
endif()
