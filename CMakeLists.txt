cmake_minimum_required(VERSION 3.16)

project(BayerClash LANGUAGES C)

add_executable(clash clash.c exoquant/exoquant.c)
target_include_directories(clash PRIVATE ${PROJECT_SOURCE_DIR}/stb ${PROJECT_SOURCE_DIR}/exoquant)

add_executable(exoclash exoclash.c)
target_include_directories(exoclash PRIVATE ${PROJECT_SOURCE_DIR}/stb)

add_executable(bayer4 bayer4.c)
target_include_directories(bayer4 PRIVATE ${PROJECT_SOURCE_DIR}/stb)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
target_link_libraries(clash)
else()
target_link_libraries(clash m)
endif()
