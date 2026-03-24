cmake_minimum_required(VERSION 3.16)

project(MojoRaw VERSION 0.1.0 LANGUAGES CXX)

# =========================
# C++ Configuration
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =========================
# Compiler Flags
# =========================
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -O3
    )
endif()

# =========================
# Threads (pthreads)
# =========================
find_package(Threads REQUIRED)
find_package(ZLIB)

# =========================
# Core Library
# =========================
file(GLOB_RECURSE CORE_SOURCES
    core/*.cpp
)

add_library(mojoraw_core STATIC ${CORE_SOURCES})

target_include_directories(mojoraw_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Enable GNU extensions globally (_GNU_SOURCE) for accept4, SO_REUSEPORT, etc.
target_compile_definitions(mojoraw_core
    PUBLIC _GNU_SOURCE
)

target_link_libraries(mojoraw_core
    PUBLIC Threads::Threads
)

# =========================
# Example Executable
# =========================
add_executable(mojoraw_example
    examples/basic_server.cpp
)

target_link_libraries(mojoraw_example
    PRIVATE
        mojoraw_core
)

if (ZLIB_FOUND)
    target_link_libraries(mojoraw_example PRIVATE ZLIB::ZLIB)
    target_compile_definitions(mojoraw_example PRIVATE MOJORAW_HAS_ZLIB=1)
endif()

# =========================
# Static Asset Precompression (.gz)
# =========================
option(MOJORAW_PRECOMPRESS_STATIC_GZIP
    "Automatically generate .gz files for assets in src/static during build"
    ON
)

if (MOJORAW_PRECOMPRESS_STATIC_GZIP)
    find_program(MOJORAW_GZIP_EXECUTABLE gzip)

    if (MOJORAW_GZIP_EXECUTABLE)
        add_custom_target(mojoraw_static_gzip
            COMMAND ${CMAKE_COMMAND}
                -DSTATIC_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/static
                -DGZIP_EXECUTABLE:FILEPATH=${MOJORAW_GZIP_EXECUTABLE}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrecompressStatic.cmake
            COMMENT "Generating precompressed .gz static assets"
            VERBATIM
        )

        add_dependencies(mojoraw_example mojoraw_static_gzip)
    else()
        message(WARNING
            "gzip not found; static .gz precompression disabled. "
            "Install gzip or set MOJORAW_PRECOMPRESS_STATIC_GZIP=OFF"
        )
    endif()
endif()
