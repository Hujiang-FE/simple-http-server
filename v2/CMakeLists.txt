cmake_minimum_required (VERSION 2.8.11)

project(simple_http_server)

include_directories(${CMAKE_SOURCE_DIR}/include)

link_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(client ${CMAKE_SOURCE_DIR}/worker.c)

add_executable(server ${CMAKE_SOURCE_DIR}/server.c)

target_link_libraries(client uv)

target_link_libraries(server uv)

