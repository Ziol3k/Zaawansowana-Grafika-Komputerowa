cmake_minimum_required(VERSION 3.16)
project(minigolf_osg CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSceneGraph REQUIRED COMPONENTS osg osgDB osgGA osgViewer osgUtil)

add_executable(minigolf
    src/main.cpp
)

target_include_directories(minigolf PRIVATE ${OPENSCENEGRAPH_INCLUDE_DIRS})
target_link_libraries(minigolf PRIVATE ${OPENSCENEGRAPH_LIBRARIES})
