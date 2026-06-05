add_executable(spsc_throughput spsc_throughput.cpp)
target_link_libraries(spsc_throughput PRIVATE lowlat pthread)

add_executable(spsc_latency spsc_latency.cpp)
target_link_libraries(spsc_latency PRIVATE lowlat pthread)
