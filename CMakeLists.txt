project(hsc)
cmake_minimum_required(VERSION 3.8)

aux_source_directory(. DIR_SRCS)

link_directories(${CMAKE_SOURCE_DIR}/deps)
include_directories(${CMAKE_SOURCE_DIR}/deps/include)

add_executable(hsc ${DIR_SRCS})
target_link_libraries(hsc libuv.lib http_parser.lib ws2_32 psapi iphlpapi shell32 userenv user32)
