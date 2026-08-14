cmake_minimum_required(VERSION 3.18.1)
project("imguimod")

set(CMAKE_CXX_STANDARD 14)

add_library(
    imguimod
    SHARED
    main.cpp
)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(
    imguimod
    ${log-lib}
    ${android-lib}
)
