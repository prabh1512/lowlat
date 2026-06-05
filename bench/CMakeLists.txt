include(FetchContent)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

add_executable(lowlat_bench
    bench_placeholder.cpp
)
target_link_libraries(lowlat_bench PRIVATE
    lowlat
    benchmark::benchmark
    benchmark::benchmark_main
)

add_executable(spsc_throughput spsc_throughput.cpp)
target_link_libraries(spsc_throughput PRIVATE lowlat pthread)

add_executable(spsc_latency spsc_latency.cpp)
target_link_libraries(spsc_latency PRIVATE lowlat pthread)
