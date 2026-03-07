cmake_minimum_required(VERSION 3.14)
project(ascii3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Allow older CMakeLists in dependencies
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

# ---- Dependencies via FetchContent ----
include(FetchContent)

# libdatachannel (WebRTC DataChannels for P2P)
FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG v0.21.2
)
# Build as static, disable media/websocket features we don't need
set(NO_MEDIA ON CACHE BOOL "" FORCE)
set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libdatachannel)

# nlohmann/json is bundled with libdatachannel, no separate fetch needed

# libcurl (HTTP client - macOS has it built-in)
find_package(CURL REQUIRED)

# ---- Main executable ----
add_executable(ascii3d
    # Entry point
    src/main.cpp

    # Core (header-only: vec_math.h, noise.h, chunk.h)

    # Rendering
    src/rendering/framebuffer.cpp
    src/rendering/mesh.cpp
    src/rendering/renderer.cpp

    # World
    src/world/world.cpp
    src/world/terrain.cpp
    src/world/vegetation.cpp
    src/world/structures.cpp

    # Game
    src/game/player.cpp
    src/game/camera.cpp
    src/game/save_system.cpp

    # Input
    src/input/input.cpp

    # UI (menu screens)
    src/ui/menu/menu_common.cpp
    src/ui/menu/mode_select.cpp
    src/ui/menu/main_menu.cpp
    src/ui/menu/load_world.cpp
    src/ui/menu/username.cpp
    src/ui/menu/lobby.cpp
    src/ui/menu/host_setup.cpp
    src/ui/menu/join_password.cpp
    src/ui/menu/connecting.cpp
    src/ui/menu/generating.cpp
    src/ui/menu/pause_overlay.cpp

    # Network
    src/network/firebase_client.cpp
    src/network/signaling.cpp
    src/network/peer_connection.cpp
    src/network/remote_player.cpp
    src/network/host_session.cpp
    src/network/client_session.cpp
    src/network/lobby.cpp
)

target_include_directories(ascii3d PRIVATE src)

# Link macOS frameworks needed for input (CGEventTap, mouse capture)
find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
find_library(APPLICATIONSERVICES_FRAMEWORK ApplicationServices)
target_link_libraries(ascii3d PRIVATE
    ${COREGRAPHICS_FRAMEWORK}
    ${COREFOUNDATION_FRAMEWORK}
    ${APPLICATIONSERVICES_FRAMEWORK}
    LibDataChannel::LibDataChannelStatic
    nlohmann_json
    CURL::libcurl
)

# Default to Release if no build type specified (critical for performance —
# the software rasterizer is dramatically slower without optimization)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

target_compile_options(ascii3d PRIVATE -O2)

# Read Firebase URL from .env file at configure time and bake it into the binary
if(EXISTS "${CMAKE_SOURCE_DIR}/.env")
    file(STRINGS "${CMAKE_SOURCE_DIR}/.env" ENV_LINES)
    foreach(LINE ${ENV_LINES})
        if(LINE MATCHES "^FIREBASE_URL=(.+)$")
            set(FIREBASE_URL "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endif()
if(NOT FIREBASE_URL)
    message(FATAL_ERROR "FIREBASE_URL not found in .env file. Create a .env file with FIREBASE_URL=https://your-project.firebaseio.com/")
endif()
target_compile_definitions(ascii3d PRIVATE FIREBASE_URL="${FIREBASE_URL}")
