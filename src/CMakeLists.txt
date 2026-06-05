# Main executable that wires everything together.
# For now, just a stub that builds and links against lowlat.

# add_executable(lowlat_main main.cpp)
# target_link_libraries(lowlat_main PRIVATE lowlat)

#add_executable(lowlat_main2 main2.cpp)
#target_link_libraries(lowlat_main2 PRIVATE lowlat)

add_executable(lowlat_main main.cpp)
target_link_libraries(lowlat_main PRIVATE lowlat)

