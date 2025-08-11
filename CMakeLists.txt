cmake_minimum_required(VERSION 3.5)
project(ProcScope VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
file(GLOB SRC "src/*.cpp" "src/*.cxx" "src/*.cc")

add_executable(procscope
    src/main.cpp
    src/ui.cpp
    src/procscope_reader.cpp
    src/scope_manager.cpp
)

find_package(Curses REQUIRED)
target_link_libraries(procscope PRIVATE ${CURSES_LIBRARIES} pthread)
target_include_directories(procscope PRIVATE ${CURSES_INCLUDE_DIR})
