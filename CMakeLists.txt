cmake_minimum_required(VERSION 3.20)
project(lowlat
    VERSION 0.1.0
    DESCRIPTION "Low-latency trading system inspired by David Gross's CppCon 2024 talk"
    LANGUAGES CXX
)

# ----- Global config -----
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ----- Options -----
option(LOWLAT_BUILD_TESTS "Build unit tests" ON)
option(LOWLAT_BUILD_BENCH "Build benchmarks" ON)
option(LOWLAT_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LOWLAT_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(LOWLAT_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# ----- Warnings & flags -----
add_library(lowlat_warnings INTERFACE)
target_compile_options(lowlat_warnings INTERFACE
    -Wall -Wextra -Wpedantic
    -Wshadow -Wnon-virtual-dtor -Wold-style-cast
    -Wcast-align -Wunused -Woverloaded-virtual
    -Wconversion -Wsign-conversion -Wnull-dereference
    -Wdouble-promotion -Wformat=2
)

add_library(lowlat_optimization INTERFACE)
target_compile_options(lowlat_optimization INTERFACE
    $<$<CONFIG:Release>:-O3 -march=native -fno-omit-frame-pointer>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g -march=native -fno-omit-frame-pointer>
)

# ----- Sanitizers (mutually exclusive at runtime) -----
if(LOWLAT_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(LOWLAT_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()
if(LOWLAT_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()

# ----- Header-only library -----
add_library(lowlat INTERFACE)
target_include_directories(lowlat INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(lowlat INTERFACE lowlat_warnings lowlat_optimization)

find_package(absl REQUIRED)
target_link_libraries(lowlat INTERFACE absl::flat_hash_map)


# ----- Subdirectories -----
add_subdirectory(src)

if(LOWLAT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LOWLAT_BUILD_BENCH)
    add_subdirectory(bench)
endif()

# ----- Summary -----
message(STATUS "")
message(STATUS "lowlat configuration:")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Tests:          ${LOWLAT_BUILD_TESTS}")
message(STATUS "  Benchmarks:     ${LOWLAT_BUILD_BENCH}")
message(STATUS "  ASan:           ${LOWLAT_ENABLE_ASAN}")
message(STATUS "  TSan:           ${LOWLAT_ENABLE_TSAN}")
message(STATUS "  UBSan:          ${LOWLAT_ENABLE_UBSAN}")
message(STATUS "")
