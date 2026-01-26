cmake_minimum_required(VERSION 3.16)
project(minigolf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSceneGraph REQUIRED COMPONENTS osgDB osgUtil osgGA osgViewer osgText)

add_executable(minigolf
  src/main.cpp
  src/Game.cpp
  src/Level.cpp
  src/Physics.cpp
  src/SceneBuilder.cpp
  src/Hud.cpp
  src/Input.cpp
)

target_include_directories(minigolf PRIVATE src ${OPENSCENEGRAPH_INCLUDE_DIRS})
target_link_libraries(minigolf PRIVATE ${OPENSCENEGRAPH_LIBRARIES})
