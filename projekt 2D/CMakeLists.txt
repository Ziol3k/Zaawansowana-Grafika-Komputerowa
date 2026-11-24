cmake_minimum_required(VERSION 3.10)

project(MosaicGenerator)

find_package(OpenCV REQUIRED)

add_executable(mosaic mosaic.cpp)

target_link_libraries(mosaic PRIVATE ${OpenCV_LIBS})