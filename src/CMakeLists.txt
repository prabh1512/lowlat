# Main executable that wires everything together.
# For now, just a stub that builds and links against lowlat.

add_executable(lowlat_main main.cpp)
target_link_libraries(lowlat_main PRIVATE lowlat)
