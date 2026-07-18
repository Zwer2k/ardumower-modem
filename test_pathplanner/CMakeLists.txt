cmake_minimum_required(VERSION 3.15)
project(pathplanner_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CLIPPER2_ROOT ${CMAKE_SOURCE_DIR}/../lib/Clipper2/CPP)

add_library(Clipper2 STATIC
  ${CLIPPER2_ROOT}/Clipper2Lib/src/clipper.engine.cpp
  ${CLIPPER2_ROOT}/Clipper2Lib/src/clipper.offset.cpp
  ${CLIPPER2_ROOT}/Clipper2Lib/src/clipper.rectclip.cpp
)
target_include_directories(Clipper2 PUBLIC ${CLIPPER2_ROOT}/Clipper2Lib/include)

add_library(pathplanner STATIC
  ../lib/pathplanner/pathplanner.cpp
)
target_include_directories(pathplanner PUBLIC ../lib/pathplanner)
target_link_libraries(pathplanner PUBLIC Clipper2)

add_executable(test_exclusion
  test_exclusion.cpp
)
target_link_libraries(test_exclusion PRIVATE pathplanner)

add_executable(test_runner
  test_main.cpp
  path_planner_test.cpp
)
# Existing test_runner still uses its own reimplementation; keep separate for now.
