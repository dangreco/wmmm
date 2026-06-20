cmake_minimum_required(VERSION 3.20)
project(project CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # -> build/compile_commands.json for clangd

# Library: every .cpp under src/ except the entrypoint, auto-discovered.
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
add_library(project_lib ${LIB_SOURCES})
target_include_directories(project_lib PUBLIC include)
target_compile_options(project_lib PRIVATE -Wall -Wextra)

# Executable.
add_executable(project src/main.cpp)
target_link_libraries(project PRIVATE project_lib)
target_compile_options(project PRIVATE -Wall -Wextra)

# Tests: one binary globbing tests/*.cpp; each TEST() becomes its own CTest
# entry via gtest_discover_tests, auto-discovered.
include(CTest)
if(BUILD_TESTING)
  find_package(GTest REQUIRED)
  include(GoogleTest)

  file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
  add_executable(unit_tests ${TEST_SOURCES})
  target_link_libraries(unit_tests PRIVATE project_lib GTest::gtest_main)
  gtest_discover_tests(unit_tests)
endif()
