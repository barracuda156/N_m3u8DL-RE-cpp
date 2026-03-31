cmake_minimum_required(VERSION 3.20)
project(n_m3u8dl_re VERSION 0.5.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer system libraries over bundled/FetchContent
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG FALSE)
set(CMAKE_PREFER_SYSTEM_LIBRARIES ON)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic -O3)
endif()

# Disable LTO by default to avoid ar wrapper issues
# Enable with -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON if desired
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Enable LTO")

# Use pkg-config for finding libraries
find_package(PkgConfig REQUIRED)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# Try CMake config first, fall back to pkg-config
find_package(pugixml QUIET)
if(NOT pugixml_FOUND)
    pkg_check_modules(pugixml REQUIRED IMPORTED_TARGET pugixml)
    add_library(pugixml::pugixml ALIAS PkgConfig::pugixml)
endif()

find_package(CLI11 QUIET)
if(NOT CLI11_FOUND)
    pkg_check_modules(CLI11 REQUIRED IMPORTED_TARGET CLI11)
    add_library(CLI11::CLI11 ALIAS PkgConfig::CLI11)
endif()

find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    pkg_check_modules(spdlog REQUIRED IMPORTED_TARGET spdlog)
    add_library(spdlog::spdlog ALIAS PkgConfig::spdlog)
endif()

# Add project root to include path for third_party headers
include_directories(${CMAKE_SOURCE_DIR})

# Common library
add_library(n_m3u8dl_common STATIC
    src/common/logger.cpp
    src/common/http_client.cpp
    src/common/crypto.cpp
    src/common/util.cpp
    src/common/types.cpp
    src/common/stream_quality.cpp
)

target_include_directories(n_m3u8dl_common PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(n_m3u8dl_common PUBLIC
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
)

# Parser library
add_library(n_m3u8dl_parser STATIC
    src/parser/stream_extractor.cpp
    src/parser/hls_extractor.cpp
    src/parser/dash_extractor.cpp
)

target_include_directories(n_m3u8dl_parser PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(n_m3u8dl_parser PUBLIC
    n_m3u8dl_common
    pugixml::pugixml
)

# Downloader library
add_library(n_m3u8dl_downloader STATIC
    src/downloader/simple_downloader.cpp
    src/downloader/download_manager.cpp
)

target_include_directories(n_m3u8dl_downloader PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(n_m3u8dl_downloader PUBLIC
    n_m3u8dl_common
    n_m3u8dl_parser
)

# Main executable
add_executable(N_m3u8DL-RE
    src/cli/main.cpp
    src/cli/command_line.cpp
)

target_link_libraries(N_m3u8DL-RE PRIVATE
    n_m3u8dl_downloader
    CLI11::CLI11
)

# Installation
install(TARGETS N_m3u8DL-RE DESTINATION bin)
